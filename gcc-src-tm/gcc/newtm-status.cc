/* Status belongs to a call definition, not to the spelling of its variable.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "gimple-pretty-print.h"
#include "options.h"
#include "newtm-flow.h"
#include "newtm-status.h"
#include "newtm-clone.h"

namespace {
enum phase { ABSENT = 1, PENDING = 2, SUCCESS = 4, FAILURE = 8 };
enum output { UNDEFINED = 1, DIRECT = 2, OWNED = 4, STALE = 8, RELEASED = 16, PROVEN = 32 };
enum alias { UNKNOWN, RAW, IS_OK, IS_BAD, KNOWN_FAILURE, CONSTANT_BASE };
struct operation
{
  gimple *call;
  tree result[2];
};
struct state
{
  unsigned phase = ABSENT;
  unsigned char result[2] = { UNDEFINED, UNDEFINED };
  auto_vec<unsigned char> aliases;
  explicit state (unsigned n) { aliases.safe_grow_cleared (n); }
  void copy (const state &s)
  {
    phase = s.phase;
    for (unsigned i = 0; i < 2; ++i) result[i] = s.result[i];
    for (unsigned i = 0; i < aliases.length (); ++i) aliases[i] = s.aliases[i];
  }
  bool merge (const state &s)
  {
    bool changed = false;
    unsigned next = phase | s.phase;
    if (next != phase) { phase = next; changed = true; }
    for (unsigned i = 0; i < 2; ++i)
      if ((result[i] | s.result[i]) != result[i])
        { result[i] |= s.result[i]; changed = true; }
    for (unsigned i = 0; i < aliases.length (); ++i)
      if (aliases[i] && aliases[i] != s.aliases[i])
        { aliases[i] = UNKNOWN; changed = true; }
    return changed;
  }
};
static bool variable (tree t)
{ return t && (VAR_P (t) || TREE_CODE (t) == PARM_DECL || TREE_CODE (t) == SSA_NAME || TREE_CODE (t) == RESULT_DECL); }
static tree lhs (gimple *s)
{ return s && is_gimple_assign (s) ? gimple_assign_lhs (s)
    : s && is_gimple_call (s) ? gimple_call_lhs (s) : NULL_TREE; }
}

struct newtm_status_builder
{
  const newtm_flow_graph &flow;
  auto_vec<operation> operations;
  auto_vec<tree> variables;
  hash_map<tree, unsigned> indices;
  hash_map<tree, gimple *> definitions;
  hash_set<gimple *> barriers, resets;
  hash_map<gimple *, gimple *> alternatives;
  hash_map<tree, bool> exposed;
  hash_set<tree> addressed;
  auto_vec<std::pair<gimple *, gimple *> > required;
  const char *reason = NULL;
  gimple *bad = NULL;
  bool strict = false;
  explicit newtm_status_builder (const newtm_flow_graph &f) : flow (f)
  {
    for (unsigned i = 0; i < flow.node_count (); ++i)
      if (gimple *s = flow.node (i).statement)
        {
          struct scan {
            static tree visit (tree *p, int *walk, void *data)
            {
              walk_stmt_info *wi = (walk_stmt_info *) data;
              newtm_status_builder *b = (newtm_status_builder *) wi->info;
              if (TREE_CODE (*p) == ADDR_EXPR)
                if (tree root = get_base_address (TREE_OPERAND (*p, 0)))
                  if (variable (root)) b->addressed.add (root);
              if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
              return NULL_TREE;
            }
          };
          walk_stmt_info wi = {}; wi.info = this;
          walk_gimple_op (s, scan::visit, &wi);
        }
    for (unsigned i = 0; i < flow.node_count (); ++i)
      if (tree t = lhs (flow.node (i).statement))
        if (variable (t))
          {
            if (!indices.get (t)) { indices.put (t, variables.length ()); variables.safe_push (t); }
            gimple **old = definitions.get (t), *s = flow.node (i).statement;
            if (!old) definitions.put (t, s);
            else if (*old != s) *old = NULL;
          }
  }
  bool refuse (gimple *s, const char *why)
  { if (!reason) { reason = why; bad = s; } return false; }
  /* Only unambiguous, unescaped address copies are aliases. Unknown indirect
     stores still invalidate every possibly overlapping result below. */
  tree address (tree t, unsigned depth = 0)
  {
    if (!t || depth > 32) return NULL_TREE;
    if (TREE_CODE (t) == ADDR_EXPR) return get_base_address (TREE_OPERAND (t, 0));
    if (CONVERT_EXPR_CODE_P (TREE_CODE (t))) return address (TREE_OPERAND (t, 0), depth + 1);
    if (variable (t) && !(DECL_P (t) && TREE_ADDRESSABLE (t)))
      if (gimple **d = definitions.get (t))
        if (*d && is_gimple_assign (*d)
            && (gimple_assign_single_p (*d) || CONVERT_EXPR_CODE_P (gimple_assign_rhs_code (*d))))
          return address (gimple_assign_rhs1 (*d), depth + 1);
    return NULL_TREE;
  }
  tree base (tree t)
  {
    tree b = get_base_address (t);
    if (b && (TREE_CODE (b) == MEM_REF || TREE_CODE (b) == INDIRECT_REF))
      return address (TREE_OPERAND (b, 0));
    return b;
  }
  bool may_address (tree pointer, tree object)
  {
    auto_vec<tree> work; hash_set<tree> seen;
    if (pointer) work.safe_push (pointer);
    for (unsigned at = 0; at < work.length (); ++at)
      {
        tree t = work[at]; if (!t || seen.add (t)) continue;
        if (TREE_CODE (t) == ADDR_EXPR)
          { if (get_base_address (TREE_OPERAND (t, 0)) == object) return true; continue; }
        if (TREE_CODE (t) == CONSTRUCTOR)
          {
            unsigned HOST_WIDE_INT index; tree value;
            FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (t), index, value) work.safe_push (value);
          }
        if (variable (t))
          {
            for (unsigned i = 0; i < flow.node_count (); ++i)
              if (gimple *g = flow.node (i).statement)
                if (is_gimple_assign (g) && gimple_assign_lhs (g) == t)
                  for (unsigned n = 1; n < gimple_num_ops (g); ++n) work.safe_push (gimple_op (g, n));
          }
        else if (EXPR_P (t))
          for (int n = 0; n < TREE_OPERAND_LENGTH (t); ++n)
            work.safe_push (TREE_OPERAND (t, n));
      }
    return false;
  }
  bool escapes (tree object)
  {
    if (bool *old = exposed.get (object)) return *old;
    bool result = !VAR_P (object) || TREE_STATIC (object) || DECL_EXTERNAL (object)
      || !DECL_CONTEXT (object) || TREE_CODE (DECL_CONTEXT (object)) != FUNCTION_DECL;
    for (unsigned i = 0; !result && i < flow.node_count (); ++i)
      if (gimple *g = flow.node (i).statement)
        {
          if (is_gimple_assign (g))
            {
              tree l = lhs (g);
              bool escape = REFERENCE_CLASS_P (l) || (DECL_P (l)
                && (TREE_STATIC (l) || DECL_EXTERNAL (l)));
              if (escape)
                for (unsigned n = 1; n < gimple_num_ops (g); ++n)
                  if (may_address (gimple_op (g, n), object)) result = true;
            }
          else if (is_gimple_call (g) && !gimple_call_internal_p (g))
            {
              bool producer = false;
              for (const operation &op : operations)
                if (op.call == g && (op.result[0] == object || op.result[1] == object)) producer = true;
              if (producer) continue;
              tree fn = gimple_call_fndecl (g);
              bool runtime = fn && fndecl_built_in_p (fn, BUILT_IN_NORMAL)
                && DECL_FUNCTION_CODE (fn) >= BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN
                && DECL_FUNCTION_CODE (fn) <= BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL;
              bool registration = runtime && fndecl_built_in_p (fn, BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER);
              if (runtime && !registration) continue; /* ABI does not retain buffers. */
              unsigned first = fn && newtm_clone_original (fn) ? 2 : 0;
              for (unsigned n = first; n < gimple_call_num_args (g); ++n)
                if (may_address (gimple_call_arg (g, n), object)) result = true;
            }
          else if (gimple_code (g) == GIMPLE_RETURN
                   && may_address (gimple_return_retval (as_a<greturn *> (g)), object)) result = true;
        }
    exposed.put (object, result); return result;
  }
  bool reads (tree t, tree object)
  {
    if (!t) return false;
    if (TREE_CODE (t) == ADDR_EXPR) return false;
    if (TREE_CODE (t) == CONSTRUCTOR)
      {
        unsigned HOST_WIDE_INT n; tree value;
        FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (t), n, value)
          if (reads (value, object)) return true;
        return false;
      }
    if (t == object || (REFERENCE_CLASS_P (t) && base (t) == object)) return true;
    if ((TREE_CODE (t) == MEM_REF || TREE_CODE (t) == INDIRECT_REF)
        && (may_address (TREE_OPERAND (t, 0), object) || escapes (object))) return true;
    if (EXPR_P (t))
      for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
        if (reads (TREE_OPERAND (t, i), object)) return true;
    return false;
  }
  unsigned form (tree t, const state &s, unsigned depth = 0)
  {
    if (!t || depth > 32) return UNKNOWN;
    if (TREE_CODE (t) == INTEGER_CST && tree_fits_uhwi_p (t) && tree_to_uhwi (t) < 32)
      return CONSTANT_BASE + tree_to_uhwi (t);
    if (unsigned *i = indices.get (t)) return s.aliases[*i];
    if (CONVERT_EXPR_CODE_P (TREE_CODE (t)) && INTEGRAL_TYPE_P (TREE_TYPE (t))
        && INTEGRAL_TYPE_P (TREE_TYPE (TREE_OPERAND (t, 0)))
        && TYPE_PRECISION (TREE_TYPE (t)) >= TYPE_PRECISION (TREE_TYPE (TREE_OPERAND (t, 0))))
      return form (TREE_OPERAND (t, 0), s, depth + 1);
    tree_code c = TREE_CODE (t);
    if (c == TRUTH_NOT_EXPR)
      { unsigned a = form (TREE_OPERAND (t, 0), s, depth + 1);
        return a == IS_OK ? IS_BAD : a == IS_BAD ? IS_OK : UNKNOWN; }
    if ((c == EQ_EXPR || c == NE_EXPR) && integer_zerop (TREE_OPERAND (t, 1)))
      {
        unsigned a = form (TREE_OPERAND (t, 0), s, depth + 1);
        if (a == RAW) return c == EQ_EXPR ? IS_OK : IS_BAD;
        if (a == IS_OK || a == IS_BAD) return c == NE_EXPR ? a : unsigned (a == IS_OK ? IS_BAD : IS_OK);
      }
    return UNKNOWN;
  }
  unsigned assigned (gimple *g, const state &s)
  {
    if (!is_gimple_assign (g)) return UNKNOWN;
    tree_code c = gimple_assign_rhs_code (g);
    if (gimple_assign_single_p (g)) return form (gimple_assign_rhs1 (g), s);
    if (CONVERT_EXPR_CODE_P (c))
      {
        tree a = gimple_assign_rhs1 (g), t = TREE_TYPE (gimple_assign_lhs (g));
        if (INTEGRAL_TYPE_P (t) && INTEGRAL_TYPE_P (TREE_TYPE (a))
            && TYPE_PRECISION (t) >= TYPE_PRECISION (TREE_TYPE (a))) return form (a, s);
      }
    if (c == TRUTH_NOT_EXPR)
      { unsigned a = form (gimple_assign_rhs1 (g), s);
        return a == IS_OK ? IS_BAD : a == IS_BAD ? IS_OK : UNKNOWN; }
    if ((c == EQ_EXPR || c == NE_EXPR) && integer_zerop (gimple_assign_rhs2 (g)))
      {
        unsigned a = form (gimple_assign_rhs1 (g), s);
        if (a == RAW) return c == EQ_EXPR ? IS_OK : IS_BAD;
        if (a == IS_OK || a == IS_BAD) return c == NE_EXPR ? a : unsigned (a == IS_OK ? IS_BAD : IS_OK);
      }
    return UNKNOWN;
  }
  unsigned condition (gimple *g, const state &s)
  {
    if (!g || gimple_code (g) != GIMPLE_COND) return UNKNOWN;
    gcond *c = as_a<gcond *> (g);
    tree a = gimple_cond_lhs (c), b = gimple_cond_rhs (c);
    tree_code code = gimple_cond_code (c);
    if (TREE_CODE (a) == INTEGER_CST && TREE_CODE (b) != INTEGER_CST) std::swap (a, b);
    unsigned f = form (a, s);
    if ((code == EQ_EXPR || code == NE_EXPR) && integer_zerop (b))
      {
        if (f == RAW) return code == EQ_EXPR ? IS_OK : IS_BAD;
        if (f == IS_OK || f == IS_BAD) return code == NE_EXPR ? f : unsigned (f == IS_OK ? IS_BAD : IS_OK);
      }
    return UNKNOWN;
  }
  bool read_at (gimple *g, tree object)
  {
    if (is_gimple_assign (g))
      {
        if (!gimple_clobber_p (g))
          for (unsigned i = 1; i < gimple_num_ops (g); ++i)
            if (reads (gimple_op (g, i), object)) return true;
        /* Indices and pointer operands of a destination are values too. */
        tree l = gimple_assign_lhs (g);
        if (REFERENCE_CLASS_P (l))
          for (int i = 0; i < TREE_OPERAND_LENGTH (l); ++i)
            if (TREE_OPERAND (l, i) && TREE_OPERAND (l, i) != object
                && reads (TREE_OPERAND (l, i), object)) return true;
        return false;
      }
    if (is_gimple_call (g))
      {
        bool produces = false;
        for (const operation &op : operations)
          if (op.call == g && (op.result[0] == object || op.result[1] == object)) produces = true;
        for (unsigned i = 0; i < gimple_call_num_args (g); ++i)
          {
            tree a = gimple_call_arg (g, i);
            if (reads (a, object) || (!produces && POINTER_TYPE_P (TREE_TYPE (a))
                                    && may_address (a, object))) return true;
          }
        return false;
      }
    if (gimple_code (g) == GIMPLE_RETURN) return reads (gimple_return_retval (as_a<greturn *> (g)), object);
    if (gimple_code (g) == GIMPLE_COND || gimple_code (g) == GIMPLE_SWITCH)
      for (unsigned i = 0; i < gimple_num_ops (g); ++i)
        if (reads (gimple_op (g, i), object)) return true;
    return false;
  }
  bool continuation (gimple *g)
  {
    if (!g) return false;
    if (barriers.contains (g)) return true;
    if (is_gimple_call (g) && !gimple_call_internal_p (g))
      {
        tree f = gimple_call_fndecl (g);
        if (!f || !fndecl_built_in_p (f, BUILT_IN_NORMAL)) return true;
        switch (DECL_FUNCTION_CODE (f))
          {
          case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL:
          case BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE:
          case BUILT_IN_GOMP_TRANSACTION_STM_V1_DISCARD:
          case BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL: break;
          default: return true;
          }
      }
    if (gimple_code (g) == GIMPLE_RETURN)
      return true;
    if (is_gimple_assign (g) && !gimple_clobber_p (g))
      for (unsigned i = 0; i < gimple_num_ops (g); ++i)
        if (REFERENCE_CLASS_P (gimple_op (g, i))
            || (VAR_P (gimple_op (g, i))
                && (TREE_STATIC (gimple_op (g, i)) || DECL_EXTERNAL (gimple_op (g, i))))) return true;
    return false;
  }
  /* Transfer never invents output availability from an unrelated STATUS call. */
  void transfer (gimple *g, const operation &op, state &s)
  {
    if (!g) return;
    unsigned value = assigned (g, s);
    if (tree t = lhs (g))
      if (unsigned *i = indices.get (t)) s.aliases[*i] = value;
    tree l = lhs (g);
    /* Cleanup selectors are local facts. Ordinary globals can change without
       an address being visible in this body; never prune an entry using a
       stale global constant after a call or a possible indirect writer. */
    if (l && (REFERENCE_CLASS_P (l) || (VAR_P (l) && (TREE_STATIC (l) || DECL_EXTERNAL (l)))))
      for (unsigned i = 0; i < variables.length (); ++i)
        if (DECL_P (variables[i]) && (TREE_STATIC (variables[i]) || DECL_EXTERNAL (variables[i])
            || TREE_ADDRESSABLE (variables[i]) || addressed.contains (variables[i]))) s.aliases[i] = UNKNOWN;
    if (resets.contains (g))
      {
        s.phase = ABSENT;
        for (unsigned i = 0; i < s.aliases.length (); ++i) s.aliases[i] = UNKNOWN;
        for (unsigned i = 0; i < 2; ++i) s.result[i] = STALE;
        return;
      }
    if (g == op.call)
      {
        s.phase = PENDING;
        for (unsigned i = 0; i < s.aliases.length (); ++i) s.aliases[i] = UNKNOWN;
        s.aliases[*indices.get (gimple_call_lhs (g))] = RAW;
        for (unsigned i = 0; i < 2; ++i) s.result[i] = OWNED;
        return;
      }
    if (is_gimple_call (g))
      {
        for (unsigned i = 0; i < variables.length (); ++i)
          if (DECL_P (variables[i]) && (TREE_STATIC (variables[i]) || DECL_EXTERNAL (variables[i])
              || TREE_ADDRESSABLE (variables[i]) || addressed.contains (variables[i]))) s.aliases[i] = UNKNOWN;
        for (const operation &other : operations)
          if (other.call == g)
            for (unsigned i = 0; i < 2; ++i)
              for (tree object : other.result)
                if (object && object == op.result[i]) s.result[i] = RELEASED;
        tree fn = gimple_call_fndecl (g);
        bool runtime = fn && fndecl_built_in_p (fn, BUILT_IN_NORMAL)
          && DECL_FUNCTION_CODE (fn) >= BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN
          && DECL_FUNCTION_CODE (fn) <= BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL;
        if (runtime && fndecl_built_in_p (fn, BUILT_IN_GOMP_TRANSACTION_STM_V1_END))
          for (unsigned i = 0; i < 2; ++i)
            if (op.result[i] && escapes (op.result[i])) s.result[i] = STALE;
        if (!gimple_call_internal_p (g) && !runtime)
          for (unsigned i = 0; i < 2; ++i)
            if (op.result[i] && s.result[i] != RELEASED)
              {
                if (escapes (op.result[i])) s.result[i] = STALE;
                for (unsigned n = 0; n < gimple_call_num_args (g); ++n)
                  {
                    tree a = gimple_call_arg (g, n);
                    bool prepared = fn && newtm_clone_original (fn);
                    /* A prepared frame borrows explicit channels. Its context
                       cannot expose a distinct automatic compiler buffer.
                       Actual program aliases, including aggregates, kill. */
                    bool possible = may_address (a, op.result[i]);
                    if (prepared ? n >= 2 && possible
                        : possible || (POINTER_TYPE_P (TREE_TYPE (a)) && !integer_zerop (a) && !address (a)))
                      s.result[i] = STALE;
                  }
              }
        if (fn && fndecl_built_in_p (fn, BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL))
          if (tree t = lhs (g))
            if (unsigned *i = indices.get (t)) s.aliases[*i] = KNOWN_FAILURE;
      }
    for (unsigned i = 0; i < 2; ++i)
      if (op.result[i] && l)
        {
          tree b = base (l);
          tree raw = get_base_address (l);
          bool possible = REFERENCE_CLASS_P (l)
            && (escapes (op.result[i])
                || (raw && (TREE_CODE (raw) == MEM_REF || TREE_CODE (raw) == INDIRECT_REF)
                    && may_address (TREE_OPERAND (raw, 0), op.result[i])));
          if (b == op.result[i] || possible)
            {
              gimple **alternate = alternatives.get (g);
              bool producer = alternate && *alternate == op.call;
              s.result[i] = ((!strict && s.phase == ABSENT) || producer) && l == op.result[i]
                && !gimple_clobber_p (g) ? DIRECT : STALE;
            }
        }
  }
  unsigned branch (gimple *g, const newtm_flow_edge &edge, state &s)
  {
    if (edge.kind != NEWTM_FLOW_TRUE && edge.kind != NEWTM_FLOW_FALSE) return s.phase;
    if (g && gimple_code (g) == GIMPLE_COND)
      {
        gcond *c = as_a<gcond *> (g);
        tree a = gimple_cond_lhs (c), b = gimple_cond_rhs (c);
        unsigned av = form (a, s), bv = form (b, s);
        if (av >= CONSTANT_BASE) a = build_int_cst (TREE_TYPE (a), av - CONSTANT_BASE);
        if (bv >= CONSTANT_BASE) b = build_int_cst (TREE_TYPE (b), bv - CONSTANT_BASE);
        if (TREE_CODE (a) == INTEGER_CST && TREE_CODE (b) == INTEGER_CST)
          {
            bool lt = tree_int_cst_lt (a, b), eq = tree_int_cst_equal (a, b), truth;
            switch (gimple_cond_code (c))
              {
              case EQ_EXPR: truth = eq; break; case NE_EXPR: truth = !eq; break;
              case LT_EXPR: truth = lt; break; case LE_EXPR: truth = lt || eq; break;
              case GT_EXPR: truth = !lt && !eq; break; case GE_EXPR: truth = !lt; break;
              default: return s.phase;
              }
            if (truth != (edge.kind == NEWTM_FLOW_TRUE)) return 0;
          }
      }
    unsigned f = condition (g, s);
    if (f == UNKNOWN) return s.phase;
    bool success = (edge.kind == NEWTM_FLOW_TRUE) == (f == IS_OK);
    unsigned possible = s.phase & (ABSENT | (success ? SUCCESS : FAILURE));
    if (s.phase & PENDING) possible |= success ? SUCCESS : FAILURE;
    if (success)
      for (unsigned i = 0; i < 2; ++i)
        if (s.result[i] & OWNED) s.result[i] = (s.result[i] & ~OWNED) | PROVEN;
    return possible;
  }
  bool inspect (unsigned id, const operation &op, const state &s)
  {
    const newtm_flow_node &node = flow.node (id);
    gimple *g = node.statement;
    for (const auto &r : required)
      if (r.first == op.call && r.second == g && s.phase != SUCCESS)
        return refuse (g, "V05: publication lacks success of its own final status definition");
    bool forward = false;
    if (g && gimple_code (g) == GIMPLE_RETURN)
      {
        tree t = gimple_return_retval (as_a<greturn *> (g));
        unsigned f = form (t, s);
        forward = t && ((TREE_CODE (t) == INTEGER_CST && !integer_zerop (t))
                         || f == RAW || f == KNOWN_FAILURE);
      }
    if ((s.phase & (PENDING | FAILURE))
        && ((continuation (g) && !forward) || node.kind == NEWTM_FLOW_NORMAL_EXIT))
      return refuse (g ? g : op.call, "V05: continuation lacks success of the corresponding call definition");
    if (g)
      for (unsigned i = 0; i < 2; ++i)
        if (op.result[i] && read_at (g, op.result[i]))
          {
            if (s.result[i] & (UNDEFINED | STALE))
              return refuse (g, "V05: result or binding is unavailable or was overwritten");
            if (s.result[i] & OWNED)
              return refuse (g, "V05: result or binding consumed before its own call succeeded");
          }
    return true;
  }
  bool analyze (const operation &op)
  {
    /* Native cleanup dispatch merges success and failure before testing its
       selector. Keep those phases separate, preserving selector constants
       on each path instead of inventing a successful failure continuation. */
    auto_vec<state *> inputs; inputs.safe_grow_cleared (flow.node_count () * 4);
    auto_vec<unsigned char> queued; queued.safe_grow_cleared (inputs.length ());
    auto_vec<unsigned> work; work.safe_push (0); queued[0] = 1;
    inputs[0] = new state (variables.length ());
    for (unsigned at = 0; at < work.length (); ++at)
      {
        unsigned row = work[at], id = row / 4; queued[row] = 0;
        state out (variables.length ()); out.copy (*inputs[row]);
        gimple *g = flow.node (id).statement;
        transfer (g, op, out);
        for (const newtm_flow_edge &edge : flow.node (id).successors)
          {
            state next (variables.length ()); next.copy (out);
            next.phase = branch (g, edge, next);
            if (!next.phase) continue;
            unsigned phase = next.phase == ABSENT ? 0 : next.phase == PENDING ? 1 : next.phase == SUCCESS ? 2 : 3;
            unsigned target = edge.target * 4 + phase;
            bool changed;
            if (!inputs[target])
              { inputs[target] = new state (variables.length ()); inputs[target]->copy (next); changed = true; }
            else changed = inputs[target]->merge (next);
            if (changed && !queued[target]) { queued[target] = 1; work.safe_push (target); }
          }
      }
    bool ok = true;
    for (unsigned i = 0; i < inputs.length (); ++i)
      if (inputs[i] && ok) ok = inspect (i / 4, op, *inputs[i]);
    if (!ok && flag_transaction_debug)
      {
        fputs ("NEWTM status origin: ", stderr); print_gimple_stmt (stderr, op.call, 0, TDF_NONE);
        for (unsigned i = 0; i < inputs.length (); ++i)
          if (inputs[i] && flow.node (i / 4).statement == bad)
            fprintf (stderr, "NEWTM status state node=%u phase=%u outputs=%u,%u\n", i / 4,
                     inputs[i]->phase, inputs[i]->result[0], inputs[i]->result[1]);
        fputs ("NEWTM status use: ", stderr); print_gimple_stmt (stderr, bad, 0, TDF_NONE);
      }
    for (state *s : inputs) delete s;
    return ok;
  }
};

newtm_status_analysis::newtm_status_analysis (const newtm_flow_graph &f) : m_builder (new newtm_status_builder (f)) {}
newtm_status_analysis::~newtm_status_analysis () { delete m_builder; }
bool newtm_status_analysis::operation (gimple *s, tree result, tree binding)
{
  if (!s || !is_gimple_call (s) || !variable (gimple_call_lhs (s))
      || !INTEGRAL_TYPE_P (TREE_TYPE (gimple_call_lhs (s)))
      || !TYPE_UNSIGNED (TREE_TYPE (gimple_call_lhs (s)))
      || TYPE_PRECISION (TREE_TYPE (gimple_call_lhs (s))) != 32)
    return m_builder->refuse (s, "V05: operation has no explicit 32-bit status definition");
  struct operation op = { s, { result, binding } };
  m_builder->operations.safe_push (op); m_builder->barriers.add (s); return true;
}
void newtm_status_analysis::continuation (gimple *s) { m_builder->barriers.add (s); }
void newtm_status_analysis::alternative (gimple *call, gimple *store)
{ m_builder->alternatives.put (store, call); m_builder->barriers.add (store); }
void newtm_status_analysis::reset (gimple *s) { m_builder->resets.add (s); }
void newtm_status_analysis::require_success (gimple *call, gimple *use)
{ m_builder->required.safe_push (std::make_pair (call, use)); }
void newtm_status_analysis::strict_outputs () { m_builder->strict = true; }
bool newtm_status_analysis::verify (const char **why, gimple **bad)
{
  bool ok = !m_builder->reason;
  for (const struct operation &op : m_builder->operations)
    if (ok) ok = m_builder->analyze (op);
  if (why) *why = m_builder->reason;
  if (bad) *bad = m_builder->bad;
  return ok;
}
