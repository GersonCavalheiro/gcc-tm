/* Guard facts belong to reaching values, and are killed by redefinitions.
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
#include "fold-const.h"
#include "bitmap.h"
#include "internal-fn.h"
#include "newtm-flow.h"
#include "newtm-guard.h"
#include "newtm-descriptor.h"
#include "options.h"
#include "tree-pretty-print.h"
#include "gimple-pretty-print.h"

static GTY(()) tree guard_roots;
namespace {
typedef int_hash<unsigned, UINT_MAX> index_hash;
struct tree_pointer_hash : pointer_hash<tree_node>, typed_noop_remove<tree> {};
typedef pair_hash<tree_pointer_hash, index_hash> expression_hash;
struct expression
{
  tree value;
  auto_vec<tree> dependencies;
};
static bool equal (tree a, tree b)
{ return a == b || (a && b && operand_equal_p (a, b, 0)); }
static void dependency (expression &e, tree t)
{
  for (tree old : e.dependencies) if (old == t) return;
  e.dependencies.safe_push (t);
}
static void merge (expression &e, const expression &other)
{ for (tree t : other.dependencies) dependency (e, t); }
static bool variable (tree t)
{ return t && (VAR_P (t) || TREE_CODE (t) == PARM_DECL || TREE_CODE (t) == RESULT_DECL || TREE_CODE (t) == SSA_NAME); }
static bool escaped (tree t)
{
  return DECL_P (t) && (TREE_ADDRESSABLE (t) || TREE_STATIC (t) || DECL_EXTERNAL (t)
    || !DECL_CONTEXT (t) || TREE_CODE (DECL_CONTEXT (t)) != FUNCTION_DECL);
}
}

struct newtm_guard_builder
{
  const newtm_flow_graph &flow;
  bitmap_obstack storage;
  auto_vec<bitmap> dominators;
  hash_map<tree, gimple *> definitions;
  hash_set<tree> exposed;
  hash_map<expression_hash, expression *> expressions;
  auto_vec<expression *> allocated;
  tree anchor;
  const char *reason = NULL;
  gimple *bad = NULL;
  newtm_guard_builder (const newtm_flow_graph &f) : flow (f)
  {
    anchor = tree_cons (NULL_TREE, NULL_TREE, guard_roots); guard_roots = anchor;
    bitmap_obstack_initialize (&storage);
    bitmap all = bitmap_alloc (&storage);
    for (unsigned i = 0; i < f.node_count (); ++i)
      {
        bitmap d = bitmap_alloc (&storage); dominators.safe_push (d);
        if (!f.node (i).reachable) continue;
        bitmap_set_bit (all, i);
        gimple *s = f.node (i).statement;
        if (s)
          {
            struct scan {
              static tree visit (tree *p, int *walk, void *data)
              {
                walk_stmt_info *wi = (walk_stmt_info *) data;
                hash_set<tree> *seen = (hash_set<tree> *) wi->info;
                if (TREE_CODE (*p) == ADDR_EXPR)
                  { tree base = get_base_address (TREE_OPERAND (*p, 0)); if (base && DECL_P (base)) seen->add (base); }
                if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
                return NULL_TREE;
              }
            };
            walk_stmt_info wi = {}; wi.info = &exposed; walk_gimple_op (s, scan::visit, &wi);
          }
        tree lhs = s && is_gimple_assign (s) ? gimple_assign_lhs (s)
          : s && is_gimple_call (s) ? gimple_call_lhs (s) : NULL_TREE;
        if (lhs && variable (lhs))
          {
            gimple **old = definitions.get (lhs);
            if (!old) definitions.put (lhs, s);
            else if (*old != s) *old = NULL;
          }
      }
    for (unsigned i = 1; i < f.node_count (); ++i)
      if (f.node (i).reachable) bitmap_copy (dominators[i], all);
    bitmap_set_bit (dominators[0], 0);
    bitmap tmp = bitmap_alloc (&storage);
    bool changed;
    do
      {
        changed = false;
        for (unsigned i = 1; i < f.node_count (); ++i)
          if (f.node (i).reachable)
            {
              bitmap_copy (tmp, all);
              for (unsigned p : f.node (i).predecessors) bitmap_and_into (tmp, dominators[p]);
              bitmap_set_bit (tmp, i);
              if (!bitmap_equal_p (tmp, dominators[i]))
                { bitmap_copy (dominators[i], tmp); changed = true; }
            }
      }
    while (changed);
  }
  ~newtm_guard_builder ()
  {
    for (expression *e : allocated) delete e;
    bitmap_obstack_release (&storage);
    tree *p = &guard_roots;
    while (*p && *p != anchor) p = &TREE_CHAIN (*p);
    if (*p) *p = TREE_CHAIN (anchor);
  }
  tree keep (tree t)
  {
    /* LTO can merge a source boolean type distinct from boolean_type_node.
       Folding then represents its 0/1 comparison as a boolean COND_EXPR.
       Preserve the predicate instead of treating that conversion as opaque. */
    if (t && TREE_CODE (t) == COND_EXPR && TREE_CODE (TREE_TYPE (t)) == BOOLEAN_TYPE
        && integer_onep (TREE_OPERAND (t, 1)) && integer_zerop (TREE_OPERAND (t, 2)))
      t = TREE_OPERAND (t, 0);
    if (t) TREE_VALUE (anchor) = tree_cons (NULL_TREE, t, TREE_VALUE (anchor));
    return t;
  }
  tree unary (tree_code code, tree type, tree a)
  { return keep (fold_build1 (code, type, a)); }
  tree binary (tree_code code, tree type, tree a, tree b)
  { return keep (fold_build2 (code, type, a, b)); }
  bool refuse (gimple *s, const char *why)
  { if (!reason) { reason = why; bad = s; } return false; }
  unsigned definition_node (gimple *s, unsigned use)
  {
    for (unsigned i = 0; i < flow.node_count (); ++i)
      if (i != use && flow.node (i).statement == s && bitmap_bit_p (dominators[use], i)) return i;
    return UINT_MAX;
  }
  gimple *reaching_definition (tree t, unsigned use)
  {
    gimple **known = definitions.get (t);
    if (known && *known) return *known;
    expression value; value.value = t; dependency (value, t);
    auto_vec<unsigned char> seen; seen.safe_grow_cleared (flow.node_count ());
    auto_vec<unsigned> work;
    for (unsigned p : flow.node (use).predecessors) { seen[p] = 1; work.safe_push (p); }
    gimple *candidate = NULL;
    for (unsigned at = 0; at < work.length (); ++at)
      {
        unsigned n = work[at]; gimple *s = flow.node (n).statement;
        tree lhs = s && is_gimple_assign (s) ? gimple_assign_lhs (s)
          : s && is_gimple_call (s) ? gimple_call_lhs (s) : NULL_TREE;
        if (lhs == t)
          { if (candidate && candidate != s) return NULL; candidate = s; continue; }
        if (!n || writes (s, value)) return NULL;
        for (unsigned p : flow.node (n).predecessors)
          if (!seen[p]) { seen[p] = 1; work.safe_push (p); }
      }
    return candidate;
  }
  bool writes (gimple *s, const expression &e)
  {
    if (!s) return false;
    tree lhs = is_gimple_assign (s) ? gimple_assign_lhs (s)
      : is_gimple_call (s) ? gimple_call_lhs (s) : NULL_TREE;
    for (tree t : e.dependencies)
      if (lhs == t) return true;
    if (lhs && VAR_P (lhs) && (TREE_STATIC (lhs) || DECL_EXTERNAL (lhs)))
      for (tree t : e.dependencies)
        if (VAR_P (t) && (TREE_STATIC (t) || DECL_EXTERNAL (t))) return true;
    if (lhs && REFERENCE_CLASS_P (lhs))
      {
        tree base = get_base_address (lhs);
        for (tree t : e.dependencies)
          if (escaped (t) || exposed.contains (t))
            {
              bool local_base = base && VAR_P (base) && !TREE_STATIC (base) && !DECL_EXTERNAL (base)
                && DECL_CONTEXT (base) && TREE_CODE (DECL_CONTEXT (base)) == FUNCTION_DECL;
              if (!(local_base && base != t && DECL_P (t) && !TREE_STATIC (t) && !DECL_EXTERNAL (t)
                    && DECL_CONTEXT (base) == DECL_CONTEXT (t))) return true;
            }
      }
    if (!is_gimple_call (s)) return false;
    if (gimple_call_internal_p (s)) return false;
    tree fn = gimple_call_fndecl (s);
    if (fn && fndecl_built_in_p (fn, BUILT_IN_NORMAL))
      switch (DECL_FUNCTION_CODE (fn))
        {
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END: return false;
        default: break;
        }
    for (tree t : e.dependencies)
      if (escaped (t) || exposed.contains (t)) return true;
    return false;
  }
  /* Do not expand an old captured boolean as if it observed today's operand.
     Reexecuting its definition on a back edge starts a new capture. */
  bool fresh (gimple *definition, unsigned start, unsigned use, const expression &e)
  {
    auto_vec<unsigned char> seen; seen.safe_grow_cleared (flow.node_count () * 2);
    auto_vec<unsigned> work;
    for (const newtm_flow_edge &edge : flow.node (start).successors)
      { unsigned key = edge.target * 2; if (!seen[key]) { seen[key] = 1; work.safe_push (key); } }
    for (unsigned at = 0; at < work.length (); ++at)
      {
        unsigned id = work[at] / 2; bool dirty = work[at] & 1;
        if (id == use && dirty) return false;
        gimple *s = flow.node (id).statement;
        if (s == definition) dirty = false;
        else if (writes (s, e)) dirty = true;
        for (const newtm_flow_edge &edge : flow.node (id).successors)
          { unsigned key = edge.target * 2 + dirty;
            if (!seen[key]) { seen[key] = 1; work.safe_push (key); } }
      }
    return true;
  }
  void address_values (tree ref, expression &out)
  {
    if (!ref || DECL_P (ref) || TREE_CODE (ref) == STRING_CST) return;
    if (TREE_CODE (ref) == ARRAY_REF)
      { values_of (TREE_OPERAND (ref, 1), out); address_values (TREE_OPERAND (ref, 0), out); }
    else if (TREE_CODE (ref) == COMPONENT_REF) address_values (TREE_OPERAND (ref, 0), out);
    else if (TREE_CODE (ref) == MEM_REF || TREE_CODE (ref) == INDIRECT_REF)
      { values_of (TREE_OPERAND (ref, 0), out);
        if (TREE_CODE (ref) == MEM_REF) values_of (TREE_OPERAND (ref, 1), out); }
  }
  void values_of (tree root, expression &out)
  {
    auto_vec<tree> work; hash_set<tree> seen; if (root) work.safe_push (root);
    for (unsigned i = 0; i < work.length (); ++i)
      {
        tree t = work[i]; if (seen.add (t)) continue;
        if (variable (t)) dependency (out, t);
        else if (TREE_CODE (t) == ADDR_EXPR) address_values (TREE_OPERAND (t, 0), out);
        else if (EXPR_P (t))
          for (int n = 0; n < TREE_OPERAND_LENGTH (t); ++n)
            if (TREE_OPERAND (t, n)) work.safe_push (TREE_OPERAND (t, n));
      }
  }
  expression *expand (tree t, unsigned use, unsigned depth = 0)
  {
    std::pair<tree, unsigned> key (t, use);
    if (expression **old = expressions.get (key)) return *old;
    expression *out = new expression; out->value = t;
    allocated.safe_push (out); expressions.put (key, out);
    if (!t) return out;
    if (depth > 64) { values_of (t, *out); return out; }
    if (variable (t))
      {
        dependency (*out, t);
        gimple *def = reaching_definition (t, use);
        if (!def || !is_gimple_assign (def)) return out;
        unsigned at = definition_node (def, use);
        if (at == UINT_MAX) return out;
        gimple *s = def;
        tree_code code = gimple_assign_rhs_code (s);
        /* Loads are value snapshots, not fresh reads of an aliased object. */
        if (gimple_assign_single_p (s) && REFERENCE_CLASS_P (gimple_assign_rhs1 (s))
            && code != REALPART_EXPR && code != IMAGPART_EXPR) return out;
        if (code == POINTER_PLUS_EXPR || code == CONSTRUCTOR || gimple_clobber_p (s)) return out;
        expression rhs; rhs.value = NULL_TREE;
        expression *a = expand (gimple_assign_rhs1 (s), at, depth + 1); merge (rhs, *a);
        if (gimple_assign_rhs_class (s) == GIMPLE_SINGLE_RHS) rhs.value = a->value;
        else if (gimple_assign_rhs_class (s) == GIMPLE_UNARY_RHS)
          rhs.value = unary (code, TREE_TYPE (t), a->value);
        else if (gimple_assign_rhs_class (s) == GIMPLE_BINARY_RHS)
          { expression *b = expand (gimple_assign_rhs2 (s), at, depth + 1); merge (rhs, *b);
            rhs.value = binary (code, TREE_TYPE (t), a->value, b->value); }
        if (rhs.value && TREE_CODE (rhs.value) == INTEGER_CST) rhs.dependencies.truncate (0);
        dependency (rhs, t); /* Includes writes through an escaped address. */
        if (rhs.value && fresh (s, at, use, rhs))
          { out->value = rhs.value; merge (*out, rhs); }
        return out;
      }
    if (TREE_CODE (t) == ADDR_EXPR)
      {
        /* Taking an object's address does not depend on its contents. */
        address_values (TREE_OPERAND (t, 0), *out);
        return out;
      }
    if (EXPR_P (t) && TREE_CODE_CLASS (TREE_CODE (t)) != tcc_statement)
      {
        unsigned n = TREE_OPERAND_LENGTH (t);
        if (n == 1)
          { expression *a = expand (TREE_OPERAND (t, 0), use, depth + 1); merge (*out, *a);
            out->value = unary (TREE_CODE (t), TREE_TYPE (t), a->value); }
        else if (n == 2 && !REFERENCE_CLASS_P (t))
          { expression *a = expand (TREE_OPERAND (t, 0), use, depth + 1);
            expression *b = expand (TREE_OPERAND (t, 1), use, depth + 1);
            merge (*out, *a); merge (*out, *b);
            out->value = binary (TREE_CODE (t), TREE_TYPE (t), a->value, b->value); }
      }
    return out;
  }
  tree bound (tree t, bool upper, unsigned depth = 0)
  {
    if (!t || depth > 24 || !INTEGRAL_TYPE_P (TREE_TYPE (t))) return NULL_TREE;
    if (TREE_CODE (t) == INTEGER_CST) return t;
    tree type = TREE_TYPE (t), result = upper ? TYPE_MAX_VALUE (type) : TYPE_MIN_VALUE (type);
    if (TREE_CODE (t) == MIN_EXPR || TREE_CODE (t) == MAX_EXPR)
      {
        tree a = bound (TREE_OPERAND (t, 0), upper, depth + 1);
        tree b = bound (TREE_OPERAND (t, 1), upper, depth + 1);
        if (a && b) return binary (TREE_CODE (t), type, a, b);
      }
    if (CONVERT_EXPR_CODE_P (TREE_CODE (t)))
      {
        tree from = TREE_OPERAND (t, 0), ft = TREE_TYPE (from);
        if (INTEGRAL_TYPE_P (ft) && TYPE_UNSIGNED (ft) && TYPE_PRECISION (type) >= TYPE_PRECISION (ft)
            && (TYPE_UNSIGNED (type) || TYPE_PRECISION (type) > TYPE_PRECISION (ft)))
          { tree b = bound (from, upper, depth + 1); if (b) return keep (fold_convert (type, b)); }
        if (POINTER_TYPE_P (ft) && TYPE_PRECISION (type) >= TYPE_PRECISION (ft))
          return upper ? keep (fold_convert (type, TYPE_MAX_VALUE (unsigned_type_for (pointer_sized_int_node))))
                       : build_zero_cst (type);
      }
    return result;
  }
  bool statically_true (tree t)
  {
    if (TREE_CODE (t) == INTEGER_CST) return !integer_zerop (t);
    if (TREE_CODE (t) == LE_EXPR || TREE_CODE (t) == LT_EXPR)
      {
        tree a = TREE_OPERAND (t, 0), b = TREE_OPERAND (t, 1);
        if (TREE_CODE (t) == LE_EXPR
            && ((TREE_CODE (a) == MIN_EXPR && (equal (TREE_OPERAND (a, 0), b) || equal (TREE_OPERAND (a, 1), b)))
                || (TREE_CODE (b) == MAX_EXPR && (equal (a, TREE_OPERAND (b, 0)) || equal (a, TREE_OPERAND (b, 1)))))) return true;
        tree hi = bound (a, true), lo = bound (b, false);
        if (hi && lo && TREE_CODE (hi) == INTEGER_CST && TREE_CODE (lo) == INTEGER_CST)
          return TREE_CODE (t) == LE_EXPR ? !tree_int_cst_lt (lo, hi) : tree_int_cst_lt (hi, lo);
      }
    return false;
  }
  bool implies (tree fact, bool truth, tree need, unsigned depth = 0)
  {
    if (!fact || depth > 64) return false;
    fact = canonical (fact); need = canonical (need);
    if (truth && equal (fact, need)) return true;
    tree_code code = TREE_CODE (fact);
    /* Folding may replace signed nonnegativity plus an upper bound by an
       unsigned range comparison. Only a non-narrowing conversion and a bound
       within the source positive range justify the interval [0, upper]. */
    if (truth && (code == LT_EXPR || code == LE_EXPR)
        && CONVERT_EXPR_CODE_P (TREE_CODE (TREE_OPERAND (fact, 0)))
        && TREE_CODE (TREE_OPERAND (fact, 1)) == INTEGER_CST)
      {
        tree converted = TREE_OPERAND (fact, 0), source = TREE_OPERAND (converted, 0);
        tree ct = TREE_TYPE (converted), st = TREE_TYPE (source), upper = TREE_OPERAND (fact, 1);
        if (INTEGRAL_TYPE_P (ct) && TYPE_UNSIGNED (ct) && INTEGRAL_TYPE_P (st)
            && TYPE_PRECISION (ct) >= TYPE_PRECISION (st)
            && !(code == LT_EXPR && integer_zerop (upper)))
          {
            if (code == LT_EXPR) upper = binary (MINUS_EXPR, ct, upper, build_int_cst (ct, 1));
            if (!tree_int_cst_lt (fold_convert (ct, TYPE_MAX_VALUE (st)), upper)
                && TREE_CODE_CLASS (TREE_CODE (need)) == tcc_comparison
                && TREE_CODE (TREE_OPERAND (need, 1)) == INTEGER_CST)
              {
                tree lhs = TREE_OPERAND (need, 0); bool fits = true;
                while (CONVERT_EXPR_CODE_P (TREE_CODE (lhs)))
                  {
                    tree t = TREE_TYPE (lhs);
                    tree wide = build_nonstandard_integer_type (MAX (TYPE_PRECISION (ct), TYPE_PRECISION (t)) + 1, false);
                    if (!INTEGRAL_TYPE_P (t) || tree_int_cst_lt (fold_convert (wide, TYPE_MAX_VALUE (t)), fold_convert (wide, upper))) fits = false;
                    lhs = TREE_OPERAND (lhs, 0);
                  }
                if (fits && equal (lhs, source) && TREE_CODE (need) != NE_EXPR)
                  {
                    tree t = TREE_TYPE (TREE_OPERAND (need, 0));
                    tree lo = binary (TREE_CODE (need), boolean_type_node, build_zero_cst (t), TREE_OPERAND (need, 1));
                    tree hi = binary (TREE_CODE (need), boolean_type_node, keep (fold_convert (t, upper)), TREE_OPERAND (need, 1));
                    if (TREE_CODE (lo) == INTEGER_CST && !integer_zerop (lo)
                        && TREE_CODE (hi) == INTEGER_CST && !integer_zerop (hi)) return true;
                  }
              }
          }
      }
    if (truth && TREE_CODE_CLASS (code) == tcc_comparison && TREE_CODE_CLASS (TREE_CODE (need)) == tcc_comparison
        && equal (TREE_OPERAND (fact, 0), TREE_OPERAND (need, 0))
        && TREE_CODE (TREE_OPERAND (fact, 1)) == INTEGER_CST && TREE_CODE (TREE_OPERAND (need, 1)) == INTEGER_CST)
      {
        tree a = TREE_OPERAND (fact, 1), b = TREE_OPERAND (need, 1); tree_code nc = TREE_CODE (need);
        if ((code == LT_EXPR || code == LE_EXPR) && (nc == LT_EXPR || nc == LE_EXPR))
          if (tree_int_cst_lt (a, b) || (equal (a, b) && (code == LT_EXPR || nc == LE_EXPR))) return true;
        if ((code == GT_EXPR || code == GE_EXPR) && (nc == GT_EXPR || nc == GE_EXPR))
          if (tree_int_cst_lt (b, a) || (equal (a, b) && (code == GT_EXPR || nc == GE_EXPR))) return true;
      }
    if (truth && code == LT_EXPR && TREE_CODE (need) == LE_EXPR
        && equal (TREE_OPERAND (fact, 0), TREE_OPERAND (need, 0))
        && equal (TREE_OPERAND (fact, 1), TREE_OPERAND (need, 1))) return true;
    if (code == TRUTH_NOT_EXPR) return implies (TREE_OPERAND (fact, 0), !truth, need, depth + 1);
    if (code == BIT_AND_EXPR || code == TRUTH_AND_EXPR || code == TRUTH_ANDIF_EXPR
        || code == BIT_IOR_EXPR || code == TRUTH_OR_EXPR || code == TRUTH_ORIF_EXPR)
      {
        bool a = implies (TREE_OPERAND (fact, 0), truth, need, depth + 1);
        bool b = implies (TREE_OPERAND (fact, 1), truth, need, depth + 1);
        bool conjunction = code == BIT_AND_EXPR || code == TRUTH_AND_EXPR || code == TRUTH_ANDIF_EXPR;
        if (code == BIT_AND_EXPR && !truth
            && (TREE_CODE (TREE_TYPE (TREE_OPERAND (fact, 0))) != BOOLEAN_TYPE
                || TREE_CODE (TREE_TYPE (TREE_OPERAND (fact, 1))) != BOOLEAN_TYPE)) return false;
        return conjunction == truth ? a || b : a && b;
      }
    if (code == EQ_EXPR || code == NE_EXPR)
      {
        tree a = TREE_OPERAND (fact, 0), b = TREE_OPERAND (fact, 1);
        if (integer_zerop (b))
          {
            bool nonzero = truth == (code == NE_EXPR);
            /* Folding combines independent zero-overflow tests as
               (flag_a | flag_b) == 0. Both flags are then zero. */
            if (!nonzero && TREE_CODE (a) == BIT_IOR_EXPR)
              for (unsigned i = 0; i < 2; ++i)
                {
                  tree operand = TREE_OPERAND (a, i);
                  tree zero = binary (EQ_EXPR, boolean_type_node, operand, build_zero_cst (TREE_TYPE (operand)));
                  if (implies (zero, true, need, depth + 1)) return true;
                }
            if (TREE_CODE (TREE_TYPE (a)) == BOOLEAN_TYPE || TREE_CODE_CLASS (TREE_CODE (a)) == tcc_comparison)
              if (implies (a, nonzero, need, depth + 1)) return true;
            if (nonzero && TREE_CODE (need) == LE_EXPR && integer_onep (TREE_OPERAND (need, 0))
                && equal (a, TREE_OPERAND (need, 1)) && INTEGRAL_TYPE_P (TREE_TYPE (a)) && TYPE_UNSIGNED (TREE_TYPE (a))) return true;
          }
      }
    if (!truth && TREE_CODE_CLASS (code) == tcc_comparison)
      {
        tree inverse = binary (invert_tree_comparison (code, false), boolean_type_node,
                               TREE_OPERAND (fact, 0), TREE_OPERAND (fact, 1));
        return equal (inverse, need);
      }
    return false;
  }
  tree canonical (tree t)
  {
    tree_code code = TREE_CODE (t);
    if (TREE_CODE_CLASS (code) == tcc_comparison && TREE_CODE (TREE_OPERAND (t, 0)) == BIT_NOT_EXPR
        && TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (t, 0))) && TREE_CODE (TREE_OPERAND (t, 1)) == INTEGER_CST)
      {
        tree a = TREE_OPERAND (t, 0), b = TREE_OPERAND (t, 1);
        tree_code inverse = code == LT_EXPR ? GT_EXPR : code == LE_EXPR ? GE_EXPR
          : code == GT_EXPR ? LT_EXPR : code == GE_EXPR ? LE_EXPR : code;
        return binary (inverse, boolean_type_node, TREE_OPERAND (a, 0), unary (BIT_NOT_EXPR, TREE_TYPE (b), b));
      }
    return t;
  }
  bool atom (unsigned target, const expression &needed)
  {
    tree need = needed.value;
    if (statically_true (need)) return true;
    if (TREE_CODE (need) == BIT_AND_EXPR || TREE_CODE (need) == TRUTH_AND_EXPR || TREE_CODE (need) == TRUTH_ANDIF_EXPR)
      {
        expression a, b; a.value = TREE_OPERAND (need, 0); b.value = TREE_OPERAND (need, 1);
        merge (a, needed); merge (b, needed); return atom (target, a) && atom (target, b);
      }
    auto_vec<unsigned char> seen; seen.safe_grow_cleared (flow.node_count () * 2);
    expression values; values.value = need;
    values_of (need, values);
    auto_vec<unsigned> work; work.safe_push (0); seen[0] = 1;
    for (unsigned at = 0; at < work.length (); ++at)
      {
        unsigned id = work[at] / 2; bool proved = work[at] & 1;
        if (id == target && !proved)
          {
            if (flag_transaction_debug)
              { fputs ("NEWTM unproved atom: ", stderr); print_generic_expr (stderr, need, TDF_NONE); fputc ('\n', stderr);
                for (tree t : values.dependencies)
                  { gimple **d = definitions.get (t); print_generic_expr (stderr, t, TDF_NONE);
                    fprintf (stderr, " definition=%s dominator=%u\n", !d ? "absent" : !*d ? "multiple" : "unique",
                             d && *d ? definition_node (*d, target) : UINT_MAX); }
                for (unsigned n = 0; n < flow.node_count (); ++n)
                  if (gimple *s = flow.node (n).statement)
                    if (gimple_code (s) == GIMPLE_COND)
                      { gcond *c = as_a<gcond *> (s);
                        tree p = binary (gimple_cond_code (c), boolean_type_node, gimple_cond_lhs (c), gimple_cond_rhs (c));
                        fprintf (stderr, "NEWTM candidate %u: ", n);
                        print_generic_expr (stderr, expand (p, n)->value, TDF_NONE); fputc ('\n', stderr); } }
            return false;
          }
        gimple *s = flow.node (id).statement;
        if (writes (s, values)) proved = false;
        tree condition = NULL_TREE;
        if (s && gimple_code (s) == GIMPLE_COND)
          {
            gcond *c = as_a<gcond *> (s);
            tree predicate = binary (gimple_cond_code (c), boolean_type_node, gimple_cond_lhs (c), gimple_cond_rhs (c));
            condition = expand (predicate, id)->value;
          }
        for (const newtm_flow_edge &edge : flow.node (id).successors)
          {
            bool next = proved;
            if (condition && (edge.kind == NEWTM_FLOW_TRUE || edge.kind == NEWTM_FLOW_FALSE))
              {
                bool truth = edge.kind == NEWTM_FLOW_TRUE;
                if (TREE_CODE (condition) == INTEGER_CST && truth == bool (integer_zerop (condition))) continue;
                if (implies (condition, truth, need)) next = true;
              }
            unsigned key = edge.target * 2 + next;
            if (!seen[key]) { seen[key] = 1; work.safe_push (key); }
          }
      }
    return true;
  }
  bool require (gimple *s, tree predicate)
  {
    if (!predicate) return refuse (s, "V03: missing mathematical guard requirement");
    keep (predicate);
    for (unsigned i = 0; i < flow.node_count (); ++i)
      if (flow.node (i).reachable && flow.node (i).statement == s)
        if (!atom (i, *expand (predicate, i)))
          {
            if (flag_transaction_debug)
              { fprintf (stderr, "NEWTM missing guard at flow node %u: ", i);
                print_generic_expr (stderr, expand (predicate, i)->value, TDF_NONE);
                fputc ('\n', stderr); print_gimple_stmt (stderr, s, 0, TDF_NONE); }
            return refuse (s, "V03: validity or arithmetic guard is not established on every reaching path");
          }
    return true;
  }
};

newtm_guard_analysis::newtm_guard_analysis (const newtm_flow_graph &f) : m_builder (new newtm_guard_builder (f)) {}
newtm_guard_analysis::~newtm_guard_analysis () { delete m_builder; }
bool newtm_guard_analysis::address_exposed (tree t) const
{ return escaped (t) || m_builder->exposed.contains (t); }
bool newtm_guard_analysis::require (gimple *s, tree predicate, const char **why, gimple **bad)
{
  bool ok = m_builder->require (s, predicate);
  if (why) *why = m_builder->reason;
  if (bad) *bad = m_builder->bad;
  return ok;
}
gimple *newtm_guard_analysis::definition_at (gimple *use, tree value)
{
  for (unsigned i = 0; i < m_builder->flow.node_count (); ++i)
    if (m_builder->flow.node (i).reachable && m_builder->flow.node (i).statement == use)
      {
        gimple *def = m_builder->reaching_definition (value, i);
        if (!def) return NULL;
        unsigned at = m_builder->definition_node (def, i);
        expression facts; facts.value = value;
        dependency (facts, value);
        return at != UINT_MAX && m_builder->fresh (def, at, i, facts) ? def : NULL;
      }
  return NULL;
}
bool newtm_guard_analysis::arithmetic (gimple *s, const char **why, gimple **bad)
{
  bool ok = true;
  if (is_gimple_assign (s))
    {
      tree lhs = gimple_assign_lhs (s), type = TREE_TYPE (lhs);
      tree_code code = gimple_assign_rhs_code (s);
      if (INTEGRAL_TYPE_P (type) && TYPE_UNSIGNED (type)
          && (code == PLUS_EXPR || code == MINUS_EXPR || code == MULT_EXPR))
        {
          tree a = gimple_assign_rhs1 (s), b = gimple_assign_rhs2 (s);
          tree needed = code == MINUS_EXPR
            ? m_builder->binary (LE_EXPR, boolean_type_node, b, a)
            : code == PLUS_EXPR
            ? m_builder->binary (LE_EXPR, boolean_type_node, b,
                m_builder->binary (MINUS_EXPR, type, TYPE_MAX_VALUE (type), a)) : NULL_TREE;
          if (!needed) ok = m_builder->refuse (s, "V03: unchecked multiplication in identity arithmetic");
          else ok = m_builder->require (s, needed);
        }
    }
  if (why) *why = m_builder->reason;
  if (bad) *bad = m_builder->bad;
  return ok;
}
bool newtm_guard_analysis::overflow_dependencies (gimple *s, tree value, const char **why, gimple **bad)
{
  auto_vec<tree> work; hash_set<tree> seen; work.safe_push (value);
  bool ok = true;
  for (unsigned at = 0; at < work.length () && ok; ++at)
    {
      tree t = work[at]; if (!t || seen.add (t)) continue;
      gimple **def = m_builder->definitions.get (t);
      if (!def || !*def || !is_gimple_assign (*def)) continue;
      gimple *d = *def;
      if (gimple_assign_cast_p (d) && newtm_descriptor_math_p (d))
        {
          tree from = gimple_assign_rhs1 (d), it = TREE_TYPE (from), ot = TREE_TYPE (gimple_assign_lhs (d));
          if (INTEGRAL_TYPE_P (it) && INTEGRAL_TYPE_P (ot))
            {
              tree wide = build_nonstandard_integer_type (MAX (TYPE_PRECISION (it), TYPE_PRECISION (ot)) + 1, false);
              tree imin = fold_convert (wide, TYPE_MIN_VALUE (it)), imax = fold_convert (wide, TYPE_MAX_VALUE (it));
              tree omin = fold_convert (wide, TYPE_MIN_VALUE (ot)), omax = fold_convert (wide, TYPE_MAX_VALUE (ot));
              if (tree_int_cst_lt (imin, omin))
                ok = m_builder->require (s, m_builder->binary (GE_EXPR, boolean_type_node, from, fold_convert (it, TYPE_MIN_VALUE (ot))));
              if (ok && tree_int_cst_lt (omax, imax))
                ok = m_builder->require (s, m_builder->binary (LE_EXPR, boolean_type_node, from, fold_convert (it, TYPE_MAX_VALUE (ot))));
            }
        }
      if (gimple_assign_rhs_code (d) == REALPART_EXPR)
        {
          tree pair = TREE_OPERAND (gimple_assign_rhs1 (d), 0);
          gimple **call = m_builder->definitions.get (pair);
          if (call && *call && is_gimple_call (*call) && gimple_call_internal_p (*call))
            {
              internal_fn f = gimple_call_internal_fn (*call);
              if (f == IFN_ADD_OVERFLOW || f == IFN_SUB_OVERFLOW || f == IFN_MUL_OVERFLOW)
                {
                  tree type = TREE_TYPE (TREE_TYPE (pair));
                  tree flag = m_builder->unary (IMAGPART_EXPR, type, pair);
                  ok = m_builder->require (s, m_builder->binary (EQ_EXPR, boolean_type_node, flag, build_zero_cst (type)));
                  for (unsigned i = 0; i < gimple_call_num_args (*call); ++i) work.safe_push (gimple_call_arg (*call, i));
                }
            }
        }
      for (unsigned i = 1; i < gimple_num_ops (d); ++i) work.safe_push (gimple_op (d, i));
    }
  if (why) *why = m_builder->reason;
  if (bad) *bad = m_builder->bad;
  return ok;
}
#include "gt-newtm-guard.h"
