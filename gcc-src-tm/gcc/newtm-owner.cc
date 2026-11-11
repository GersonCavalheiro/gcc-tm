/* Prove admission, exclusive slot ownership, commit, teardown and full retry.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "fold-const.h"
#include "gimple.h"
#include "gimple-pretty-print.h"
#include "options.h"
#include "newtm-abi.h"
#include "newtm-flow.h"
#include "newtm-owner.h"

namespace {
enum bits { ZERO = 1, ONE = 2, TWO = 4, OTHER = 8, ANY = 15 };
enum tag { NONE, CONTEXT, STALE, SLOT_ADDRESS, CONTROL_ADDRESS, OUTER, OUTER_TEST, CAUSE, EFFECTIVE,
           CLEANUP_STATUS, COUNTER };
enum phase { INITIAL, ENTRY, BODY, CHECKED, COMMITTED,
             FAILED_C, FAILED_I, FAILED_U, EMPTY_C, EMPTY_I, EMPTY_U,
             VALIDATION, CONDEMNED_C, CONDEMNED_I, CLEANED_C, CLEANED_I,
             CLEANED_U, PHASES };
enum operation { PURE, BEGIN, HELPER, STATUS, END, FAIL, AFTER, FATAL, BAD };
struct value { unsigned bits = ANY, tag = NONE; bool fresh = false; };
struct state
{
  unsigned phase = INITIAL;
  bool outer = false, counter = false, below = false, maximum = false;
  bool bumped = false, ready = false, retry = false;
  auto_vec<value> values;
  auto_vec<unsigned char> fields;
  state (unsigned n, unsigned m)
  { for (unsigned i = 0; i < n; ++i) values.safe_push (value ()); fields.safe_grow_cleared (m); }
  void copy (const state &s)
  {
    phase = s.phase; outer = s.outer; counter = s.counter;
    below = s.below; maximum = s.maximum; bumped = s.bumped; ready = s.ready; retry = s.retry;
    for (unsigned i = 0; i < values.length (); ++i) values[i] = s.values[i];
    for (unsigned i = 0; i < fields.length (); ++i) fields[i] = s.fields[i];
  }
};
struct location { tree expression, root; };
struct arc { unsigned target, next; };
static tree destination (gimple *g)
{ return g && is_gimple_assign (g) ? gimple_assign_lhs (g)
    : g && is_gimple_call (g) ? gimple_call_lhs (g) : NULL_TREE; }
static bool variable (tree t)
{ return t && (VAR_P (t) || TREE_CODE (t) == PARM_DECL || TREE_CODE (t) == SSA_NAME); }
static tree addressed (tree t)
{ return t && TREE_CODE (t) == ADDR_EXPR ? get_base_address (TREE_OPERAND (t, 0)) : NULL_TREE; }
static bool local (tree t)
{ return t && VAR_P (t) && !TREE_STATIC (t) && !DECL_EXTERNAL (t); }
static unsigned constant (tree t)
{ return integer_zerop (t) ? ZERO : integer_onep (t) ? ONE
    : tree_fits_uhwi_p (t) && tree_to_uhwi (t) == 2 ? TWO : OTHER; }
static bool failed (unsigned p) { return p >= FAILED_C && p <= VALIDATION; }
static bool empty_failure (unsigned p) { return p >= EMPTY_C && p <= EMPTY_U; }
static bool condemned (unsigned p) { return p == CONDEMNED_C || p == CONDEMNED_I; }
static bool cleaned (unsigned p) { return p >= CLEANED_C && p <= CLEANED_U; }
static bool tainted (unsigned t)
{ return t == CONTEXT || t == STALE || t == SLOT_ADDRESS || t == CONTROL_ADDRESS; }
}

struct newtm_owner_builder
{
  const newtm_flow_graph &flow;
  tree helper, slot = NULL_TREE, outer = NULL_TREE, counter = NULL_TREE;
  gimple *admission = NULL;
  unsigned storage_scope = 0;
  hash_map<tree, unsigned> indices;
  auto_vec<tree> variables;
  auto_vec<location> locations;
  hash_set<gimple *> invalid;
  hash_set<gimple *> invalid_true;
  hash_set<tree> protected_values;
  bool current;
  const char *reason = NULL; gimple *bad = NULL;

  operation classify (gimple *g)
  {
    if (!g || !is_gimple_call (g)) return PURE;
    if (gimple_call_internal_p (g)) return PURE;
    tree fn = gimple_call_fndecl (g);
    if (fn == helper && helper) return HELPER;
    if (!fn || !fndecl_built_in_p (fn, BUILT_IN_NORMAL)) return BAD;
    switch (DECL_FUNCTION_CODE (fn))
      {
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN: return BEGIN;
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS: return STATUS;
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_END: return END;
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL: return FAIL;
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE: return AFTER;
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL: return FATAL;
      default: return BAD;
      }
  }
  bool refuse (gimple *g, const char *why)
  { if (!reason) { reason = why; bad = g; } return false; }
  void index (tree t)
  { if (variable (t) && !indices.get (t)) { indices.put (t, variables.length ()); variables.safe_push (t); } }
  newtm_owner_builder (const newtm_flow_graph &f, tree h, bool c)
    : flow (f), helper (h), current (c)
  {
    for (unsigned n = 0; n < flow.node_count (); ++n)
      {
        gimple *g = flow.node (n).statement; tree t = destination (g);
        index (t);
        if (g && is_gimple_assign (g) && !gimple_clobber_p (g) && t && REFERENCE_CLASS_P (t))
          {
            bool found = false;
            for (const location &l : locations) found |= operand_equal_p (t, l.expression, 0);
            if (!found) { location l = { t, get_base_address (t) }; locations.safe_push (l); }
          }
        if (classify (g) == BEGIN)
          {
            if (admission && admission != g) refuse (g, "V07: controller has multiple admission operations");
            admission = g;
            if (gimple_call_num_args (g) != 2) { refuse (g, "V07: admission lacks slot and outermost"); continue; }
            slot = addressed (gimple_call_arg (g, 0)); outer = addressed (gimple_call_arg (g, 1));
          }
        if (classify (g) == AFTER)
          {
            tree c = gimple_call_num_args (g) == 2 ? gimple_call_arg (g, 1) : NULL_TREE;
            if (counter && c != counter) refuse (g, "V07: failure cleanup changes its conflict counter");
            counter = c;
          }
      }
    if (!helper || !admission || !local (slot) || !local (outer) || !local (counter)
        || slot == outer || slot == counter || outer == counter
        || TYPE_MAIN_VARIANT (TREE_TYPE (slot)) != TYPE_MAIN_VARIANT (newtm_abi_context_type ())
        || TYPE_MAIN_VARIANT (TREE_TYPE (outer)) != TYPE_MAIN_VARIANT (uint32_type_node)
        || TYPE_MAIN_VARIANT (TREE_TYPE (counter)) != TYPE_MAIN_VARIANT (size_type_node))
      refuse (admission, "V07: controller lacks a unique typed local slot, outermost, counter or delegated helper");
    index (slot); index (outer); index (counter);
    if (current && !reason
        && (DECL_CONTEXT (slot) != DECL_CONTEXT (outer) || DECL_CONTEXT (slot) != DECL_CONTEXT (counter)))
      refuse (admission, "V08: owning control carriers belong to different current frames");
    unsigned outer_scope = 0, counter_scope = 0;
    for (unsigned i = 1; i < flow.scope_count (); ++i)
      if (flow.scope (i).kind == NEWTM_FLOW_BIND)
        for (tree d = gimple_bind_vars (as_a<gbind *> (flow.scope (i).owner)); d; d = DECL_CHAIN (d))
          {
            if (d == slot) storage_scope = i;
            if (d == outer) outer_scope = i;
            if (d == counter) counter_scope = i;
          }
    if (storage_scope != outer_scope || storage_scope != counter_scope)
      refuse (admission, "V07: owning slot and control carriers have incompatible lexical lifetimes");
  }
  bool inside (unsigned scope)
  { for (;; scope = flow.scope (scope).parent) { if (scope == storage_scope) return true; if (!scope) return false; } }
  value address_operands (tree t, const state &s, unsigned depth)
  {
    value v; v.fresh = true;
    if (!t || depth > 32) { v.fresh = false; return v; }
    if (DECL_P (t) || CONSTANT_CLASS_P (t)) return v;
    if (!REFERENCE_CLASS_P (t)) { v.fresh = false; return v; }
    for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
      {
        tree part = TREE_OPERAND (t, i);
        if (!part || TREE_CODE (part) == FIELD_DECL || TYPE_P (part)) continue;
        /* Addressing an object does not read its value, but a subscript,
           offset or dereferenced pointer is a real value consumption. */
        value a = i == 0 && TREE_CODE (t) != MEM_REF && TREE_CODE (t) != INDIRECT_REF
          ? address_operands (part, s, depth + 1) : get (part, s, depth + 1);
        v.fresh &= a.fresh; if (tainted (a.tag)) v.tag = STALE;
      }
    return v;
  }
  value get (tree t, const state &s, unsigned depth = 0)
  {
    value v;
    if (!t || depth > 32) return v;
    if (TREE_CODE (t) == INTEGER_CST) { v.bits = constant (t); v.fresh = true; return v; }
    if (unsigned *i = indices.get (t)) return s.values[*i];
    if (TREE_CODE (t) == ADDR_EXPR)
      {
        v.fresh = true; tree root = addressed (t);
        if (root == slot) v.tag = SLOT_ADDRESS;
        else if (root == outer || root == counter) v.tag = CONTROL_ADDRESS;
        else if (variable (root) && TYPE_MAIN_VARIANT (TREE_TYPE (root)) == TYPE_MAIN_VARIANT (newtm_abi_context_type ()))
          v.tag = STALE;
        if (current)
          {
            value parts = address_operands (TREE_OPERAND (t, 0), s, depth + 1);
            v.fresh &= parts.fresh; if (tainted (parts.tag)) v.tag = STALE;
          }
        return v;
      }
    if (CONVERT_EXPR_CODE_P (TREE_CODE (t)))
      {
        tree from = TREE_OPERAND (t, 0);
        v = get (from, s, depth + 1);
        if (TYPE_PRECISION (TREE_TYPE (t)) >= TYPE_PRECISION (TREE_TYPE (from))) return v;
        v.bits = ANY; if (!tainted (v.tag)) v.tag = NONE;
        return v;
      }
    v.fresh = true;
    if (TREE_CODE (t) == CONSTRUCTOR)
      {
        unsigned HOST_WIDE_INT i; tree part;
        FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (t), i, part)
          { value a = get (part, s, depth + 1); v.fresh &= a.fresh;
            if (tainted (a.tag)) v.tag = a.tag; }
      }
    else if (EXPR_P (t))
      for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
        {
          tree part = TREE_OPERAND (t, i); if (!part || TYPE_P (part)) continue;
          value a = get (part, s, depth + 1); v.fresh &= a.fresh;
          if (tainted (a.tag)) v.tag = a.tag;
        }
    return v;
  }
  value comparison (tree_code code, value a, value b)
  {
    value v; v.bits = 0; v.fresh = a.fresh && b.fresh;
    for (unsigned i = 0; i < 4; ++i) for (unsigned j = 0; j < 4; ++j)
      if ((a.bits & (1u << i)) && (b.bits & (1u << j)))
        v.bits |= i == 3 && j == 3 ? ZERO | ONE : ((i == j) == (code == EQ_EXPR)) ? ONE : ZERO;
    if (code == EQ_EXPR && ((a.tag == OUTER && b.bits == ONE && b.tag == NONE)
                            || (b.tag == OUTER && a.bits == ONE && a.tag == NONE))) v.tag = OUTER_TEST;
    return v;
  }
  value assigned (gimple *g, const state &s)
  {
    if (gimple_clobber_p (g)) return value ();
    if (gimple_assign_single_p (g)) return get (gimple_assign_rhs1 (g), s);
    if (gimple_assign_cast_p (g))
      {
        value v = get (gimple_assign_rhs1 (g), s);
        if (TYPE_PRECISION (TREE_TYPE (destination (g))) >= TYPE_PRECISION (TREE_TYPE (gimple_assign_rhs1 (g)))) return v;
        v.bits = ANY; if (!tainted (v.tag)) v.tag = NONE;
        return v;
      }
    tree_code c = gimple_assign_rhs_code (g);
    if (c == EQ_EXPR || c == NE_EXPR) return comparison (c, get (gimple_assign_rhs1 (g), s), get (gimple_assign_rhs2 (g), s));
    value v; v.fresh = true;
    for (unsigned i = 1; i < gimple_num_ops (g); ++i)
      { value a = get (gimple_op (g, i), s); v.fresh &= a.fresh;
        if (tainted (a.tag)) v.tag = a.tag; }
    return v;
  }
  void put (tree t, value v, state &s)
  {
    if (tainted (v.tag) && variable (t)) protected_values.add (t);
    if (unsigned *i = indices.get (t)) s.values[*i] = v;
  }
  void stale (state &s)
  { for (value &v : s.values) if (v.tag == CONTEXT) { v.tag = STALE; v.fresh = false; } }
  void failure (state &s, unsigned p)
  {
    s.phase = p; s.bumped = s.ready = s.below = s.maximum = s.retry = false;
    for (value &v : s.values) if (v.tag == CAUSE || v.tag == EFFECTIVE || v.tag == CLEANUP_STATUS) v.tag = NONE;
  }
  bool counter_increment (gimple *g, const state &s)
  { return condemned (s.phase) && !s.bumped && s.below
      && gimple_assign_rhs_code (g) == PLUS_EXPR && gimple_assign_rhs1 (g) == counter
      && integer_onep (gimple_assign_rhs2 (g)); }
  bool inspect (unsigned n, const state &s)
  {
    const newtm_flow_node &node = flow.node (n); gimple *g = node.statement;
    if (node.kind == NEWTM_FLOW_SCOPE_EXIT && node.scope == storage_scope && s.phase != COMMITTED)
      return refuse (node.origin, "V07: owning storage lifetime ends before completing the controller protocol");
    if (node.kind == NEWTM_FLOW_NORMAL_EXIT)
      return s.phase == COMMITTED ? true : refuse (g, "V07: controller continues without consuming a successful commit");
    if (node.kind == NEWTM_FLOW_RETURN_EXIT)
      return refuse (node.origin, "V07: controller returns instead of reaching its normal continuation");
    if (node.kind == NEWTM_FLOW_TERMINATE_EXIT)
      return classify (node.origin) == FATAL ? true : refuse (node.origin, "V07: controller terminates outside its admitted fatal boundary");
    if (!g) return true;
    if ((classify (g) == BEGIN || classify (g) == END || classify (g) == AFTER
         || destination (g) == slot || destination (g) == outer || destination (g) == counter)
        && !inside (node.scope))
      return refuse (g, "V07: controller uses owning storage outside its lexical lifetime");
    if (is_gimple_assign (g) && gimple_clobber_p (g) && s.phase != INITIAL && s.phase != COMMITTED
        && (destination (g) == slot || destination (g) == counter || destination (g) == outer))
      return refuse (g, "V07: clobber ends or restarts live controller storage");
    if (is_gimple_call (g) && (destination (g) == slot || destination (g) == counter || destination (g) == outer))
      return refuse (g, "V07: call result aliases protected controller storage");
    if (gimple_code (g) == GIMPLE_COND)
      {
        gcond *c = as_a<gcond *> (g);
        value a = get (gimple_cond_lhs (c), s), b = get (gimple_cond_rhs (c), s);
        if (!a.fresh || !b.fresh || a.tag == STALE || b.tag == STALE
            || a.tag == SLOT_ADDRESS || b.tag == SLOT_ADDRESS
            || a.tag == CONTROL_ADDRESS || b.tag == CONTROL_ADDRESS)
          return refuse (g, "V07: controller condition consumes stale attempt data or an escaped slot address");
      }
    operation op = classify (g);
    if (op == BAD) return refuse (g, "V07: controller invokes an operation outside its owning protocol");
    if (current && op == PURE && is_gimple_call (g))
      for (unsigned i = 0; i < gimple_call_num_args (g); ++i)
        {
          value v = get (gimple_call_arg (g, i), s);
          if (!v.fresh || tainted (v.tag))
            return refuse (g, "V08: internal computation consumes stale data or protected controller identity");
        }
    if (op != PURE)
      {
        unsigned count = gimple_call_num_args (g);
        bool owning = op == BEGIN || op == END || op == AFTER;
        if (!count) return refuse (g, "V07: controller service lacks its explicit argument");
        if (owning && (TREE_CODE (gimple_call_arg (g, 0)) != ADDR_EXPR || addressed (gimple_call_arg (g, 0)) != slot))
          return refuse (g, "V07: controller service consumes a different or copied owning slot");
        if (op == BEGIN)
          {
            if (!(s.phase == INITIAL || (s.phase == CLEANED_C && s.retry))
                || get (slot, s).bits != ZERO || !s.counter || !get (counter, s).fresh)
              return refuse (g, "V07: admission does not start with an empty slot and an authorized complete retry");
            if (addressed (gimple_call_arg (g, 1)) != outer)
              return refuse (g, "V07: admission changes its outermost carrier");
          }
        if (op == HELPER || op == STATUS || op == END)
          {
            unsigned expected = op == HELPER ? ENTRY : op == STATUS ? BODY : CHECKED;
            if (s.phase != expected || !s.outer)
              return refuse (g, "V07: body/status/commit is out of order or lacks proven outermost admission");
          }
        if (op == HELPER || op == STATUS || op == FAIL)
          if (get (gimple_call_arg (g, 0), s).tag != CONTEXT)
            return refuse (g, "V07: controller uses a foreign or consumed context");
        if (op == HELPER)
          {
            if (count < 2) return refuse (g, "V07: delegated helper lacks its incoming channel");
            tree incoming = gimple_call_arg (g, 1), root = addressed (incoming);
            if (!integer_zerop (incoming) && !local (root))
              return refuse (g, "V07: controller incoming is not an identifiable private object");
            if (root && !(indices.get (root) && get (root, s).fresh))
              {
                bool found = false;
                for (unsigned i = 0; i < locations.length (); ++i)
                  if (locations[i].root == root)
                    {
                      found = true;
                      if (!s.fields[i]) return refuse (g, "V07: retry reuses incoming fields from an earlier attempt");
                    }
                if (!found) return refuse (g, "V07: incoming has no fresh preparation in this attempt");
              }
          }
        if (op == FAIL)
          {
            if ((!failed (s.phase) || empty_failure (s.phase)) || count != 2)
              return refuse (g, "V07: condemnation lacks an owned failed attempt or is duplicated");
            value v = get (gimple_call_arg (g, 1), s);
            unsigned required = s.phase == FAILED_C ? ONE : s.phase == FAILED_I ? TWO : OTHER;
            if (s.phase == VALIDATION ? (v.bits & ONE) != 0 : v.tag != CAUSE || v.bits != required)
              return refuse (g, "V07: controller condemnation loses or substitutes its originating failure");
          }
        if (op == AFTER)
          if (!condemned (s.phase) || count != 2 || gimple_call_arg (g, 1) != counter
              || !s.ready)
            return refuse (g, "V07: cleanup lacks unique condemnation or a positive saturating conflict count");
        if (op == FATAL)
          {
            value v = get (gimple_call_arg (g, 0), s);
            if (current && s.phase == CLEANED_C)
              return refuse (g, "V08: cleaned conflict terminates instead of retrying the complete block");
            if (!(empty_failure (s.phase) || cleaned (s.phase)) || (v.bits & ZERO)
                || (v.tag != CAUSE && v.tag != CLEANUP_STATUS))
              return refuse (g, "V07: fatal bypasses owned cleanup or loses the effective diagnostic status");
          }
        else
          {
            tree t = destination (g);
            if (!variable (t) || TREE_TYPE (t) != uint32_type_node)
              return refuse (g, "V07: controller operation lacks an explicit uint32 status");
          }
        for (unsigned i = 0; i < count; ++i)
          {
            value v = get (gimple_call_arg (g, i), s);
            if (!v.fresh) return refuse (g, "V07: controller operation uses stale attempt data");
            if (v.tag == STALE || (v.tag == SLOT_ADDRESS && !(owning && i == 0))
                || (v.tag == CONTROL_ADDRESS && !(op == BEGIN && i == 1))
                || (v.tag == CONTEXT && !(i == 0 && (op == HELPER || op == STATUS || op == FAIL))))
              return refuse (g, "V07: owning slot or context escapes through a data channel");
          }
      }
    if (is_gimple_assign (g) && !gimple_clobber_p (g))
      {
        tree t = destination (g); value v = assigned (g, s);
        if (current && variable (t) && !local (t))
          return refuse (g, "V08: controller writes nonlocal storage before its continuation");
        if (t == slot && !(s.phase == INITIAL && integer_zerop (gimple_assign_rhs1 (g)) && gimple_assign_single_p (g)))
          return refuse (g, "V07: program assignment overwrites or fabricates the owning slot");
        if (t == counter && !(s.phase == INITIAL && gimple_assign_single_p (g) && integer_zerop (gimple_assign_rhs1 (g)))
            && !counter_increment (g, s))
          return refuse (g, "V07: conflict count resets, wraps, increments twice or lacks its bound");
        if (v.tag == SLOT_ADDRESS || v.tag == CONTROL_ADDRESS || v.tag == STALE)
          return refuse (g, "V07: owning slot address escapes or consumed context is reused");
        if (v.tag == CONTEXT && (!variable (t) || !is_gimple_reg (t)
            || (!gimple_assign_single_p (g) && !gimple_assign_cast_p (g))))
          return refuse (g, "V07: borrowed controller context is published or used as data");
        if (!v.fresh)
          return refuse (g, "V07: attempt preparation depends on a prior or unavailable definition");
        if (t && REFERENCE_CLASS_P (t) && s.phase != ENTRY)
          return refuse (g, "V07: controller writes memory outside fresh attempt preparation");
        if (t && REFERENCE_CLASS_P (t) && tainted (get (t, s).tag))
          return refuse (g, "V07: indirect assignment changes protected controller storage");
      }
    if (invalid.contains (g) && s.phase != ENTRY)
      return refuse (g, "V07: controller validates outside admitted attempt preparation");
    return true;
  }
  void transfer (unsigned n, const state &in, state &out, unsigned outcome, bool live)
  {
    out.copy (in); gimple *g = flow.node (n).statement; if (!g) return;
    operation op = classify (g);
    if (is_gimple_call (g) && op != PURE)
      {
        value v; v.bits = outcome; v.fresh = true;
        if (op == BEGIN)
          {
            value count = get (counter, in);
            for (value &a : out.values) a = value ();
            for (unsigned char &a : out.fields) a = 0;
            put (counter, count, out); out.outer = out.retry = out.bumped = out.ready = out.below = out.maximum = false;
            value owned; owned.bits = live ? OTHER : ZERO; owned.tag = live ? CONTEXT : NONE; owned.fresh = true;
            put (slot, owned, out);
            value o; o.tag = OUTER; o.fresh = true; put (outer, o, out);
            if (outcome == ZERO) out.phase = ENTRY;
            else { failure (out, live ? outcome == ONE ? FAILED_C : outcome == TWO ? FAILED_I : FAILED_U
                               : outcome == ONE ? EMPTY_C : outcome == TWO ? EMPTY_I : EMPTY_U); v.tag = CAUSE; }
          }
        else if (op == HELPER || op == STATUS || op == END)
          {
            if (outcome != ZERO)
              { failure (out, outcome == ONE ? FAILED_C : outcome == TWO ? FAILED_I : FAILED_U); v.tag = CAUSE; }
            else if (op == HELPER) out.phase = BODY;
            else if (op == STATUS) out.phase = CHECKED;
            else
              { out.phase = COMMITTED; stale (out); value empty; empty.bits = ZERO; empty.fresh = true; put (slot, empty, out); }
          }
        else if (op == FAIL)
          { out.phase = outcome == ONE ? CONDEMNED_C : CONDEMNED_I; v.tag = EFFECTIVE; }
        else if (op == AFTER)
          {
            out.phase = outcome == ONE ? CLEANED_C : outcome == TWO ? CLEANED_I : CLEANED_U;
            out.retry = false; stale (out); value empty; empty.bits = ZERO; empty.fresh = true; put (slot, empty, out);
            v.tag = CLEANUP_STATUS;
          }
        put (destination (g), v, out);
      }
    else if (is_gimple_assign (g))
      {
        tree t = destination (g); value v = assigned (g, in);
        if (t == counter && !gimple_clobber_p (g))
          {
            v.tag = COUNTER; out.counter = true;
            if (in.phase != INITIAL)
              { v.bits = ONE | TWO | OTHER; out.bumped = out.ready = true; out.below = out.maximum = false;
                for (value &a : out.values) if (a.tag == COUNTER) a.tag = NONE; }
          }
        put (t, v, out);
        for (unsigned i = 0; i < locations.length (); ++i)
          if (operand_equal_p (locations[i].expression, t, 0) || locations[i].root == t)
            out.fields[i] = v.fresh;
      }
    else if (is_gimple_call (g))
      {
        value v; v.fresh = true;
        for (unsigned i = 0; i < gimple_call_num_args (g); ++i) v.fresh &= get (gimple_call_arg (g, i), in).fresh;
        put (destination (g), v, out);
      }
  }
  bool branch (gimple *g, const newtm_flow_edge &e, state &s)
  {
    if (!g || gimple_code (g) != GIMPLE_COND || (e.kind != NEWTM_FLOW_TRUE && e.kind != NEWTM_FLOW_FALSE)) return true;
    gcond *cond = as_a<gcond *> (g);
    tree a = gimple_cond_lhs (cond), b = gimple_cond_rhs (cond); tree_code c = gimple_cond_code (cond);
    bool truth = e.kind == NEWTM_FLOW_TRUE;
    value av = get (a, s), bv = get (b, s);
    if (c == EQ_EXPR || c == NE_EXPR)
      {
        value v = comparison (c, av, bv);
        if (!(v.bits & (truth ? ONE : ZERO))) return false;
        bool equal = truth == (c == EQ_EXPR);
        if (TREE_CODE (a) == INTEGER_CST) { std::swap (a, b); std::swap (av, bv); }
        if (TREE_CODE (b) == INTEGER_CST && constant (b) != OTHER)
          {
            unsigned bits = constant (b);
            if (unsigned *i = indices.get (a)) s.values[*i].bits &= equal ? bits : ANY ^ bits;
            if (equal && bits == ONE && av.tag == OUTER && s.phase == ENTRY) s.outer = true;
            if (av.tag == OUTER_TEST && s.phase == ENTRY
                && ((equal && bits == ONE) || (!equal && bits == ZERO))) s.outer = true;
            if (equal && bits == ONE && av.tag == CLEANUP_STATUS && s.phase == CLEANED_C) s.retry = true;
          }
      }
    if ((c == LT_EXPR || c == NE_EXPR || c == EQ_EXPR)
        && a == counter && TREE_CODE (b) == INTEGER_CST && tree_int_cst_equal (b, TYPE_MAX_VALUE (size_type_node)))
      {
        bool below = c == EQ_EXPR ? !truth : truth;
        s.below = below; s.maximum = !below;
        if (!below && condemned (s.phase)) s.ready = true;
      }
    if (invalid.contains (g) && truth == invalid_true.contains (g)) failure (s, VALIDATION);
    return true;
  }
  bool merge (state &a, const state &b)
  {
    bool changed = false;
#define MEET(F) do { bool v = a.F && b.F; changed |= a.F != v; a.F = v; } while (0)
    MEET (outer); MEET (counter); MEET (below); MEET (maximum); MEET (ready); MEET (retry);
#undef MEET
    changed |= a.bumped != (a.bumped || b.bumped); a.bumped |= b.bumped;
    for (unsigned i = 0; i < a.values.length (); ++i)
      {
        value &v = a.values[i], w = b.values[i];
        unsigned bits = v.bits | w.bits, tag = v.tag == w.tag ? v.tag : unsigned (NONE);
        bool fresh = v.fresh && w.fresh;
        /* Never erase a possible consumed/live context escape at a join. */
        if (v.tag != w.tag && (v.tag == STALE || w.tag == STALE)) tag = STALE;
        else if (v.tag != w.tag && (tainted (v.tag) || tainted (w.tag)))
          tag = STALE;
        changed |= v.bits != bits || v.tag != tag || v.fresh != fresh;
        v.bits = bits; v.tag = tag; v.fresh = fresh;
      }
    for (unsigned i = 0; i < a.fields.length (); ++i)
      { unsigned char f = a.fields[i] && b.fields[i]; changed |= a.fields[i] != f; a.fields[i] = f; }
    return changed;
  }
  template<class Visit> void successors (unsigned n, const state &in, Visit visit)
  {
    gimple *g = flow.node (n).statement; operation op = classify (g);
    unsigned outcomes = ZERO;
    if (op == BEGIN || op == STATUS || op == END) outcomes = ZERO | ONE | TWO;
    if (current && (op == BEGIN || op == STATUS || op == END)) outcomes = ANY;
    if (op == HELPER) outcomes = ANY;
    if (op == FAIL) outcomes = in.phase == FAILED_C ? ONE | TWO : TWO;
    if (op == AFTER) outcomes = (in.phase == CONDEMNED_C ? unsigned (ONE) : 0u) | TWO | OTHER;
    for (unsigned outcome = ZERO; outcome <= OTHER; outcome <<= 1)
      if (outcomes & outcome)
        for (unsigned variant = 0; variant < (op == BEGIN && outcome != ZERO ? 2u : 1u); ++variant)
          {
            state out (variables.length (), locations.length ());
            transfer (n, in, out, outcome, op == BEGIN && (outcome == ZERO || variant));
            for (const newtm_flow_edge &e : flow.node (n).successors)
              {
                state next (variables.length (), locations.length ()); next.copy (out);
                if (branch (g, e, next)) visit (e.target * PHASES + next.phase, next);
              }
          }
  }
  bool cycles (auto_vec<state *> &inputs)
  {
    auto_vec<unsigned> heads, degree, work; auto_vec<arc> edges;
    unsigned count = 0; heads.safe_grow (inputs.length ()); degree.safe_grow_cleared (inputs.length ());
    for (unsigned i = 0; i < heads.length (); ++i) heads[i] = UINT_MAX;
    for (unsigned i = 0; i < inputs.length (); ++i) if (inputs[i])
      {
        ++count;
        /* The only admitted cycle crosses a proved full readmission. */
        if (i % PHASES == CLEANED_C && classify (flow.node (i / PHASES).statement) == BEGIN && inputs[i]->retry) continue;
        successors (i / PHASES, *inputs[i], [&] (unsigned to, const state &)
          { if (inputs[to]) { arc a = { to, heads[i] }; heads[i] = edges.length (); edges.safe_push (a); ++degree[to]; } });
      }
    for (unsigned i = 0; i < inputs.length (); ++i) if (inputs[i] && !degree[i]) work.safe_push (i);
    for (unsigned at = 0; at < work.length (); ++at)
      for (unsigned e = heads[work[at]]; e != UINT_MAX; e = edges[e].next)
        if (!--degree[edges[e].target]) work.safe_push (edges[e].target);
    if (work.length () == count) return true;
    for (unsigned i = 0; i < inputs.length (); ++i) if (degree[i])
      return refuse (flow.node (i / PHASES).statement, "V07: controller cycle bypasses complete cleanup and readmission");
    return false;
  }
  bool verify ()
  {
    auto_vec<state *> inputs; inputs.safe_grow_cleared (flow.node_count () * PHASES);
    auto_vec<unsigned char> queued; queued.safe_grow_cleared (inputs.length ());
    auto_vec<unsigned> work; work.safe_push (0); queued[0] = 1;
    inputs[0] = new state (variables.length (), locations.length ());
    for (unsigned at = 0; at < work.length () && !reason; ++at)
      {
        unsigned id = work[at]; queued[id] = 0;
        if (!inspect (id / PHASES, *inputs[id])) break;
        successors (id / PHASES, *inputs[id], [&] (unsigned to, const state &next)
          {
            bool change;
            if (!inputs[to]) { inputs[to] = new state (variables.length (), locations.length ()); inputs[to]->copy (next); change = true; }
            else change = merge (*inputs[to], next);
            if (change && !queued[to]) { queued[to] = 1; work.safe_push (to); }
          });
      }
    if (!reason) cycles (inputs);
    for (state *s : inputs) delete s;
    if (reason && flag_transaction_debug)
      { fprintf (stderr, "NEWTM owner: %s\n", reason); if (bad) print_gimple_stmt (stderr, bad, 0, TDF_NONE); }
    return !reason;
  }
};
newtm_owner_analysis::newtm_owner_analysis (const newtm_flow_graph &f, tree helper, bool current)
  : m_builder (new newtm_owner_builder (f, helper, current)) {}
newtm_owner_analysis::~newtm_owner_analysis () { delete m_builder; }
void newtm_owner_analysis::invalid (gimple *g, bool bad_true)
{ m_builder->invalid.add (g); if (bad_true) m_builder->invalid_true.add (g); }
bool newtm_owner_analysis::verify (const char **why, gimple **bad)
{
  bool ok = m_builder->verify ();
  if (why) *why = m_builder->reason;
  if (bad) *bad = m_builder->bad;
  return ok;
}
bool newtm_owner_analysis::protects (tree t) const
{ return t && (t == m_builder->slot || t == m_builder->outer || t == m_builder->counter
               || m_builder->protected_values.contains (t)); }
