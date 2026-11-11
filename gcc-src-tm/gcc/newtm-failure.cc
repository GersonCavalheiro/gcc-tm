/* Failure paths abandon nesting; only the external owner cleans the slot.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-pretty-print.h"
#include "options.h"
#include "newtm-flow.h"
#include "newtm-failure.h"

namespace {
enum bits { OK = 1, CONFLICT = 2, INVALID = 4, OTHER = 8, ANY = 15 };
enum source { NONE, CAUSE, EFFECTIVE };
enum context { NO_CONTEXT, EXACT_CONTEXT, MAY_CONTEXT };
enum mode { HEALTHY, CALL_CONFLICT, CALL_INVALID, CALL_OTHER, GUARD_INVALID,
            FINAL_CONFLICT, FINAL_INVALID, MODES };
enum operation { PURE, STATUS, ACCESS, PREPARED, NEST_BEGIN, NEST_END, FAIL, SLOT, UNSUPPORTED };
struct value { unsigned char bits = ANY, source = NONE, context = NO_CONTEXT; };
struct state
{
  unsigned mode = HEALTHY, depth = 0;
  value returned;
  auto_vec<value> values;
  explicit state (unsigned n)
  { for (unsigned i = 0; i < n; ++i) values.safe_push (value ()); }
  void copy (const state &s)
  { mode = s.mode; depth = s.depth; returned = s.returned;
    for (unsigned i = 0; i < values.length (); ++i) values[i] = s.values[i]; }
};
static bool variable (tree t)
{ return t && (VAR_P (t) || TREE_CODE (t) == PARM_DECL || TREE_CODE (t) == RESULT_DECL || TREE_CODE (t) == SSA_NAME); }
static tree lhs (gimple *g)
{ return g && is_gimple_assign (g) ? gimple_assign_lhs (g) : g && is_gimple_call (g) ? gimple_call_lhs (g) : NULL_TREE; }
static operation classify (gimple *g)
{
  if (!g || !is_gimple_call (g) || gimple_call_internal_p (g)) return PURE;
  tree fn = gimple_call_fndecl (g);
  if (!fn) return UNSUPPORTED;
  if (!fndecl_built_in_p (fn, BUILT_IN_NORMAL)) return PREPARED;
  switch (DECL_FUNCTION_CODE (fn))
    {
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS: return STATUS;
    case BUILT_IN_DWARF_CFA: return PURE;
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_READ:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER: return ACCESS;
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN: return NEST_BEGIN;
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END: return NEST_END;
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL: return FAIL;
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_END:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_DISCARD:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL: return SLOT;
    default: return UNSUPPORTED;
    }
}
static value constant (tree t)
{
  value v;
  v.bits = integer_zerop (t) ? OK : integer_onep (t) ? CONFLICT
    : tree_fits_uhwi_p (t) && tree_to_uhwi (t) == 2 ? INVALID : OTHER;
  return v;
}
static bool merge_value (value &a, value b)
{
  value n = a;
  n.bits |= b.bits;
  if (n.source != b.source) n.source = NONE;
  if (n.context != b.context) n.context = MAY_CONTEXT;
  bool changed = n.bits != a.bits || n.source != a.source || n.context != a.context;
  a = n; return changed;
}
struct arc { unsigned target, next; };
}
struct newtm_failure_builder
{
  const newtm_flow_graph &flow;
  tree ctx;
  hash_map<tree, unsigned> indices;
  auto_vec<tree> variables;
  hash_map<gimple *, bool> invalid;
  const char *reason = NULL; gimple *bad = NULL;
  newtm_failure_builder (const newtm_flow_graph &f, tree c) : flow (f), ctx (c)
  {
    for (unsigned n = 0; n < flow.node_count (); ++n)
      if (tree t = lhs (flow.node (n).statement))
        if (variable (t) && !indices.get (t))
          { indices.put (t, variables.length ()); variables.safe_push (t); }
  }
  bool refuse (gimple *g, const char *why)
  { if (!reason) { reason = why; bad = g; } return false; }
  value compare (tree_code code, value a, value b)
  {
    value out; out.bits = 0;
    for (unsigned i = 0; i < 4; ++i) for (unsigned j = 0; j < 4; ++j)
      if ((a.bits & (1u << i)) && (b.bits & (1u << j)))
        {
          if (i == 3 && j == 3) out.bits |= OK | CONFLICT;
          else out.bits |= ((i == j) == (code == EQ_EXPR)) ? CONFLICT : OK;
        }
    return out;
  }
  value get (tree t, const state &s, unsigned depth = 0)
  {
    value v;
    if (!t || depth > 32) return v;
    if (t == ctx) { v.context = EXACT_CONTEXT; return v; }
    if (TREE_CODE (t) == INTEGER_CST) return constant (t);
    if (unsigned *i = indices.get (t)) return s.values[*i];
    if (CONVERT_EXPR_CODE_P (TREE_CODE (t)))
      {
        tree a = TREE_OPERAND (t, 0);
        if ((INTEGRAL_TYPE_P (TREE_TYPE (t)) || POINTER_TYPE_P (TREE_TYPE (t)))
            && (INTEGRAL_TYPE_P (TREE_TYPE (a)) || POINTER_TYPE_P (TREE_TYPE (a)))
            && TYPE_PRECISION (TREE_TYPE (t)) >= TYPE_PRECISION (TREE_TYPE (a))) return get (a, s, depth + 1);
      }
    if (TREE_CODE (t) == EQ_EXPR || TREE_CODE (t) == NE_EXPR)
      return compare (TREE_CODE (t), get (TREE_OPERAND (t, 0), s, depth + 1), get (TREE_OPERAND (t, 1), s, depth + 1));
    if (TREE_CODE (t) == CONSTRUCTOR)
      {
        unsigned HOST_WIDE_INT n; tree part;
        FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (t), n, part)
          if (get (part, s, depth + 1).context != NO_CONTEXT) v.context = MAY_CONTEXT;
      }
    if (EXPR_P (t))
      for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
        if (get (TREE_OPERAND (t, i), s, depth + 1).context != NO_CONTEXT) v.context = MAY_CONTEXT;
    return v;
  }
  value assigned (gimple *g, const state &s)
  {
    value v;
    if (!is_gimple_assign (g) || gimple_clobber_p (g)) return v;
    if (gimple_assign_single_p (g)) return get (gimple_assign_rhs1 (g), s);
    if (gimple_assign_cast_p (g))
      {
        tree from = TREE_TYPE (gimple_assign_rhs1 (g)), to = TREE_TYPE (lhs (g));
        if ((INTEGRAL_TYPE_P (from) || POINTER_TYPE_P (from))
            && (INTEGRAL_TYPE_P (to) || POINTER_TYPE_P (to))
            && TYPE_PRECISION (to) >= TYPE_PRECISION (from)) return get (gimple_assign_rhs1 (g), s);
      }
    tree_code c = gimple_assign_rhs_code (g);
    if (c == EQ_EXPR || c == NE_EXPR) return compare (c, get (gimple_assign_rhs1 (g), s), get (gimple_assign_rhs2 (g), s));
    for (unsigned i = 1; i < gimple_num_ops (g); ++i)
      if (get (gimple_op (g, i), s).context != NO_CONTEXT) v.context = MAY_CONTEXT;
    return v;
  }
  void put (tree t, value v, state &s)
  { if (unsigned *i = indices.get (t)) s.values[*i] = v; }
  bool address_of_context (tree t)
  {
    if (!t) return false;
    if (TREE_CODE (t) == ADDR_EXPR && get_base_address (TREE_OPERAND (t, 0)) == ctx) return true;
    if (EXPR_P (t)) for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
      if (address_of_context (TREE_OPERAND (t, i))) return true;
    return false;
  }
  bool memory (tree t)
  {
    if (!t || TREE_CODE (t) == ADDR_EXPR) return false;
    if (TREE_CODE (t) == REALPART_EXPR || TREE_CODE (t) == IMAGPART_EXPR) return false;
    return REFERENCE_CLASS_P (t) || (VAR_P (t) && (TREE_STATIC (t) || DECL_EXTERNAL (t)));
  }
  bool inspect (unsigned n, const state &s)
  {
    const newtm_flow_node &node = flow.node (n); gimple *g = node.statement;
    if (node.kind == NEWTM_FLOW_NORMAL_EXIT || node.kind == NEWTM_FLOW_TERMINATE_EXIT)
      return refuse (g ? g : node.origin, "V07: borrowed frame does not return an explicit effective status");
    if (node.kind == NEWTM_FLOW_RETURN_EXIT)
      {
        if (s.mode == HEALTHY)
          return s.returned.bits == OK && s.depth == 0
            ? true : refuse (node.origin, "V07: success return loses failure or leaves unbalanced relative nesting");
        if (s.mode != FINAL_CONFLICT && s.mode != FINAL_INVALID)
          return refuse (node.origin, "V07: failure returns without passing through FAIL");
        unsigned required = s.mode == FINAL_CONFLICT ? CONFLICT : INVALID;
        return s.returned.source == EFFECTIVE && s.returned.bits == required
          ? true : refuse (node.origin, "V07: failure does not return the effective FAIL result");
      }
    if (!g) return true;
    if (lhs (g) == ctx) return refuse (g, "V07: borrowed context identity is overwritten");
    for (unsigned i = 0; i < gimple_num_ops (g); ++i)
      if (address_of_context (gimple_op (g, i))) return refuse (g, "V07: address of the borrowed context carrier escapes");
    operation op = classify (g);
    if (op == SLOT) return refuse (g, "V07: borrowed frame invokes a slot-owning or terminal service");
    if (op == UNSUPPORTED) return refuse (g, "V07: call has no admitted borrowed-frame contract");
    if (op != PURE)
      {
        if (!gimple_call_num_args (g) || get (gimple_call_arg (g, 0), s).context != EXACT_CONTEXT)
          return refuse (g, "V07: operation does not borrow the original context");
        tree t = lhs (g);
        if (!variable (t) || !INTEGRAL_TYPE_P (TREE_TYPE (t)) || !TYPE_UNSIGNED (TREE_TYPE (t))
            || TYPE_PRECISION (TREE_TYPE (t)) != 32)
          return refuse (g, "V07: borrowed operation has no explicit uint32 status");
        for (unsigned i = 1; i < gimple_call_num_args (g); ++i)
          if (get (gimple_call_arg (g, i), s).context != NO_CONTEXT)
            return refuse (g, "V07: borrowed context escapes through a data argument");
        if (op == FAIL)
          {
            if (s.mode == FINAL_CONFLICT || s.mode == FINAL_INVALID)
              return refuse (g, "V07: failure epilogue condemns the same frame twice");
            if (gimple_call_num_args (g) != 2) return refuse (g, "V07: FAIL lacks its explicit cause");
            value cause = get (gimple_call_arg (g, 1), s);
            if (s.mode == HEALTHY || s.mode == GUARD_INVALID)
              return !(cause.bits & CONFLICT) ? true : refuse (g, "V07: validation failure can be translated into a retry");
            unsigned expected = s.mode == CALL_CONFLICT ? CONFLICT : s.mode == CALL_INVALID ? INVALID : OTHER;
            return cause.source == CAUSE && cause.bits == expected
              ? true : refuse (g, "V07: epilogue loses or substitutes the originating operation status");
          }
        if (s.mode != HEALTHY) return refuse (g, "V07: failed frame continues with a normal or nested operation");
        if (op == NEST_END && !s.depth) return refuse (g, "V07: nested end would consume the caller's level");
      }
    if (is_gimple_assign (g) && !gimple_clobber_p (g))
      {
        value v = assigned (g, s);
        if (memory (lhs (g)) && v.context != NO_CONTEXT) return refuse (g, "V07: borrowed context is stored into memory");
        if (s.mode != HEALTHY)
          for (unsigned i = 0; i < gimple_num_ops (g); ++i)
            if (memory (gimple_op (g, i))) return refuse (g, "V07: failed frame performs a program memory access");
      }
    if (s.mode != HEALTHY && invalid.get (g)) return refuse (g, "V07: failed frame continues with normal validation");
    return true;
  }
  void failure (state &s, unsigned mode)
  {
    s.mode = mode; s.depth = UINT_MAX;
    for (value &v : s.values) v.source = NONE;
  }
  void transfer (unsigned n, const state &in, state &out, unsigned outcome)
  {
    out.copy (in); gimple *g = flow.node (n).statement;
    if (!g) return;
    operation op = classify (g);
    if (is_gimple_call (g))
      {
        value v;
        if (op == FAIL)
          { out.mode = outcome == CONFLICT ? FINAL_CONFLICT : FINAL_INVALID; out.depth = UINT_MAX;
            v.bits = outcome; v.source = EFFECTIVE; }
        else if (op != PURE && op != SLOT && op != UNSUPPORTED)
          {
            v.bits = outcome;
            if (outcome != OK)
              { failure (out, outcome == CONFLICT ? CALL_CONFLICT : outcome == INVALID ? CALL_INVALID : CALL_OTHER); v.source = CAUSE; }
            else if (op == NEST_BEGIN) ++out.depth;
            else if (op == NEST_END && out.depth) --out.depth;
          }
        put (lhs (g), v, out);
      }
    else if (is_gimple_assign (g)) put (lhs (g), assigned (g, in), out);
    else if (gimple_code (g) == GIMPLE_RETURN) out.returned = get (gimple_return_retval (as_a<greturn *> (g)), in);
  }
  unsigned outcomes (gimple *g, const state &s)
  {
    operation op = classify (g);
    if (op == PURE || op == SLOT || op == UNSUPPORTED) return OK;
    if (op == FAIL)
      {
        value cause = gimple_call_num_args (g) > 1 ? get (gimple_call_arg (g, 1), s) : value ();
        return cause.bits == CONFLICT ? CONFLICT | INVALID : INVALID;
      }
    /* The executable frontier must conservatively normalize future/unknown
       nonzero statuses as permanent errors too, even for runtime services. */
    return OK | CONFLICT | INVALID | OTHER;
  }
  bool branch (gimple *g, const newtm_flow_edge &e, state &s)
  {
    if (!g || gimple_code (g) != GIMPLE_COND || (e.kind != NEWTM_FLOW_TRUE && e.kind != NEWTM_FLOW_FALSE)) return true;
    gcond *c = as_a<gcond *> (g); tree a = gimple_cond_lhs (c), b = gimple_cond_rhs (c);
    tree_code code = gimple_cond_code (c);
    if (code == EQ_EXPR || code == NE_EXPR)
      {
        value v = compare (code, get (a, s), get (b, s));
        if (!(v.bits & (e.kind == NEWTM_FLOW_TRUE ? CONFLICT : OK))) return false;
        if (TREE_CODE (a) == INTEGER_CST && variable (b)) std::swap (a, b);
        if (TREE_CODE (b) == INTEGER_CST && constant (b).bits != OTHER)
          if (unsigned *i = indices.get (a))
            {
              bool equal = (e.kind == NEWTM_FLOW_TRUE) == (code == EQ_EXPR);
              s.values[*i].bits &= equal ? constant (b).bits : ANY ^ constant (b).bits;
              if (!s.values[*i].bits) return false;
            }
      }
    else if (TREE_CODE (a) == INTEGER_CST && TREE_CODE (b) == INTEGER_CST)
      {
        bool lt = tree_int_cst_lt (a, b), eq = tree_int_cst_equal (a, b), truth;
        switch (code)
          { case LT_EXPR: truth = lt; break; case LE_EXPR: truth = lt || eq; break;
            case GT_EXPR: truth = !lt && !eq; break; case GE_EXPR: truth = !lt; break; default: return true; }
        if (truth != (e.kind == NEWTM_FLOW_TRUE)) return false;
      }
    if (bool *bad_true = invalid.get (g))
      if ((e.kind == NEWTM_FLOW_TRUE) == *bad_true) failure (s, GUARD_INVALID);
    return true;
  }
  bool merge (state &a, const state &b, gimple *g)
  {
    if (a.mode == HEALTHY && a.depth != b.depth)
      { refuse (g, "V07: incompatible relative nesting at a join or loop"); return false; }
    bool changed = merge_value (a.returned, b.returned);
    for (unsigned i = 0; i < a.values.length (); ++i) changed |= merge_value (a.values[i], b.values[i]);
    return changed;
  }
  template<class Visit> void successors (unsigned n, const state &in, Visit visit)
  {
    unsigned possible = outcomes (flow.node (n).statement, in);
    for (unsigned outcome = OK; outcome <= OTHER; outcome <<= 1)
      if (possible & outcome)
        {
          state out (variables.length ()); transfer (n, in, out, outcome);
          for (const newtm_flow_edge &e : flow.node (n).successors)
            { state next (variables.length ()); next.copy (out);
              if (branch (flow.node (n).statement, e, next)) visit (e.target * MODES + next.mode, next); }
        }
  }
  bool terminates (auto_vec<state *> &inputs)
  {
    unsigned total = inputs.length (), count = 0;
    auto_vec<unsigned> head, degree, work; auto_vec<arc> edges;
    head.safe_grow (total); degree.safe_grow_cleared (total);
    for (unsigned i = 0; i < total; ++i) head[i] = UINT_MAX;
    for (unsigned i = 0; i < total; ++i)
      if (inputs[i] && i % MODES != HEALTHY)
        {
          ++count;
          successors (i / MODES, *inputs[i], [&] (unsigned to, const state &)
            { if (to % MODES != HEALTHY && inputs[to])
                { arc a = { to, head[i] }; head[i] = edges.length (); edges.safe_push (a); ++degree[to]; } });
        }
    for (unsigned i = 0; i < total; ++i) if (inputs[i] && i % MODES != HEALTHY && !degree[i]) work.safe_push (i);
    for (unsigned at = 0; at < work.length (); ++at)
      for (unsigned e = head[work[at]]; e != UINT_MAX; e = edges[e].next)
        if (!--degree[edges[e].target]) work.safe_push (edges[e].target);
    if (work.length () == count) return true;
    for (unsigned i = 0; i < total; ++i) if (inputs[i] && degree[i])
      return refuse (flow.node (i / MODES).statement, "V07: failure path can cycle instead of returning through its epilogue");
    return false;
  }
  bool verify ()
  {
    auto_vec<state *> inputs; inputs.safe_grow_cleared (flow.node_count () * MODES);
    auto_vec<unsigned char> queued; queued.safe_grow_cleared (inputs.length ());
    auto_vec<unsigned> work; work.safe_push (0); queued[0] = 1;
    inputs[0] = new state (variables.length ());
    for (unsigned at = 0; at < work.length () && !reason; ++at)
      {
        unsigned id = work[at]; queued[id] = 0;
        /* Safety first also avoids applying a normal depth transition to a
           failed state. Must-facts can only lose precision at later merges. */
        if (!inspect (id / MODES, *inputs[id])) break;
        successors (id / MODES, *inputs[id], [&] (unsigned to, const state &next)
          {
            bool changed;
            if (!inputs[to]) { inputs[to] = new state (variables.length ()); inputs[to]->copy (next); changed = true; }
            else changed = merge (*inputs[to], next, flow.node (to / MODES).statement);
            if (changed && !queued[to]) { queued[to] = 1; work.safe_push (to); }
          });
      }
    if (!reason) terminates (inputs);
    if (reason && flag_transaction_debug)
      { fprintf (stderr, "NEWTM failure: %s\n", reason); if (bad) print_gimple_stmt (stderr, bad, 0, TDF_NONE); }
    for (state *s : inputs) delete s;
    return !reason;
  }
};
newtm_failure_analysis::newtm_failure_analysis (const newtm_flow_graph &f, tree ctx) : m_builder (new newtm_failure_builder (f, ctx)) {}
newtm_failure_analysis::~newtm_failure_analysis () { delete m_builder; }
void newtm_failure_analysis::invalid (gimple *g, bool bad_true) { m_builder->invalid.put (g, bad_true); }
bool newtm_failure_analysis::verify (const char **why, gimple **bad)
{
  bool ok = m_builder->verify ();
  if (why) *why = m_builder->reason;
  if (bad) *bad = m_builder->bad;
  return ok;
}
