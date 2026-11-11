/* Independent memory proofs on current declaration CFGs.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "fold-const.h"
#include "diagnostic-core.h"
#include "internal-fn.h"
#include "bitmap.h"
#include "tree-pretty-print.h"
#include "gimple-pretty-print.h"
#include "newtm-abi.h"
#include "newtm-clone.h"
#include "newtm-lto.h"
#include "newtm-current.h"
#include "newtm-flow.h"
#include "newtm-guard.h"
#include "newtm-obligation.h"
#include "newtm-memory.h"
#include "newtm-lifetime.h"
#include "newtm-status.h"
#include "newtm-failure.h"
#include "newtm-owner.h"
#include "tree-dfa.h"

/* Pending operands are rooted only during preparation.  Persistent records
   contain declarations and scalar proof terms, never a GIMPLE pointer or a
   descriptor.  Terms have no TREE expression operators and cannot be emitted
   as executable access plans. */
static GTY(()) hash_map<gimple *, tree> *pending_memory;
static GTY(()) hash_map<tree, tree> *memory_contracts;
static GTY(()) hash_map<tree, tree> *memory_results;
static GTY(()) hash_map<tree, tree> *memory_arithmetic;
static GTY(()) hash_map<tree, tree> *channel_contracts;
static GTY(()) hash_map<tree, tree> *channel_publications;
static GTY(()) hash_map<tree, tree> *channel_entries;
static GTY(()) hash_map<tree, tree> *channel_results;
static GTY(()) hash_map<tree, tree> *status_contracts;
static GTY(()) hash_map<tree, tree> *status_publications;
static GTY(()) hash_map<tree, tree> *status_guards;
static GTY(()) hash_map<tree, tree> *status_results;
static GTY(()) hash_map<tree, tree> *owner_entries;
static GTY(()) hash_map<tree, tree> *owner_results;
static GTY(()) hash_map<tree, tree> *coverage_contracts;
static GTY(()) hash_map<tree, tree> *coverage_results;
static GTY(()) hash_map<tree, tree> *coverage_entries;
static GTY(()) hash_map<tree, tree> *coverage_entry_results;

namespace {
enum term_kind { TERM_DECL = LAST_AND_UNUSED_TREE_CODE + 1, TERM_INPUT,
                 TERM_LOAD, TERM_CALL, TERM_VERSIONS, TERM_TYPE, TERM_SWITCH,
                 TERM_CONTROL };
enum tuple_field { OBJECT, EXTENT, OFFSET, SIZE, KIND, STORAGE, LOGICAL,
                   TUPLE_FIELDS };
enum contract_field { ROLE, SITE, OPERAND, TUPLE, EXPRESSION, CONTRACT_FIELDS };
static tree number (unsigned n) { return build_int_cst (unsigned_type_node, n); }
static tree term (unsigned code, tree type, tree children)
{
  tree t = make_tree_vec (3);
  TREE_VEC_ELT (t, 0) = number (code);
  TREE_VEC_ELT (t, 1) = type;
  TREE_VEC_ELT (t, 2) = children;
  return t;
}
static tree list (tree a, tree b = NULL_TREE, tree c = NULL_TREE)
{
  tree t = make_tree_vec (3);
  TREE_VEC_ELT (t, 0) = a; TREE_VEC_ELT (t, 1) = b;
  TREE_VEC_ELT (t, 2) = c; return t;
}
/* SSA_NAME itself owns a def-statement link.  Only its declaration and scalar
   version may survive the preparation window. */
static tree symbol (tree t)
{
  if (!t) return NULL_TREE;
  if (TREE_CODE (t) == SSA_NAME)
    return term (SSA_NAME, NULL_TREE,
      list (SSA_NAME_VAR (t) ? newtm_clone_memory_origin (SSA_NAME_VAR (t)) : NULL_TREE,
            number (SSA_NAME_VERSION (t))));
  if (tree origin = newtm_lto_source_ssa (t)) return term (SSA_NAME, NULL_TREE, origin);
  return newtm_clone_memory_origin (t);
}
static bool complete (tree t)
{
  if (t == error_mark_node) return false;
  if (t && TREE_CODE (t) == TREE_VEC)
    for (int i = 0; i < TREE_VEC_LENGTH (t); ++i)
      if (!complete (TREE_VEC_ELT (t, i))) return false;
  return true;
}
static bool same (tree a, tree b)
{
  if (a == b) return true;
  if (!a || !b || TREE_CODE (a) != TREE_CODE (b)) return false;
  if (TREE_CODE (a) == INTEGER_CST) return tree_int_cst_equal (a, b);
  if (CONSTANT_CLASS_P (a)) return operand_equal_p (a, b, 0);
  if (TREE_CODE (a) != TREE_VEC || TREE_VEC_LENGTH (a) != TREE_VEC_LENGTH (b)) return false;
  if (TREE_VEC_LENGTH (a) == 3 && TREE_VEC_ELT (a, 0) && TREE_VEC_ELT (b, 0)
      && TREE_CODE (TREE_VEC_ELT (a, 0)) == INTEGER_CST
      && TREE_CODE (TREE_VEC_ELT (b, 0)) == INTEGER_CST
      && (tree_to_uhwi (TREE_VEC_ELT (a, 0)) == TERM_VERSIONS
          || tree_to_uhwi (TREE_VEC_ELT (a, 0)) == TERM_CONTROL)
      && tree_int_cst_equal (TREE_VEC_ELT (a, 0), TREE_VEC_ELT (b, 0)))
    {
      tree aa = TREE_VEC_ELT (a, 2), bb = TREE_VEC_ELT (b, 2);
      if (TREE_VEC_LENGTH (aa) != TREE_VEC_LENGTH (bb)) return false;
      for (int i = 0; i < TREE_VEC_LENGTH (aa); ++i)
        {
          bool found = false;
          for (int j = 0; j < TREE_VEC_LENGTH (bb); ++j)
            if (same (TREE_VEC_ELT (aa, i), TREE_VEC_ELT (bb, j))) found = true;
          if (!found) return false;
        }
      return true;
    }
  for (int i = 0; i < TREE_VEC_LENGTH (a); ++i)
    if (!same (TREE_VEC_ELT (a, i), TREE_VEC_ELT (b, i))) return false;
  return true;
}
static void difference (tree a, tree b, unsigned depth = 0)
{
  if (same (a, b) || depth > 100) return;
  if (a && b && TREE_CODE (a) == TREE_VEC && TREE_CODE (b) == TREE_VEC
      && TREE_VEC_LENGTH (a) == 3 && TREE_VEC_LENGTH (b) == 3
      && TREE_VEC_ELT (a, 0) && TREE_CODE (TREE_VEC_ELT (a, 0)) == INTEGER_CST
      && tree_to_uhwi (TREE_VEC_ELT (a, 0)) == TERM_VERSIONS
      && same (TREE_VEC_ELT (a, 0), TREE_VEC_ELT (b, 0)))
    {
      tree av = TREE_VEC_ELT (a, 2), bv = TREE_VEC_ELT (b, 2);
      for (int i = 1; i < TREE_VEC_LENGTH (av); ++i)
        {
          bool matched = false;
          for (int j = 1; j < TREE_VEC_LENGTH (bv); ++j)
            matched |= same (TREE_VEC_ELT (av, i), TREE_VEC_ELT (bv, j));
          if (matched) continue;
          for (int j = 1; j < TREE_VEC_LENGTH (bv); ++j)
            { fprintf (stderr, " writer[%d,%d]", i, j);
              difference (TREE_VEC_ELT (av, i), TREE_VEC_ELT (bv, j), depth + 1); }
          return;
        }
    }

  if (a && b && TREE_CODE (a) == TREE_VEC && TREE_CODE (b) == TREE_VEC
      && TREE_VEC_LENGTH (a) == TREE_VEC_LENGTH (b))
    for (int i = 0; i < TREE_VEC_LENGTH (a); ++i)
      if (!same (TREE_VEC_ELT (a, i), TREE_VEC_ELT (b, i)))
        { fprintf (stderr, "[%d]", i); difference (TREE_VEC_ELT (a, i), TREE_VEC_ELT (b, i), depth + 1); return; }
  fputs (" expected=", stderr); if (a) { fprintf (stderr, "%s ", get_tree_code_name (TREE_CODE (a))); print_generic_expr (stderr, a); }
  fputs (" actual=", stderr); if (b) { fprintf (stderr, "%s ", get_tree_code_name (TREE_CODE (b))); print_generic_expr (stderr, b); }
  fputc ('\n', stderr);
}
/* Freeze layout numbers, not pointers to mutable TYPE nodes.  Describe one
   pointee layer deeply and subsequent pointees by their allocation headers,
   so recursive pointer records do not recurse indefinitely. */
static tree layout (tree type, unsigned depth = 0, bool pointees = true)
{
  if (!type || depth > 32) return error_mark_node;
  tree fields = NULL_TREE;
  if (TREE_CODE (type) == RECORD_TYPE || TREE_CODE (type) == UNION_TYPE)
    for (tree f = TYPE_FIELDS (type); f; f = DECL_CHAIN (f))
      if (TREE_CODE (f) == FIELD_DECL)
        {
          tree offset = DECL_FIELD_OFFSET (f), bit = DECL_FIELD_BIT_OFFSET (f);
          if (!offset || !bit || TREE_CODE (offset) != INTEGER_CST
              || TREE_CODE (bit) != INTEGER_CST || !DECL_SIZE_UNIT (f)
              || TREE_CODE (DECL_SIZE_UNIT (f)) != INTEGER_CST) return error_mark_node;
          tree position = list (list (offset, bit, DECL_SIZE_UNIT (f)), number (DECL_ALIGN (f)), number (DECL_USER_ALIGN (f)));
          tree item = list (newtm_clone_memory_origin (f), position,
                            layout (TREE_TYPE (f), depth + 1, pointees));
          fields = tree_cons (NULL_TREE, item, fields);
        }
  tree size = TYPE_SIZE_UNIT (type);
  if (size && TREE_CODE (size) != INTEGER_CST) return error_mark_node;
  tree detail = fields;
  if (TREE_CODE (type) == ARRAY_TYPE || TREE_CODE (type) == COMPLEX_TYPE)
    detail = layout (TREE_TYPE (type), depth + 1, pointees);
  else if (INTEGRAL_TYPE_P (type) || POINTER_TYPE_P (type))
    detail = list (number (TYPE_PRECISION (type)), number (TYPE_UNSIGNED (type)));
  if (POINTER_TYPE_P (type))
    {
      tree target = TREE_TYPE (type);
      if (TYPE_SIZE_UNIT (target) && TREE_CODE (TYPE_SIZE_UNIT (target)) != INTEGER_CST) return error_mark_node;
      tree pointed = pointees ? layout (target, depth + 1, false)
        : term (TERM_TYPE, number (TREE_CODE (target)), list (TYPE_SIZE_UNIT (target), number (TYPE_ALIGN (target))));
      detail = list (detail, pointed);
    }
  /* Convert field lists to vectors, so structural equality never follows a
     mutable type or declaration chain. */
  if (fields)
    {
      unsigned count = list_length (fields); detail = make_tree_vec (count);
      for (unsigned i = 0; fields; fields = TREE_CHAIN (fields), ++i)
        TREE_VEC_ELT (detail, i) = TREE_VALUE (fields);
    }
  return term (TERM_TYPE, number (TREE_CODE (type)), list (size, number (TYPE_ALIGN (type)), detail));
}
static bool variable (tree t)
{ return t && (VAR_P (t) || TREE_CODE (t) == PARM_DECL || TREE_CODE (t) == SSA_NAME); }
static tree memory_operand (gimple *s, newtm_memory_role role)
{
  if (!is_gimple_assign (s)) return NULL_TREE;
  return role == NEWTM_MEMORY_PRIVATE_WRITE ? gimple_assign_lhs (s)
       : role == NEWTM_MEMORY_PRIVATE_READ ? gimple_assign_rhs1 (s) : NULL_TREE;
}

/* A resolver is per inspection.  Origin identifies a source declaration;
   instance binding separately forbids collapsing two current allocations.
   Incoming pointees retain the complete channel/slot path.  Loads include
   their reaching memory versions; a write through an unclassified alias
   cannot silently reuse an older load's identity. */
struct resolver
{
  struct tree_hash : pointer_hash<tree_node>, typed_noop_remove<tree> {};
  struct stmt_hash : pointer_hash<gimple>, typed_noop_remove<gimple *> {};
  typedef pair_hash<tree_hash, stmt_hash> use_hash;
  tree owner;
  tree proof_owner;
  tree entry_frame = NULL_TREE;
  bool lowered;
  const newtm_flow_graph &flow;
  newtm_guard_analysis guards;
  hash_map<tree, tree> instances;
  hash_map<tree, tree> current_decls;
  auto_vec<tree> ssa_decls;
  hash_map<gimple *, unsigned> positions;
  hash_map<tree, tree> type_layouts;
  hash_map<use_hash, tree> values;
  hash_map<use_hash, tree> condition_values;
  hash_map<tree, tree> frame_instances;
  bitmap_obstack control_storage;
  auto_vec<bitmap> controls;
  auto_vec<tree> predicates;
  hash_map<gimple *, unsigned> condition_indices;
  hash_map<gimple *, tree> switch_indices;
  hash_set<gimple *> inverted_conditions;
  bool controls_built = false;
  const char *failure = NULL;
  const char *failure_detail = NULL;
  newtm_local_rule failure_rule = NEWTM_V03;
  unsigned dependencies = NEWTM_MEMORY_OWNER;
  resolver (tree fn, const newtm_flow_graph &f, bool current = false)
    : owner (fn), proof_owner (fn), lowered (current), flow (f), guards (f)
  {
    bitmap_obstack_initialize (&control_storage);
    for (unsigned i = 0; i < flow.node_count (); ++i)
      if (gimple *s = flow.node (i).statement)
        {
          positions.put (s, i);
          struct scan {
            static tree visit (tree *p, int *walk, void *data)
            {
              walk_stmt_info *wi = (walk_stmt_info *) data;
              resolver *r = (resolver *) wi->info;
              if (variable (*p)) r->declare (*p);
              if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
              return NULL_TREE;
            }
          };
          walk_stmt_info wi = {}; wi.info = this;
          walk_gimple_op (s, scan::visit, &wi);
        }
    for (tree p = DECL_ARGUMENTS (fn); p; p = DECL_CHAIN (p)) declare (p);
  }
  ~resolver () { bitmap_obstack_release (&control_storage); }
  void declare (tree t)
  {
    if (TREE_CODE (t) == SSA_NAME)
      { for (tree old : ssa_decls) if (old == t) return; ssa_decls.safe_push (t); return; }
    tree origin = newtm_clone_memory_origin (t);
    if (!origin) return;
    if (tree *p = current_decls.get (origin))
      { if (*p != t) *p = error_mark_node; }
    else current_decls.put (origin, t);
  }
  tree refuse (const char *why, newtm_local_rule rule = NEWTM_V03)
  {
    if (!failure) { failure = why; failure_rule = rule; }
    return error_mark_node;
  }
  const char *failure_rule_name () const
  { return failure_rule == NEWTM_V04 ? "V04" : "V03"; }
  tree type (tree t)
  {
    if (!t) return refuse ("memory operand has no layout type");
    if (tree *p = type_layouts.get (t)) return *p;
    tree result = layout (t);
    if (!complete (result)) refuse ("memory object has an unrepresentable layout");
    type_layouts.put (t, result); return result;
  }
  tree declaration (tree t, bool logical = false)
  {
    if (tree origin = newtm_lto_source_ssa (t))
      return term (SSA_NAME, type (TREE_TYPE (t)), origin);
    if (TREE_CODE (t) == SSA_NAME)
      {
        tree var = SSA_NAME_VAR (t);
        if (lowered && var && DECL_CONTEXT (var) != owner && !TREE_STATIC (var) && !DECL_EXTERNAL (var))
          return refuse ("SSA value belongs to another activation");
        return term (SSA_NAME, type (TREE_TYPE (t)),
          list (var ? newtm_clone_memory_origin (var) : NULL_TREE, number (SSA_NAME_VERSION (t))));
      }
    tree origin = newtm_clone_memory_origin (t);
    if (!origin) return refuse ("ambiguous declaration origin");
    bool global = DECL_P (t) && (TREE_STATIC (t) || DECL_EXTERNAL (t));
    if (global && variable (t) && origin != t)
      return refuse ("global allocation cannot inherit another object's identity");
    if (tree *old = instances.get (origin))
      { if (*old != t) return refuse ("distinct storage instances share an origin"); }
    else instances.put (origin, t);
    if (variable (t) && !global && DECL_CONTEXT (t) != owner)
      {
        if (lowered && !logical) return refuse ("unmapped storage belongs to another activation");
        dependencies |= NEWTM_MEMORY_CHANNELS;
      }
    return term (TERM_DECL, type (TREE_TYPE (t)), list (origin, global ? NULL_TREE : proof_owner, number (global)));
  }
  /* A shallow writer signature terminates cycles without unrolling loops.
     It is evidence about a definition, never approval of its result. */
  tree shallow (tree t, unsigned depth = 0)
  {
    if (!t) return NULL_TREE;
    if (depth > 64) return refuse ("memory provenance exceeds proof depth");
    if (DECL_P (t) || TREE_CODE (t) == SSA_NAME) return declaration (t);
    if (TREE_CODE (t) == COMPONENT_REF)
      if (tree source = newtm_clone_memory_field (TREE_OPERAND (t, 1)))
        return integer_onep (TREE_PURPOSE (source))
          ? term (ADDR_EXPR, NULL_TREE, list (declaration (TREE_VALUE (source), true))) : declaration (TREE_VALUE (source), true);
    if (CONSTANT_CLASS_P (t)) return term (TREE_CODE (t), type (TREE_TYPE (t)), t);
    if (TREE_CODE (t) == CONSTRUCTOR)
      return constructor (t, NULL, false, depth);
    if (!EXPR_P (t))
      {
        if (flag_transaction_debug) { fprintf (stderr, "NEWTM provenance operand %s: ", get_tree_code_name (TREE_CODE (t))); print_generic_expr (stderr, t); fputc ('\n', stderr); }
        return refuse ("unclassified memory provenance operand");
      }
    tree children = make_tree_vec (TREE_OPERAND_LENGTH (t));
    for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
      TREE_VEC_ELT (children, i) = shallow (TREE_OPERAND (t, i), depth + 1);
    return term (TREE_CODE (t), type (TREE_TYPE (t)), children);
  }
  tree constructor (tree t, gimple *at, bool expand, unsigned depth)
  {
    tree elements = make_tree_vec (CONSTRUCTOR_NELTS (t) * 2);
    unsigned HOST_WIDE_INT i; tree index, v;
    FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (t), i, index, v)
      {
        TREE_VEC_ELT (elements, i * 2) = shallow (index, depth + 1);
        TREE_VEC_ELT (elements, i * 2 + 1) = expand ? value (v, at, depth + 1) : shallow (v, depth + 1);
      }
    tree flags = list (number (CONSTRUCTOR_NO_CLEARING (t)), number (TREE_CLOBBER_P (t)),
                       TREE_CLOBBER_P (t) ? number (CLOBBER_KIND (t)) : NULL_TREE);
    return term (CONSTRUCTOR, type (TREE_TYPE (t)), list (flags, elements));
  }
  tree writer_argument (tree t, gimple *at, unsigned depth = 0)
  {
    if (depth > 64) return refuse ("writer argument exceeds proof depth");
    if (variable (t))
      if (gimple *d = guards.definition_at (at, t))
        if (is_gimple_assign (d)
            && (gimple_assign_single_p (d)
                || (gimple_assign_cast_p (d)
                    && useless_type_conversion_p (TREE_TYPE (t), TREE_TYPE (gimple_assign_rhs1 (d))))))
          return writer_argument (gimple_assign_rhs1 (d), d, depth + 1);
    return shallow (t);
  }
  tree writer (gimple *s)
  {
    if (is_gimple_call (s))
      {
        tree fn = gimple_call_fndecl (s);
        tree args = make_tree_vec (gimple_call_num_args (s));
        for (unsigned i = 0; i < gimple_call_num_args (s); ++i) TREE_VEC_ELT (args, i) = writer_argument (gimple_call_arg (s, i), s);
        return term (TERM_CALL, NULL_TREE,
          list (symbol (gimple_call_lhs (s)),
                gimple_call_internal_p (s) ? number (gimple_call_internal_fn (s)) : fn, args));
      }
    if (!is_gimple_assign (s)) return refuse ("unclassified memory writer");
    tree children = make_tree_vec (gimple_num_ops (s));
    for (unsigned i = 0; i < gimple_num_ops (s); ++i) TREE_VEC_ELT (children, i) = shallow (gimple_op (s, i));
    return term (gimple_assign_rhs_code (s), NULL_TREE, children);
  }
  bool readonly_call (gimple *s)
  {
    if (gimple_call_internal_p (s)) return true;
    tree fn = gimple_call_fndecl (s);
    if (!fn || !fndecl_built_in_p (fn, BUILT_IN_NORMAL)) return false;
    switch (DECL_FUNCTION_CODE (fn))
      {
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS:
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER:
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE:
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL:
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN:
      case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END:
      case BUILT_IN_DWARF_CFA: return true;
      default: return false;
      }
  }
  tree allocation_root (tree t, gimple *at, bool pointer = false, unsigned depth = 0)
  {
    if (!t || depth > 64) return NULL_TREE;
    if (TREE_CODE (t) == COMPONENT_REF || TREE_CODE (t) == ARRAY_REF)
      return allocation_root (TREE_OPERAND (t, 0), at, pointer, depth + 1);
    if (TREE_CODE (t) == MEM_REF || TREE_CODE (t) == INDIRECT_REF)
      return allocation_root (TREE_OPERAND (t, 0), at, true, depth + 1);
    if (TREE_CODE (t) == ADDR_EXPR)
      return allocation_root (TREE_OPERAND (t, 0), at, false, depth + 1);
    if (!variable (t)) return NULL_TREE;
    if (!pointer) return t;
    if (gimple *d = guards.definition_at (at, t))
      if (is_gimple_assign (d)
          && (gimple_assign_single_p (d) || gimple_assign_cast_p (d)
              || gimple_assign_rhs_code (d) == POINTER_PLUS_EXPR))
        return allocation_root (gimple_assign_rhs1 (d), d, true, depth + 1);
    return TREE_CODE (t) == PARM_DECL ? t : NULL_TREE;
  }
  /* Control provenance describes decisions at their evaluation points, not
     assertions about a variable's later value.  Keep source variables as
     symbolic inputs and expand compiler temporaries/copies.  This avoids
     recursively asking for reaching memory versions while building control
     history (and preserves user variables moved into lexical frames). */
  tree condition_value (tree t, gimple *at, unsigned depth = 0)
  {
    if (!t) return NULL_TREE;
    if (depth > 64) return refuse ("control provenance exceeds proof depth");
    std::pair<tree, gimple *> key (t, at);
    if (tree *old = condition_values.get (key)) return *old;
    tree result = NULL_TREE;
    if (variable (t))
      {
        gimple *d = guards.definition_at (at, t);
        bool temporary = TREE_CODE (t) == SSA_NAME || (DECL_P (t) && DECL_ARTIFICIAL (t));
        if (temporary && d && is_gimple_assign (d))
          {
            tree a = gimple_assign_rhs1 (d);
            if (gimple_assign_single_p (d)
                || (gimple_assign_cast_p (d) && useless_type_conversion_p (TREE_TYPE (t), TREE_TYPE (a))))
              result = condition_value (a, d, depth + 1);
            else
              {
                tree args = make_tree_vec (gimple_num_ops (d) - 1);
                for (unsigned i = 1; i < gimple_num_ops (d); ++i)
                  TREE_VEC_ELT (args, i - 1) = condition_value (gimple_op (d, i), d, depth + 1);
                result = term (gimple_assign_rhs_code (d), type (TREE_TYPE (t)), args);
              }
          }
        else result = shallow (t);
      }
    else if (TREE_CODE (t) == ADDR_EXPR)
      result = term (ADDR_EXPR, NULL_TREE, list (location (TREE_OPERAND (t, 0), at, depth + 1, true)));
    else if (TREE_CODE (t) == COMPONENT_REF && newtm_clone_memory_field (TREE_OPERAND (t, 1)))
      result = location (t, at, depth + 1, true);
    else if (REFERENCE_CLASS_P (t) && TREE_CODE (t) != REALPART_EXPR && TREE_CODE (t) != IMAGPART_EXPR)
      result = term (TERM_LOAD, type (TREE_TYPE (t)), list (location (t, at, depth + 1, true)));
    else if (EXPR_P (t))
      {
        tree args = make_tree_vec (TREE_OPERAND_LENGTH (t));
        for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
          TREE_VEC_ELT (args, i) = condition_value (TREE_OPERAND (t, i), at, depth + 1);
        result = term (TREE_CODE (t), type (TREE_TYPE (t)), args);
      }
    else result = shallow (t);
    condition_values.put (key, result);
    return result;
  }
  /* Must history at every node, including loops.  A reexecuted predicate
     replaces its previous decision; joins intersect facts.  Unlike a set of
     may-reaching writers alone, (writer, history) distinguishes conditional
     selections with the same values assigned on opposite branches. */
  void build_controls ()
  {
    controls_built = true;
    for (unsigned i = 0; i < flow.node_count (); ++i)
      if (gimple *s = flow.node (i).statement)
        {
        if (flow.node (i).reachable && gimple_code (s) == GIMPLE_COND && !condition_indices.get (s))
          {
            gcond *c = as_a<gcond *> (s);
            tree_code code = gimple_cond_code (c);
            bool inverse = false;
            bool integral = INTEGRAL_TYPE_P (TREE_TYPE (gimple_cond_lhs (c))) || POINTER_TYPE_P (TREE_TYPE (gimple_cond_lhs (c)));
            if (code == NE_EXPR) { code = EQ_EXPR; inverse = true; }
            else if (integral && code == GE_EXPR) { code = LT_EXPR; inverse = true; }
            else if (integral && code == GT_EXPR) { code = LE_EXPR; inverse = true; }
            tree key = term (code, NULL_TREE, list (condition_value (gimple_cond_lhs (c), s), condition_value (gimple_cond_rhs (c), s)));
            unsigned index = 0;
            while (index < predicates.length () && !same (key, predicates[index])) ++index;
            if (index == predicates.length ()) predicates.safe_push (key);
            condition_indices.put (s, index);
            if (inverse) inverted_conditions.add (s);
          }
        else if (flow.node (i).reachable && gimple_code (s) == GIMPLE_SWITCH && !switch_indices.get (s))
          {
            tree selector = condition_value (gimple_switch_index (as_a<gswitch *> (s)), s);
            auto_vec<tree> choices;
            for (unsigned j = 0; j < gimple_switch_num_labels (as_a<gswitch *> (s)); ++j)
              {
                tree c = gimple_switch_label (as_a<gswitch *> (s), j), lo = CASE_LOW (c), hi = CASE_HIGH (c);
                /* Small ranges are atomized so grouping adjacent labels in
                   build_cfg does not change the proof. Large ranges retain
                   their interval; unsupported regroupings fail closed. */
                auto_vec<tree> lows, highs;
                if (lo && hi)
                  {
                    tree at = lo;
                    for (unsigned count = 0; count <= 128; ++count)
                      {
                        if (count == 128) { lows.truncate (0); highs.truncate (0); break; }
                        lows.safe_push (at); highs.safe_push (NULL_TREE);
                        if (tree_int_cst_equal (at, hi)) break;
                        at = fold_build2 (PLUS_EXPR, TREE_TYPE (at), at, build_one_cst (TREE_TYPE (at)));
                      }
                  }
                if (lows.is_empty ()) { lows.safe_push (lo); highs.safe_push (hi); }
                for (unsigned k = 0; k < lows.length (); ++k)
                  {
                    tree key = term (TERM_SWITCH, selector, list (lows[k], highs[k]));
                    unsigned index = 0;
                    while (index < predicates.length () && !same (key, predicates[index])) ++index;
                    if (index == predicates.length ()) predicates.safe_push (key);
                    choices.safe_push (list (lows[k], highs[k], number (index)));
                  }
              }
            tree data = make_tree_vec (choices.length ());
            for (unsigned j = 0; j < choices.length (); ++j) TREE_VEC_ELT (data, j) = choices[j];
            switch_indices.put (s, data);
          }
        }
    unsigned count = predicates.length () * 2;
    for (unsigned i = 0; i < flow.node_count (); ++i)
      {
        bitmap state = BITMAP_ALLOC (&control_storage);
        if (i && flow.node (i).reachable && count) bitmap_set_range (state, 0, count);
        controls.safe_push (state);
      }
    auto_bitmap input (&control_storage), path (&control_storage);
    bool changed;
    do
      {
        changed = false;
        for (unsigned i = 1; i < flow.node_count (); ++i)
          if (flow.node (i).reachable)
            {
              bitmap_clear (input); if (count) bitmap_set_range (input, 0, count);
              bool incoming = false;
              for (unsigned p : flow.node (i).predecessors)
                if (flow.node (p).reachable)
                  for (const newtm_flow_edge &edge : flow.node (p).successors)
                    if (edge.target == i)
                      {
                        tree *selection = switch_indices.get (flow.node (p).statement);
                        if (selection && edge.kind == NEWTM_FLOW_CASE && edge.selector)
                          {
                            tree lo = CASE_LOW (edge.selector), hi = CASE_HIGH (edge.selector);
                            if (!hi) hi = lo;
                            bool matched = false;
                            for (int j = 0; j < TREE_VEC_LENGTH (*selection); ++j)
                              {
                                tree choice = TREE_VEC_ELT (*selection, j), a = TREE_VEC_ELT (choice, 0), b = TREE_VEC_ELT (choice, 1);
                                if (!b) b = a;
                                if ((!lo && !a) || (lo && a && !tree_int_cst_lt (a, lo) && !tree_int_cst_lt (hi, b)))
                                  {
                                    matched = incoming = true; bitmap_copy (path, controls[p]);
                                    for (int k = 0; k < TREE_VEC_LENGTH (*selection); ++k)
                                      {
                                        unsigned id = tree_to_uhwi (TREE_VEC_ELT (TREE_VEC_ELT (*selection, k), 2));
                                        bitmap_clear_bit (path, id * 2 + 1); bitmap_set_bit (path, id * 2);
                                      }
                                    unsigned id = tree_to_uhwi (TREE_VEC_ELT (choice, 2));
                                    bitmap_clear_bit (path, id * 2); bitmap_set_bit (path, id * 2 + 1);
                                    bitmap_and_into (input, path);
                                  }
                              }
                            if (!matched) { refuse ("switch edge lost its scalar choice"); return; }
                            continue;
                          }
                        incoming = true; bitmap_copy (path, controls[p]);
                        unsigned *condition = condition_indices.get (flow.node (p).statement);
                        if (condition && (edge.kind == NEWTM_FLOW_TRUE || edge.kind == NEWTM_FLOW_FALSE))
                          {
                            bool truth = edge.kind == NEWTM_FLOW_TRUE;
                            if (inverted_conditions.contains (flow.node (p).statement)) truth = !truth;
                            bitmap_clear_bit (path, *condition * 2 + !truth);
                            bitmap_set_bit (path, *condition * 2 + truth);
                          }
                        bitmap_and_into (input, path);
                      }
              if (!incoming) bitmap_clear (input);
              if (!bitmap_equal_p (controls[i], input))
                { bitmap_copy (controls[i], input); changed = true; }
            }
      }
    while (changed);
  }
  tree control_at (unsigned n)
  {
    if (!controls_built) build_controls ();
    tree facts = make_tree_vec (bitmap_count_bits (controls[n]));
    unsigned bit, index = 0; bitmap_iterator it;
    EXECUTE_IF_SET_IN_BITMAP (controls[n], 0, bit, it)
      TREE_VEC_ELT (facts, index++) = list (predicates[bit / 2], number (bit & 1));
    return term (TERM_CONTROL, NULL_TREE, facts);
  }
  /* May-alias versions, at the load/use itself.  Disjoint local allocations
     are distinct; all unresolved indirect aliases conservatively participate.
     Channel nonalias and call-output validity remain explicit block-3/4
     dependencies, not consequences of an ABI-compatible type. */
  tree versions (tree target, gimple *use, bool memory)
  {
    unsigned *point = positions.get (use);
    if (!point) return refuse ("memory use is absent from CFG");
    auto_vec<unsigned char> seen; seen.safe_grow_cleared (flow.node_count ());
    auto_vec<unsigned> work;
    for (unsigned p : flow.node (*point).predecessors) work.safe_push (p);
    auto_vec<tree> writers;
    bool entry = false;
    for (unsigned at = 0; at < work.length (); ++at)
      {
        unsigned n = work[at]; if (seen[n]) continue; seen[n] = 1;
        if (!n) { entry = true; continue; }
        gimple *s = flow.node (n).statement;
        tree lhs = s && is_gimple_assign (s) ? gimple_assign_lhs (s)
          : s && is_gimple_call (s) ? gimple_call_lhs (s) : NULL_TREE;
        bool exact = lhs && operand_equal_p (lhs, target, 0);
        if (lhs && DECL_P (target) && TREE_CODE (lhs) == COMPONENT_REF)
          if (tree source = newtm_clone_memory_field (TREE_OPERAND (lhs, 1)))
            if (integer_zerop (TREE_PURPOSE (source))
                && newtm_clone_memory_origin (TREE_VALUE (source)) == newtm_clone_memory_origin (target)) exact = true;
        bool alias = false;
        if (memory && lhs && REFERENCE_CLASS_P (lhs))
          {
            tree a = allocation_root (lhs, s), b = allocation_root (target, use);
            alias = !(a && b && VAR_P (a) && VAR_P (b) && a != b);
            if (alias && a && b && VAR_P (a) && DECL_CONTEXT (a) == owner
                && !TREE_STATIC (a) && !DECL_EXTERNAL (a))
              for (unsigned channel = 2; channel <= 4; ++channel)
                if (newtm_clone_channel (owner, channel)
                    && newtm_clone_memory_origin (b) == newtm_clone_memory_origin (newtm_clone_channel (owner, channel)))
                  { alias = false; dependencies |= NEWTM_MEMORY_CHANNELS; }
          }
        if (memory && s && is_gimple_call (s) && !readonly_call (s))
          { alias = true; dependencies |= NEWTM_MEMORY_CHANNELS | NEWTM_MEMORY_STATUS; }
        if (exact || alias)
          {
            tree value = list (writer (s), control_at (n)); bool present = false;
            for (tree old : writers) if (same (value, old)) present = true;
            if (!present) writers.safe_push (value);
            if (exact) continue;
          }
        for (unsigned p : flow.node (n).predecessors) work.safe_push (p);
      }
    /* Canonicalize the set by stable statement traversal order through the
       flow rather than predecessor enumeration order. */
    tree result = make_tree_vec (writers.length () + 1);
    TREE_VEC_ELT (result, 0) = number (entry);
    for (unsigned i = 0; i < writers.length (); ++i) TREE_VEC_ELT (result, i + 1) = writers[i];
    return term (TERM_VERSIONS, NULL_TREE, result);
  }
  tree location (tree t, gimple *at, unsigned depth, bool control = false)
  {
    if (!t || depth > 64) return refuse ("memory location exceeds proof depth");
    if (TREE_CODE (t) == SSA_NAME)
      {
        gimple *d = guards.definition_at (at, t);
        if (d && is_gimple_assign (d) && gimple_assign_single_p (d)
            && REFERENCE_CLASS_P (gimple_assign_rhs1 (d))) return location (gimple_assign_rhs1 (d), d, depth + 1, control);
        return declaration (t);
      }
    if (DECL_P (t)) return declaration (t);
    if (TREE_CODE (t) == MEM_REF && integer_zerop (TREE_OPERAND (t, 1)))
      {
        tree pointer = TREE_OPERAND (t, 0);
        if (TREE_CODE (pointer) == ADDR_EXPR) return location (TREE_OPERAND (pointer, 0), at, depth + 1, control);
      }
    if (!REFERENCE_CLASS_P (t)) return refuse ("unclassified memory location");
    if (TREE_CODE (t) == COMPONENT_REF)
      {
        tree field = TREE_OPERAND (t, 1);
        tree source = newtm_clone_memory_field (field);
        if (source)
          {
            /* Structural correspondence is checked by block 1.  Preserve
               the activation: a null or arbitrary computed frame is not a
               declaration instance. */
            tree base = TREE_OPERAND (t, 0);
            if (TREE_CODE (base) == MEM_REF) base = TREE_OPERAND (base, 0);
            if (!variable (base)) return refuse ("unknown remapped frame instance");
            tree source_type = TREE_TYPE (TREE_VALUE (source));
            bool indirect = integer_onep (TREE_PURPOSE (source));
            if (indirect) source_type = build_pointer_type (source_type);
            if (!types_compatible_p (TREE_TYPE (t), source_type))
              return refuse ("remapped field changed its object layout");
            if (!frame (TREE_OPERAND (t, 0), field, TREE_VALUE (source), at)) return error_mark_node;
            dependencies |= NEWTM_MEMORY_CHANNELS;
            tree object = declaration (TREE_VALUE (source), true);
            return indirect ? term (ADDR_EXPR, NULL_TREE, list (object)) : object;
          }
        tree base = location (TREE_OPERAND (t, 0), at, depth + 1, control);
        tree detail = list (newtm_clone_memory_origin (field),
          list (DECL_FIELD_OFFSET (field), DECL_FIELD_BIT_OFFSET (field), DECL_SIZE_UNIT (field)),
          type (TREE_TYPE (TREE_OPERAND (t, 0))));
        return term (COMPONENT_REF, type (TREE_TYPE (t)), list (base, detail));
      }
    tree children = make_tree_vec (TREE_OPERAND_LENGTH (t));
    for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
      {
        tree part = TREE_OPERAND (t, i);
        TREE_VEC_ELT (children, i) = i == 0 && TREE_CODE (t) == ARRAY_REF
          ? location (part, at, depth + 1, control)
          : control ? condition_value (part, at, depth + 1) : value (part, at, depth + 1);
      }
    return term (TREE_CODE (t), type (TREE_TYPE (t)), children);
  }
  bool frame (tree ref, tree field, tree source, gimple *at)
  {
    tree record = TREE_TYPE (ref), bytes = TYPE_SIZE_UNIT (record);
    if (TREE_CODE (record) != RECORD_TYPE || !bytes || !tree_fits_uhwi_p (bytes))
      { refuse ("unknown frame layout"); return false; }
    unsigned HOST_WIDE_INT extent = tree_to_uhwi (bytes);
    auto_vec<unsigned HOST_WIDE_INT> starts, sizes;
    bool found = false;
    for (tree f = TYPE_FIELDS (record); f; f = DECL_CHAIN (f))
      if (TREE_CODE (f) == FIELD_DECL)
        {
          tree off = DECL_FIELD_OFFSET (f), bit = DECL_FIELD_BIT_OFFSET (f), n = DECL_SIZE_UNIT (f);
          if (!off || !bit || !n || !tree_fits_uhwi_p (off) || !tree_fits_uhwi_p (bit) || !tree_fits_uhwi_p (n)
              || tree_to_uhwi (bit) % BITS_PER_UNIT)
            { refuse ("unrepresentable frame field interval"); return false; }
          unsigned HOST_WIDE_INT start = tree_to_uhwi (off), extra = tree_to_uhwi (bit) / BITS_PER_UNIT, size = tree_to_uhwi (n);
          if (start > extent || extra > extent - start) { refuse ("frame field is outside its allocation"); return false; }
          start += extra;
          if (size > extent - start) { refuse ("frame field is outside its allocation"); return false; }
          unsigned alignment = DECL_ALIGN (f);
          if (!alignment || alignment > TYPE_ALIGN (record)
              || (alignment >= BITS_PER_UNIT && start % (alignment / BITS_PER_UNIT)))
            { refuse ("frame field has an invalid alignment"); return false; }
          for (unsigned i = 0; i < starts.length (); ++i)
            if (size && sizes[i] && start < starts[i] + sizes[i] && starts[i] < start + size)
              { refuse ("overlapping frame field intervals"); return false; }
          starts.safe_push (start); sizes.safe_push (size); found |= f == field;
        }
    if (!found) { refuse ("field does not belong to its current frame"); return false; }
    tree root = ref;
    if (TREE_CODE (root) == MEM_REF) root = TREE_OPERAND (root, 0);
    for (unsigned depth = 0; depth < 64 && variable (root); ++depth)
      {
        gimple *d = guards.definition_at (at, root);
        if (!d || !is_gimple_assign (d) || (!gimple_assign_single_p (d) && !gimple_assign_cast_p (d))) break;
        root = gimple_assign_rhs1 (d); at = d;
      }
    if (TREE_CODE (root) == ADDR_EXPR) root = TREE_OPERAND (root, 0);
    if (TREE_CODE (root) == SSA_NAME && SSA_NAME_VAR (root)) root = SSA_NAME_VAR (root);
    function *body = DECL_STRUCT_FUNCTION (owner);
    bool own = VAR_P (root) && DECL_CONTEXT (root) == owner && !TREE_STATIC (root) && !POINTER_TYPE_P (TREE_TYPE (root));
    bool chain = body && root == body->static_chain_decl;
    bool receiver = root == entry_frame && root
      && types_compatible_p (TREE_TYPE (TREE_TYPE (root)), record);
    if (!own && !chain && !receiver) { refuse ("frame address lost its activation instance"); return false; }
    tree key = DECL_CONTEXT (source);
    if (!key || TREE_CODE (key) != FUNCTION_DECL) key = source;
    if (tree *old = frame_instances.get (key))
      { if (*old != root) { refuse ("one lexical activation uses distinct frames"); return false; } }
    else frame_instances.put (key, root);
    return true;
  }
  tree rhs (gimple *s, unsigned depth)
  {
    if (is_gimple_assign (s))
      {
        tree a = gimple_assign_rhs1 (s);
        if (gimple_assign_single_p (s)) return value (a, s, depth + 1);
        tree lhs = gimple_assign_lhs (s);
        if (gimple_assign_cast_p (s)
            && useless_type_conversion_p (TREE_TYPE (lhs), TREE_TYPE (a))) return value (a, s, depth + 1);
        tree children = make_tree_vec (gimple_num_ops (s) - 1);
        for (unsigned i = 1; i < gimple_num_ops (s); ++i)
          TREE_VEC_ELT (children, i - 1) = value (gimple_op (s, i), s, depth + 1);
        return term (gimple_assign_rhs_code (s), type (TREE_TYPE (lhs)), children);
      }
    if (is_gimple_call (s) && gimple_call_internal_p (s))
      {
        tree children = make_tree_vec (gimple_call_num_args (s));
        for (unsigned i = 0; i < gimple_call_num_args (s); ++i)
          TREE_VEC_ELT (children, i) = value (gimple_call_arg (s, i), s, depth + 1);
        return term (TERM_CALL, number (gimple_call_internal_fn (s)), children);
      }
    return term (TERM_INPUT, NULL_TREE, writer (s));
  }
  tree value (tree t, gimple *at, unsigned depth = 0)
  {
    if (!t) return NULL_TREE;
    std::pair<tree, gimple *> key (t, at);
    if (tree *p = values.get (key)) return *p;
    tree result = value_uncached (t, at, depth);
    values.put (key, result);
    return result;
  }
  tree value_uncached (tree t, gimple *at, unsigned depth)
  {
    if (!t) return NULL_TREE;
    if (depth > 64) return refuse ("memory value exceeds proof depth");
    if (CONSTANT_CLASS_P (t)) return term (TREE_CODE (t), type (TREE_TYPE (t)), t);
    if (TREE_CODE (t) == CONSTRUCTOR) return constructor (t, at, true, depth);
    if (variable (t))
      {
        if (gimple *d = guards.definition_at (at, t)) return rhs (d, depth + 1);
        if (TREE_CODE (t) == PARM_DECL && POINTER_TYPE_P (TREE_TYPE (t)))
          dependencies |= NEWTM_MEMORY_CHANNELS;
        return term (TERM_INPUT, type (TREE_TYPE (t)), list (declaration (t), versions (t, at, guards.address_exposed (t))));
      }
    if (TREE_CODE (t) == ADDR_EXPR)
      return term (ADDR_EXPR, NULL_TREE, list (location (TREE_OPERAND (t, 0), at, depth + 1)));
    if (REFERENCE_CLASS_P (t) && TREE_CODE (t) != REALPART_EXPR && TREE_CODE (t) != IMAGPART_EXPR)
      {
        if (TREE_CODE (t) == COMPONENT_REF)
          if (tree source = newtm_clone_memory_field (TREE_OPERAND (t, 1)))
            {
              tree object = location (t, at, depth + 1);
              if (integer_onep (TREE_PURPOSE (source))) return object;
              return term (TERM_INPUT, type (TREE_TYPE (t)),
                list (object, versions (TREE_VALUE (source), at, true)));
            }
        dependencies |= NEWTM_MEMORY_CHANNELS;
        return term (TERM_LOAD, type (TREE_TYPE (t)),
                     list (location (t, at, depth + 1), versions (t, at, true)));
      }
    if (!EXPR_P (t)) return refuse ("unclassified memory value");
    tree children = make_tree_vec (TREE_OPERAND_LENGTH (t));
    for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
      TREE_VEC_ELT (children, i) = value (TREE_OPERAND (t, i), at, depth + 1);
    return term (TREE_CODE (t), type (TREE_TYPE (t)), children);
  }
  tree witness (tree old)
  {
    if (!old || TREE_CODE (old) == INTEGER_CST) return old;
    if (TREE_CODE (old) == TREE_VEC)
      {
        tree found = NULL_TREE;
        for (tree candidate : ssa_decls)
          if (same (symbol (candidate), old))
            { if (found && found != candidate) return NULL_TREE; found = candidate; }
        return found;
      }
    tree *p = current_decls.get (old);
    return p && *p != error_mark_node ? *p : NULL_TREE;
  }
  bool frames ()
  {
    struct inspect {
      resolver *self; gimple *statement;
      static tree visit (tree *p, int *walk, void *data)
      {
        walk_stmt_info *wi = (walk_stmt_info *) data;
        inspect *v = (inspect *) wi->info;
        if (TREE_CODE (*p) == COMPONENT_REF
            && newtm_clone_memory_field (TREE_OPERAND (*p, 1)))
          v->self->location (*p, v->statement, 0);
        if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
        return NULL_TREE;
      }
    } v = { this, NULL };
    for (unsigned i = 0; i < flow.node_count (); ++i)
      if (gimple *s = flow.node (i).statement)
        {
          v.statement = s; walk_stmt_info wi = {}; wi.info = &v;
          walk_gimple_op (s, inspect::visit, &wi);
        }
    return !failure;
  }
  bool require (gimple *s, tree predicate, newtm_local_rule rule = NEWTM_V03)
  {
    const char *why = NULL;
    if (guards.require (s, predicate, &why)) return true;
    if (!failure)
      {
        failure_rule = rule;
        if (rule == NEWTM_V04)
          {
            failure = "storage classification guard is not established on every reaching path";
            failure_detail = why;
          }
        else failure = why;
      }
    return false;
  }
  bool ranges (gimple *s, tree *v, newtm_memory_role role)
  {
    tree o = v[OBJECT], e = v[EXTENT], d = v[OFFSET], n = v[SIZE], k = v[KIND], storage = v[STORAGE];
    if (!o || !e || !d || !n || !k || !storage) return false;
    if (TREE_CODE (storage) == INTEGER_CST && integer_onep (storage))
      {
        tree root = o; gimple *use = s;
        for (unsigned depth = 0; depth < 64 && variable (root); ++depth)
          {
            gimple *def = guards.definition_at (use, root);
            if (!def || !is_gimple_assign (def) || (!gimple_assign_single_p (def) && !gimple_assign_cast_p (def))) break;
            root = gimple_assign_rhs1 (def); use = def;
          }
        tree allocation = TREE_CODE (root) == ADDR_EXPR ? get_base_address (TREE_OPERAND (root, 0)) : NULL_TREE;
        if (!allocation || !DECL_P (allocation) || DECL_CONTEXT (allocation) != owner
            || TREE_STATIC (allocation) || DECL_EXTERNAL (allocation))
          { refuse ("constant PRIVATE classification lacks an activation-owned allocation", NEWTM_V04); return false; }
      }
    tree pu = unsigned_type_for (pointer_sized_int_node);
    tree wide = build_nonstandard_integer_type (MAX (TYPE_PRECISION (size_type_node), TYPE_PRECISION (pu)), true);
    tree address = fold_convert (wide, fold_convert (pu, o));
    tree room = fold_build2 (MINUS_EXPR, wide, fold_convert (wide, TYPE_MAX_VALUE (pu)), address);
    tree span = fold_build2 (MINUS_EXPR, size_type_node,
      fold_build2 (MAX_EXPR, size_type_node, e, size_one_node), size_one_node);
    tree left = fold_build2 (MINUS_EXPR, size_type_node, e, fold_build2 (MIN_EXPR, size_type_node, d, e));
    tree predicates[] = {
      fold_build2 (NE_EXPR, boolean_type_node, o, build_zero_cst (TREE_TYPE (o))),
      fold_build2 (NE_EXPR, boolean_type_node, e, size_zero_node),
      fold_build2 (LE_EXPR, boolean_type_node, k, build_int_cst (TREE_TYPE (k), NEWTM_ABI_AGGREGATE)),
      fold_build2 (LE_EXPR, boolean_type_node, storage, build_int_cst (TREE_TYPE (storage), NEWTM_STORAGE_PRIVATE)),
      fold_build2 (LE_EXPR, boolean_type_node, fold_convert (wide, span), room),
      fold_build2 (LE_EXPR, boolean_type_node, fold_convert (wide, d), room),
      fold_build2 (LE_EXPR, boolean_type_node, d, e),
      fold_build2 (LE_EXPR, boolean_type_node, n, left)
    };
    for (unsigned i = 0; i < ARRAY_SIZE (predicates); ++i)
      if (!require (s, predicates[i], i == 3 ? NEWTM_V04 : NEWTM_V03)) return false;
    if (!integer_zerop (n))
      {
        tree whole = fold_build2 (BIT_AND_EXPR, boolean_type_node,
          fold_build2 (EQ_EXPR, boolean_type_node, d, size_zero_node),
          fold_build2 (EQ_EXPR, boolean_type_node, n, e));
        tree aggregate = fold_build2 (NE_EXPR, boolean_type_node, k, build_zero_cst (TREE_TYPE (k)));
        if (!require (s, fold_build2 (BIT_IOR_EXPR, boolean_type_node, aggregate, whole))) return false;
      }
    if (role != NEWTM_MEMORY_ADDRESS)
      {
        unsigned expected = role == NEWTM_MEMORY_PRIVATE_READ || role == NEWTM_MEMORY_PRIVATE_WRITE
          ? NEWTM_STORAGE_PRIVATE : NEWTM_STORAGE_REGISTERED;
        /* With the separately proved unsigned storage <= PRIVATE, the
           non-private edge is exactly REGISTERED.  Keep the actual branch
           predicate rather than asking the guard solver to combine atoms. */
        tree choice = fold_build2 (expected == NEWTM_STORAGE_PRIVATE ? EQ_EXPR : NE_EXPR,
          boolean_type_node, storage, build_int_cst (TREE_TYPE (storage), NEWTM_STORAGE_PRIVATE));
        if (!require (s, choice, NEWTM_V04)) return false;
      }
    if (v[LOGICAL])
      {
        tree position = fold_build2 (PLUS_EXPR, wide, address, fold_convert (wide, d));
        tree logical = fold_convert (wide, fold_convert (pu, v[LOGICAL]));
        if (!require (s, fold_build2 (EQ_EXPR, boolean_type_node, logical, position))) return false;
      }
    return true;
  }
  bool arithmetic_dependencies (gimple *use, tree root, hash_set<tree> &math,
                                 unsigned depth = 0, gimple *critical = NULL)
  {
    if (!critical) critical = use;
    if (!root) return true;
    if (depth > 64) { refuse ("arithmetic dependency exceeds proof depth"); return false; }
    if (variable (root))
      {
        gimple *d = guards.definition_at (use, root);
        if (!d) return true; /* Reaching-version term was compared separately. */
        if (!is_gimple_assign (d))
          {
            if (is_gimple_call (d) && gimple_call_internal_p (d))
              for (unsigned i = 0; i < gimple_call_num_args (d); ++i)
                if (!arithmetic_dependencies (d, gimple_call_arg (d, i), math, depth + 1, critical)) return false;
            return true;
          }
        tree_code code = gimple_assign_rhs_code (d);
        if (math.contains (root) && gimple_assign_cast_p (d))
          {
            tree from = gimple_assign_rhs1 (d), a = TREE_TYPE (from), b = TREE_TYPE (root);
            if (INTEGRAL_TYPE_P (a) && INTEGRAL_TYPE_P (b))
              {
                tree wide = build_nonstandard_integer_type (MAX (TYPE_PRECISION (a), TYPE_PRECISION (b)) + 1, false);
                tree amin = fold_convert (wide, TYPE_MIN_VALUE (a)), amax = fold_convert (wide, TYPE_MAX_VALUE (a));
                tree bmin = fold_convert (wide, TYPE_MIN_VALUE (b)), bmax = fold_convert (wide, TYPE_MAX_VALUE (b));
                if (tree_int_cst_lt (amin, bmin)
                    && !require (critical, fold_build2 (GE_EXPR, boolean_type_node, from, fold_convert (a, TYPE_MIN_VALUE (b))))) return false;
                if (tree_int_cst_lt (bmax, amax)
                    && !require (critical, fold_build2 (LE_EXPR, boolean_type_node, from, fold_convert (a, TYPE_MAX_VALUE (b))))) return false;
              }
          }
        if (code == REALPART_EXPR)
          {
            tree pair = TREE_OPERAND (gimple_assign_rhs1 (d), 0);
            gimple *producer = guards.definition_at (d, pair);
            if (producer && is_gimple_call (producer) && gimple_call_internal_p (producer))
              {
                internal_fn op = gimple_call_internal_fn (producer);
                if (op == IFN_ADD_OVERFLOW || op == IFN_SUB_OVERFLOW || op == IFN_MUL_OVERFLOW)
                  {
                    tree type = TREE_TYPE (TREE_TYPE (pair));
                    tree flag = build1 (IMAGPART_EXPR, type, pair);
                    if (!require (critical, fold_build2 (EQ_EXPR, boolean_type_node, flag, build_zero_cst (type)))) return false;
                  }
              }
          }
        for (unsigned i = 1; i < gimple_num_ops (d); ++i)
          if (!arithmetic_dependencies (d, gimple_op (d, i), math, depth + 1, critical)) return false;
      }
    else if (EXPR_P (root) && TREE_CODE (root) != ADDR_EXPR)
      for (int i = 0; i < TREE_OPERAND_LENGTH (root); ++i)
        if (!arithmetic_dependencies (use, TREE_OPERAND (root, i), math, depth + 1, critical)) return false;
    return true;
  }
};
static bool fail (tree fn, gimple *s, const char *why,
                  const char *rule = "V03", const char *operation = NULL)
{
  newtm_current_diagnose (fn, s, rule, "memory",
                          why ? why : "unresolved memory identity", operation);
  if (!memory_results) memory_results = hash_map<tree, tree>::create_ggc ();
  memory_results->put (fn, list (number (NEWTM_MEMORY_REFUSED), number (0), number (0)));
  return false;
}

static bool memory_range_fail (resolver &r, gimple *s)
{
  fail (r.owner, s, r.failure ? r.failure : "missing storage or range guard",
        r.failure_rule_name (), r.failure_rule == NEWTM_V04
        ? "storage classification" : "memory range");
  if (r.failure_detail)
    inform (newtm_current_diagnostic_location (r.owner, s),
            "underlying guard analysis: %s", r.failure_detail);
  return false;
}

static bool channel_fail (tree fn, gimple *s, const char *why)
{
  if (!channel_results) channel_results = hash_map<tree, tree>::create_ggc ();
  channel_results->put (fn, list (number (NEWTM_MEMORY_REFUSED), number (0), number (0)));
  newtm_current_diagnose (fn, s, "V06", "channel", why);
  return false;
}

static bool channel_call (gimple *s)
{
  if (!is_gimple_call (s) || gimple_call_internal_p (s)) return false;
  tree callee = gimple_call_fndecl (s);
  return callee && (newtm_clone_original (callee)
    || fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_READ)
    || fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE));
}

static bool status_call (gimple *s)
{
  if (!s || !is_gimple_call (s) || gimple_call_internal_p (s)) return false;
  tree fn = gimple_call_fndecl (s);
  return fn && (newtm_clone_original (fn)
    || (fndecl_built_in_p (fn, BUILT_IN_NORMAL)
        && DECL_FUNCTION_CODE (fn) >= BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN
        && DECL_FUNCTION_CODE (fn) < BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL));
}

static bool status_fail (tree fn, gimple *s, const char *why,
                         const char *rule = "V05")
{
  if (!status_results) status_results = hash_map<tree, tree>::create_ggc ();
  status_results->put (fn, list (number (NEWTM_MEMORY_REFUSED), number (0), number (0)));
  newtm_current_diagnose (fn, s, rule, "status", why);
  return false;
}

static bool status_dominates (const newtm_flow_graph &flow, gimple *a, gimple *b)
{
  auto_vec<unsigned char> seen; seen.safe_grow_cleared (flow.node_count ());
  auto_vec<unsigned> work; work.safe_push (0); seen[0] = 1;
  for (unsigned at = 0; at < work.length (); ++at)
    {
      unsigned n = work[at];
      if (flow.node (n).statement == a) continue;
      if (flow.node (n).statement == b) return false;
      for (const newtm_flow_edge &e : flow.node (n).successors)
        if (!seen[e.target]) { seen[e.target] = 1; work.safe_push (e.target); }
    }
  return true;
}

static tree status_predicate (resolver &r, gimple *s, bool *inverse)
{
  if (!r.controls_built) r.build_controls ();
  unsigned *i = r.condition_indices.get (s);
  if (!i) return NULL_TREE;
  *inverse = r.inverted_conditions.contains (s);
  return r.predicates[*i];
}

static gimple *status_final (const newtm_flow_graph &flow, gimple *store)
{
  gimple *last = NULL;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      if (is_gimple_call (s) && gimple_call_fndecl (s)
          && fndecl_built_in_p (gimple_call_fndecl (s), BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS)
          && status_dominates (flow, s, store)
          && (!last || status_dominates (flow, last, s))) last = s;
  return last;
}

/* Resolve only evaluated pointer copies at this use, with current reaching
   definitions.  A compatible pointer type is not evidence of an allocation. */
static tree channel_address (resolver &r, tree p, gimple *at, unsigned depth = 0)
{
  if (!p || depth > 64) return NULL_TREE;
  if (TREE_CODE (p) == ADDR_EXPR) return TREE_OPERAND (p, 0);
  if (CONVERT_EXPR_CODE_P (TREE_CODE (p))
      && useless_type_conversion_p (TREE_TYPE (p), TREE_TYPE (TREE_OPERAND (p, 0))))
    return channel_address (r, TREE_OPERAND (p, 0), at, depth + 1);
  if (variable (p))
    if (gimple *d = r.guards.definition_at (at, p))
      if (is_gimple_assign (d) && (gimple_assign_single_p (d)
          || (gimple_assign_cast_p (d)
              && useless_type_conversion_p (TREE_TYPE (p), TREE_TYPE (gimple_assign_rhs1 (d))))))
        return channel_address (r, gimple_assign_rhs1 (d), d, depth + 1);
  return NULL_TREE;
}

/* The finite semantic fields of an incoming array. Padding is not data.
   Proof terms freeze actual reaching producers, including slot and storage,
   without saving descriptors, instructions or re-executable expressions. */
static tree channel_fields (resolver &r, tree ref, gimple *at, unsigned depth = 0)
{
  if (!ref || depth > 16) return r.refuse ("channel layout exceeds proof depth");
  tree type = TREE_TYPE (ref), fields = NULL_TREE;
  if (TREE_CODE (type) == RECORD_TYPE)
    {
      for (tree f = TYPE_FIELDS (type); f; f = DECL_CHAIN (f))
        if (TREE_CODE (f) == FIELD_DECL)
          fields = tree_cons (NULL_TREE, channel_fields (r,
            build3 (COMPONENT_REF, TREE_TYPE (f), ref, f, NULL_TREE), at, depth + 1), fields);
    }
  else if (TREE_CODE (type) == ARRAY_TYPE)
    {
      tree size = TYPE_SIZE_UNIT (type), unit = TYPE_SIZE_UNIT (TREE_TYPE (type));
      if (!size || !unit || !tree_fits_uhwi_p (size) || !tree_fits_uhwi_p (unit)
          || !tree_to_uhwi (unit) || tree_to_uhwi (size) / tree_to_uhwi (unit) > 4096)
        return r.refuse ("incoming channel has no finite array extent");
      for (unsigned HOST_WIDE_INT n = 0; n < tree_to_uhwi (size) / tree_to_uhwi (unit); ++n)
        fields = tree_cons (NULL_TREE, channel_fields (r,
          build4 (ARRAY_REF, TREE_TYPE (type), ref, size_int (n), NULL_TREE, NULL_TREE), at, depth + 1), fields);
    }
  else return r.value (ref, at);
  tree result = make_tree_vec (list_length (fields));
  for (unsigned i = 0; fields; fields = TREE_CHAIN (fields), ++i)
    TREE_VEC_ELT (result, i) = TREE_VALUE (fields);
  return result;
}

static tree channel_call_term (resolver &r, gimple *s)
{
  tree callee = gimple_call_fndecl (s);
  tree formal = callee ? TYPE_ARG_TYPES (TREE_TYPE (callee)) : NULL_TREE;
  for (unsigned i = 0; i < gimple_call_num_args (s); ++i)
    {
      if (!formal || TREE_VALUE (formal) == void_type_node
          || !types_compatible_p (TREE_VALUE (formal), TREE_TYPE (gimple_call_arg (s, i))))
        return r.refuse ("channel call has an incompatible signature");
      formal = TREE_CHAIN (formal);
    }
  if (!formal || TREE_VALUE (formal) != void_type_node || TREE_CHAIN (formal))
    return r.refuse ("channel call has an incomplete signature");
  tree args = make_tree_vec (gimple_call_num_args (s));
  for (unsigned i = 0; i < gimple_call_num_args (s); ++i)
    TREE_VEC_ELT (args, i) = r.value (gimple_call_arg (s, i), s);
  tree input = NULL_TREE;
  if (newtm_clone_original (callee))
    {
      tree bytes = newtm_clone_transport_extent (callee, newtm_clone_channel (callee, 2), false);
      if (!bytes || !tree_fits_uhwi_p (bytes)) return r.refuse ("incoming has no signature extent");
      if (!integer_zerop (bytes))
        {
          tree ref = channel_address (r, gimple_call_arg (s, 1), s);
          if (!ref) return r.refuse ("incoming has no current allocation");
          /* Inspect every slot, not just the first element addressed by arg1. */
          tree pointer = build_fold_addr_expr (ref);
          tree unit_size = TYPE_SIZE_UNIT (newtm_abi_clone_binding_type ());
          if (!unit_size || !tree_fits_uhwi_p (unit_size) || integer_zerop (unit_size))
            return r.refuse ("incoming has no finite binding layout");
          unsigned HOST_WIDE_INT unit = tree_to_uhwi (unit_size);
          if (tree_to_uhwi (bytes) % unit) return r.refuse ("incoming has a partial binding extent");
          unsigned HOST_WIDE_INT count = tree_to_uhwi (bytes) / unit;
          if (count > 4096) return r.refuse ("incoming exceeds finite slot domain");
          input = make_tree_vec (count);
          for (unsigned HOST_WIDE_INT i = 0; i < count; ++i)
            {
              tree slot = build2 (MEM_REF, newtm_abi_clone_binding_type (), pointer,
                build_int_cst (TREE_TYPE (pointer), i * unit));
              if (TREE_CODE (ref) == ARRAY_REF && tree_fits_uhwi_p (TREE_OPERAND (ref, 1)))
                slot = build4 (ARRAY_REF, newtm_abi_clone_binding_type (), TREE_OPERAND (ref, 0),
                  size_int (tree_to_uhwi (TREE_OPERAND (ref, 1)) + i), NULL_TREE, NULL_TREE);
              TREE_VEC_ELT (input, i) = channel_fields (r, slot, s);
            }
        }
    }
  else if (fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE))
    {
      tree ref = channel_address (r, gimple_call_arg (s, 3), s);
      if (!ref) return r.refuse ("WRITE source has no current allocation");
      input = channel_fields (r, ref, s);
    }
  return list (callee, args, input);
}

/* Outlining may add surrounding control facts. It may not erase any
   pre-existing fact required by an incoming producer, or change its writers. */
static bool same_entry (tree a, tree b)
{
  if (same (a, b)) return true;
  if (!a || !b || TREE_CODE (a) != TREE_VEC || TREE_CODE (b) != TREE_VEC
      || TREE_VEC_LENGTH (a) != TREE_VEC_LENGTH (b)) return false;
  if (TREE_VEC_LENGTH (a) == 3 && TREE_VEC_ELT (a, 0) && TREE_VEC_ELT (b, 0)
      && TREE_CODE (TREE_VEC_ELT (a, 0)) == INTEGER_CST
      && TREE_CODE (TREE_VEC_ELT (b, 0)) == INTEGER_CST
      && tree_to_uhwi (TREE_VEC_ELT (a, 0)) == TERM_CONTROL
      && tree_to_uhwi (TREE_VEC_ELT (b, 0)) == TERM_CONTROL)
    {
      tree aa = TREE_VEC_ELT (a, 2), bb = TREE_VEC_ELT (b, 2);
      for (int i = 0; i < TREE_VEC_LENGTH (aa); ++i)
        {
          bool found = false;
          for (int j = 0; j < TREE_VEC_LENGTH (bb); ++j)
            if (same_entry (TREE_VEC_ELT (aa, i), TREE_VEC_ELT (bb, j))) found = true;
          if (!found) return false;
        }
      return true;
    }
  for (int i = 0; i < TREE_VEC_LENGTH (a); ++i)
    if (!same_entry (TREE_VEC_ELT (a, i), TREE_VEC_ELT (b, i))) return false;
  return true;
}

struct channel_interval { tree base; HOST_WIDE_INT start, size; };

/* Follow actual pointer copies, retaining the byte displacement.  Unknown
   arithmetic is not accepted as a channel access proof. */
static tree channel_pointer (resolver &r, tree p, gimple *at,
                             HOST_WIDE_INT &offset, unsigned depth = 0)
{
  if (!p || depth > 64) return NULL_TREE;
  for (unsigned i = 2; i <= 4; ++i)
    if (p == newtm_clone_channel (r.owner, i)) return p;
  if (CONVERT_EXPR_CODE_P (TREE_CODE (p)))
    return useless_type_conversion_p (TREE_TYPE (p), TREE_TYPE (TREE_OPERAND (p, 0)))
      ? channel_pointer (r, TREE_OPERAND (p, 0), at, offset, depth + 1) : NULL_TREE;
  if (variable (p))
    if (gimple *d = r.guards.definition_at (at, p))
      if (is_gimple_assign (d))
        {
          if (gimple_assign_single_p (d) || gimple_assign_cast_p (d))
            return channel_pointer (r, gimple_assign_rhs1 (d), d, offset, depth + 1);
          if (gimple_assign_rhs_code (d) == POINTER_PLUS_EXPR)
            {
              tree n = gimple_assign_rhs2 (d);
              if (!tree_fits_shwi_p (n) || tree_to_shwi (n) < 0
                  || tree_to_shwi (n) > HOST_WIDE_INT_MAX - offset) return NULL_TREE;
              offset += tree_to_shwi (n);
              return channel_pointer (r, gimple_assign_rhs1 (d), d, offset, depth + 1);
            }
        }
  return NULL_TREE;
}

static unsigned channel_destination (resolver &r, gimple *s)
{
  if (!is_gimple_assign (s)) return 0;
  tree l = gimple_assign_lhs (s), base = get_base_address (l);
  if (!base || TREE_CODE (base) != MEM_REF) return 0;
  HOST_WIDE_INT offset = 0;
  tree p = channel_pointer (r, TREE_OPERAND (base, 0), s, offset);
  for (unsigned i = 2; i <= 4; ++i)
    if (p && p == newtm_clone_channel (r.owner, i)) return i;
  return 0;
}

static bool channel_access (resolver &r, tree ref, gimple *s, bool write)
{
  if (!ref || !REFERENCE_CLASS_P (ref)) return true;
  bool reverse; HOST_WIDE_INT at, size;
  tree base = get_ref_base_and_extent_hwi (ref, &at, &size, &reverse);
  if (!base || TREE_CODE (base) != MEM_REF) return true;
  HOST_WIDE_INT displacement = 0;
  tree p = channel_pointer (r, TREE_OPERAND (base, 0), s, displacement);
  if (!p) return true; /* Ordinary memory is checked separately. */
  tree delta = TREE_OPERAND (base, 1);
  tree bytes = newtm_clone_transport_extent (r.owner, p, write);
  if (!bytes || !tree_fits_uhwi_p (bytes) || !tree_fits_shwi_p (delta)
      || tree_to_shwi (delta) < 0 || reverse || at < 0 || size <= 0
      || tree_to_shwi (delta) > HOST_WIDE_INT_MAX - displacement)
    return channel_fail (r.owner, s, "channel direction or access extent is invalid");
  displacement += tree_to_shwi (delta);
  unsigned HOST_WIDE_INT extent = tree_to_uhwi (bytes);
  if (extent > HOST_WIDE_INT_MAX / BITS_PER_UNIT
      || (unsigned HOST_WIDE_INT) displacement > extent
      || (unsigned HOST_WIDE_INT) at > (extent - displacement) * BITS_PER_UNIT
      || (unsigned HOST_WIDE_INT) size > (extent - displacement) * BITS_PER_UNIT - at)
    return channel_fail (r.owner, s, "channel access exceeds its signature extent");
  return true;
}

static bool channel_accesses (resolver &r, gimple *s)
{
  struct scan {
    resolver *r; gimple *s; tree destination; bool valid;
    static tree visit (tree *p, int *walk, void *data)
    {
      walk_stmt_info *wi = (walk_stmt_info *) data;
      scan *v = (scan *) wi->info;
      if (REFERENCE_CLASS_P (*p))
        {
          v->valid &= channel_access (*v->r, *p, v->s, *p == v->destination);
          /* The maximal reference determines the accessed extent. */
          *walk = 0;
        }
      if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
      return NULL_TREE;
    }
  } v = { &r, s, is_gimple_assign (s) ? gimple_assign_lhs (s) : NULL_TREE, true };
  if (v.destination)
    for (unsigned i = 2; i <= 4; ++i)
      if (v.destination == newtm_clone_channel (r.owner, i))
        return channel_fail (r.owner, s, "signature channel pointer is overwritten");
  walk_stmt_info wi = {}; wi.info = &v;
  walk_gimple_op (s, scan::visit, &wi);
  return v.valid;
}

static bool channel_interval_of (tree ref, channel_interval &v)
{
  bool reverse;
  v.base = ref ? get_ref_base_and_extent_hwi (ref, &v.start, &v.size, &reverse) : NULL_TREE;
  return v.base && !reverse && v.start >= 0 && v.size > 0;
}

static bool channel_buffer (resolver &r, newtm_lifetime_analysis &life,
                            gimple *s, unsigned arg, tree type,
                            unsigned HOST_WIDE_INT bytes, bool input,
                            auto_vec<channel_interval> &intervals,
                            tree *result = NULL)
{
  tree ref = channel_address (r, gimple_call_arg (s, arg), s);
  channel_interval v;
  if (!channel_interval_of (ref, v) || !VAR_P (v.base)
      || TREE_STATIC (v.base) || DECL_EXTERNAL (v.base)
      || DECL_CONTEXT (v.base) != r.owner)
    return channel_fail (r.owner, s, "buffer is not a current activation allocation");
  tree total = DECL_SIZE (v.base);
  if (!total || !tree_fits_shwi_p (total)
      || bytes > HOST_WIDE_INT_MAX / BITS_PER_UNIT
      || v.start > tree_to_shwi (total)
      || (HOST_WIDE_INT) bytes * BITS_PER_UNIT > tree_to_shwi (total) - v.start)
    return channel_fail (r.owner, s, "buffer exceeds its effective allocation extent");
  if (!types_compatible_p (TREE_TYPE (ref), type)
      || DECL_ALIGN (v.base) < TYPE_ALIGN (type)
      || v.start % TYPE_ALIGN (type))
    return channel_fail (r.owner, s, "buffer has incompatible type or alignment");
  v.size = bytes * BITS_PER_UNIT;
  for (const channel_interval &other : intervals)
    if (other.base == v.base && v.size && other.size
        && v.start < other.start + other.size && other.start < v.start + v.size)
      return channel_fail (r.owner, s, "incoming and result channels overlap");
  intervals.safe_push (v);
  life.watch (v.base); life.borrow (s, arg);
  if (input) life.input (s, ref, bytes);
  if (result) *result = ref;
  return true;
}

static bool channel_configure (resolver &r, newtm_lifetime_analysis &life, gimple *s)
{
  tree callee = gimple_call_fndecl (s);
  auto_vec<channel_interval> intervals;
  if (tree original = newtm_clone_original (callee))
    {
      tree bytes = newtm_clone_transport_extent (callee, newtm_clone_channel (callee, 2), false);
      if (!bytes || !tree_fits_uhwi_p (bytes)) return channel_fail (r.owner, s, "incoming lacks a finite signature extent");
      if (!integer_zerop (bytes))
        {
          if (!channel_buffer (r, life, s, 1, newtm_abi_clone_binding_type (), tree_to_uhwi (bytes), true, intervals)) return false;
        }
      else if (!integer_zerop (gimple_call_arg (s, 1)))
        return channel_fail (r.owner, s, "empty incoming is not null");
      tree type = TREE_TYPE (TREE_TYPE (original));
      bool pointer = TREE_CODE (type) == POINTER_TYPE;
      unsigned end = gimple_call_num_args (s) - (TREE_CODE (type) == VOID_TYPE ? 0 : pointer ? 2 : 1);
      life.effects (s, 2, end);
      if (TREE_CODE (type) != VOID_TYPE)
        {
          tree output;
          if (!TYPE_SIZE_UNIT (type) || !tree_fits_uhwi_p (TYPE_SIZE_UNIT (type))
              || !channel_buffer (r, life, s, end, type, tree_to_uhwi (TYPE_SIZE_UNIT (type)), false, intervals, &output)) return false;
          life.output (s, output, pointer, false, end);
          if (pointer)
            {
              tree binding = newtm_abi_clone_binding_type ();
              if (!channel_buffer (r, life, s, end + 1, binding,
                   tree_to_uhwi (TYPE_SIZE_UNIT (binding)), false, intervals, &output)) return false;
              life.output (s, output, false, true, end);
            }
        }
      unsigned arg = 2;
      for (tree capture = newtm_clone_captures (callee); capture; capture = TREE_CHAIN (capture), ++arg)
        if (newtm_clone_region_owner (callee) && TREE_CODE (TREE_TYPE (TREE_VALUE (capture))) == REFERENCE_TYPE)
          {
            tree ref = channel_address (r, gimple_call_arg (s, arg), s);
            if (!ref) return channel_fail (r.owner, s, "reference capture lacks a representation allocation");
            life.watch (get_base_address (ref)); life.input (s, ref); life.borrow (s, arg);
          }
    }
  else
    {
      tree bytes = gimple_call_arg (s, 4), ref = channel_address (r, gimple_call_arg (s, 3), s);
      if (!ref || !tree_fits_uhwi_p (bytes)) return channel_fail (r.owner, s, "runtime buffer lacks a finite current extent");
      tree size = TYPE_SIZE_UNIT (TREE_TYPE (ref));
      if (!size || !tree_fits_uhwi_p (size) || tree_to_uhwi (size) != tree_to_uhwi (bytes))
        return channel_fail (r.owner, s, "runtime buffer size disagrees with its value type");
      bool input = fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE);
      if (!channel_buffer (r, life, s, 3, TREE_TYPE (ref), tree_to_uhwi (bytes), input, intervals)) return false;
      if (!input) life.output (s, ref);
    }
  return true;
}
}

void newtm_memory_note (gimple *s, newtm_memory_role role, tree object,
                        tree extent, tree offset, tree size, tree kind,
                        tree storage, tree logical)
{
  if (!pending_memory) pending_memory = hash_map<gimple *, tree>::create_ggc ();
  tree tuple = make_tree_vec (TUPLE_FIELDS + 1);
  tree values[] = { object, extent, offset, size, kind, storage, logical };
  for (unsigned i = 0; i < TUPLE_FIELDS; ++i) TREE_VEC_ELT (tuple, i) = values[i];
  TREE_VEC_ELT (tuple, TUPLE_FIELDS) = number (role);
  pending_memory->put (s, tuple);
}

namespace {
enum coverage_field { COVER_ROOTS, COVER_CALLS, COVER_REFERENCES, COVER_FIELDS };
/* Declaration membership is frozen while BINDs still exist.  Artificial
   flags, debug names and ABI-compatible types never confer membership. */
static bool coverage_snapshot (tree fn, gimple_seq sequence, tree locals,
                               tree key, bool controller)
{
  tree proof = make_tree_vec (COVER_FIELDS), roots = NULL_TREE;
  if (!controller)
    for (tree d = DECL_ARGUMENTS (fn); d; d = DECL_CHAIN (d))
      roots = tree_cons (NULL_TREE, newtm_clone_memory_origin (d), roots);
  for (tree d = locals; d; d = DECL_CHAIN (d))
    roots = tree_cons (NULL_TREE, newtm_clone_memory_origin (d), roots);
  struct collect {
    static tree stmt (gimple_stmt_iterator *at, bool *, walk_stmt_info *wi)
    {
      tree *roots = (tree *) wi->info;
      if (gimple_code (gsi_stmt (*at)) == GIMPLE_BIND)
        for (tree d = gimple_bind_vars (as_a<gbind *> (gsi_stmt (*at))); d; d = DECL_CHAIN (d))
          *roots = tree_cons (NULL_TREE, newtm_clone_memory_origin (d), *roots);
      return NULL_TREE;
    }
  };
  walk_stmt_info wi = {}; wi.info = &roots;
  walk_gimple_seq (sequence, collect::stmt, NULL, &wi);
  for (tree p = roots; p; p = TREE_CHAIN (p))
    if (!TREE_VALUE (p) || !DECL_P (TREE_VALUE (p)))
      return fail (fn, NULL, "coverage local has an ambiguous declaration origin");
  TREE_VEC_ELT (proof, COVER_ROOTS) = roots;
  newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
  if (!flow.build (sequence, &why, &bad)) return fail (fn, bad, why);
  hash_set<gimple *> seen;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      {
        if (seen.add (s)) continue;
        if (is_gimple_call (s) && !gimple_call_internal_p (s))
          {
            tree lhs = gimple_call_lhs (s);
            if (lhs && !variable (lhs)) return fail (fn, s, "coverage call lacks a scalar operation identity");
            TREE_VEC_ELT (proof, COVER_CALLS) = tree_cons (NULL_TREE,
              list (symbol (lhs), gimple_call_fndecl (s), number (gimple_call_num_args (s))),
              TREE_VEC_ELT (proof, COVER_CALLS));
          }
        /* The existing C++ reference-representation exception is positional
           and source-bound, never a general permission to read captures. */
        if (controller && is_gimple_assign (s) && gimple_assign_single_p (s)
            && DECL_P (gimple_assign_rhs1 (s)) && variable (gimple_assign_rhs1 (s))
            && TREE_CODE (TREE_TYPE (gimple_assign_rhs1 (s))) == REFERENCE_TYPE)
          TREE_VEC_ELT (proof, COVER_REFERENCES) = tree_cons (symbol (gimple_assign_lhs (s)),
            newtm_clone_memory_origin (gimple_assign_rhs1 (s)), TREE_VEC_ELT (proof, COVER_REFERENCES));
      }
  hash_map<tree, tree> *&contracts = controller ? coverage_entries : coverage_contracts;
  if (!contracts) contracts = hash_map<tree, tree>::create_ggc ();
  contracts->put (key, proof);
  return true;
}
}

bool newtm_memory_snapshot (tree fn, gimple_seq sequence)
{
  newtm_current_invalidate (fn);
  struct cleanup { ~cleanup () { newtm_memory_retire (); } } release;
  newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
  if (!flow.build (sequence, &why, &bad)) return fail (fn, bad, why);
  resolver r (fn, flow);
  tree records = NULL_TREE, arithmetic = NULL_TREE;
  hash_set<gimple *> seen;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    {
      gimple *s = flow.node (i).statement;
      if (!s || seen.add (s)) continue;
      if (flow.node (i).reachable && is_gimple_assign (s) && newtm_obligation_math_p (s))
        arithmetic = tree_cons (NULL_TREE,
          list (symbol (gimple_assign_lhs (s)), r.rhs (s, 0), r.type (TREE_TYPE (gimple_assign_lhs (s)))), arithmetic);
      tree *pending = pending_memory ? pending_memory->get (s) : NULL;
      if (!pending) continue;
      tree data = *pending;
      if (flow.node (i).reachable)
        {
          tree record = make_tree_vec (CONTRACT_FIELDS);
          newtm_memory_role role = (newtm_memory_role) tree_to_uhwi (TREE_VEC_ELT (data, TUPLE_FIELDS));
          TREE_VEC_ELT (record, ROLE) = number (role);
          tree site = is_gimple_call (s) ? gimple_call_lhs (s)
            : role == NEWTM_MEMORY_PRIVATE_READ ? gimple_assign_lhs (s)
            : role == NEWTM_MEMORY_ADDRESS && is_gimple_assign (s) ? gimple_assign_lhs (s) : NULL_TREE;
          TREE_VEC_ELT (record, SITE) = site && variable (site) ? symbol (site) : NULL_TREE;
          if (tree ref = memory_operand (s, role)) TREE_VEC_ELT (record, OPERAND) = r.location (ref, s, 0);
          tree tuple = make_tree_vec (TUPLE_FIELDS);
          for (unsigned j = 0; j < TUPLE_FIELDS; ++j)
            {
              tree v = TREE_VEC_ELT (data, j);
              if (v && !variable (v) && TREE_CODE (v) != INTEGER_CST)
                return fail (fn, s, "tuple witness is not an evaluated scalar");
              TREE_VEC_ELT (tuple, j) = list (v && variable (v) ? symbol (v) : v, r.value (v, s));
            }
          TREE_VEC_ELT (record, TUPLE) = tuple;
          if (role == NEWTM_MEMORY_ADDRESS || role == NEWTM_MEMORY_PRIVATE_READ || role == NEWTM_MEMORY_PRIVATE_WRITE)
            TREE_VEC_ELT (record, EXPRESSION) = r.rhs (s, 0);
          records = tree_cons (NULL_TREE, record, records);
        }
      pending_memory->remove (s);
    }
  if (r.failure) return fail (fn, NULL, r.failure);
  if (!memory_contracts) memory_contracts = hash_map<tree, tree>::create_ggc ();
  memory_contracts->put (fn, records);
  if (!memory_arithmetic) memory_arithmetic = hash_map<tree, tree>::create_ggc ();
  memory_arithmetic->put (fn, arithmetic);
  tree channels = NULL_TREE, publications = NULL_TREE;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      if (flow.node (i).reachable)
        {
          if (channel_call (s))
            channels = tree_cons (symbol (gimple_call_lhs (s)), channel_call_term (r, s), channels);
          if (unsigned destination = channel_destination (r, s))
            publications = tree_cons (number (destination),
              list (r.location (gimple_assign_lhs (s), s, 0),
                channel_fields (r, gimple_assign_rhs1 (s), s)), publications);
        }
  if (r.failure) return channel_fail (fn, NULL, r.failure);
  if (!channel_contracts) channel_contracts = hash_map<tree, tree>::create_ggc ();
  channel_contracts->put (fn, channels);
  if (!channel_publications) channel_publications = hash_map<tree, tree>::create_ggc ();
  channel_publications->put (fn, publications);
  tree operations = NULL_TREE, finals = NULL_TREE, guards = NULL_TREE;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      if (flow.node (i).reachable)
        {
          if (status_call (s))
            operations = tree_cons (symbol (gimple_call_lhs (s)),
              list (gimple_call_fndecl (s), number (gimple_call_num_args (s))), operations);
          if (newtm_obligation_abort_p (s))
            {
              bool inverse;
              tree predicate = status_predicate (r, s, &inverse);
              tree actual = fold_build2 (gimple_cond_code (s), boolean_type_node,
                gimple_cond_lhs (s), gimple_cond_rhs (s));
              const char *unused = NULL;
              bool required = !r.guards.require (s, actual, &unused);
              guards = tree_cons (NULL_TREE, list (predicate, number (inverse), number (required)), guards);
            }
          if (unsigned destination = channel_destination (r, s))
            {
              gimple *last = status_final (flow, s);
              if (!last) return status_fail (fn, s, "publication has no final status instance");
              tree publication = list (r.location (gimple_assign_lhs (s), s, 0), channel_fields (r, gimple_assign_rhs1 (s), s));
              finals = tree_cons (number (destination), list (publication, symbol (gimple_call_lhs (last))), finals);
            }
        }
  if (!status_contracts) status_contracts = hash_map<tree, tree>::create_ggc ();
  if (!status_publications) status_publications = hash_map<tree, tree>::create_ggc ();
  if (!status_guards) status_guards = hash_map<tree, tree>::create_ggc ();
  status_contracts->put (fn, operations); status_publications->put (fn, finals);
  status_guards->put (fn, guards);
  if (r.failure) return status_fail (fn, NULL, r.failure);
  return coverage_snapshot (fn, sequence, NULL_TREE, fn, false);
}

void newtm_memory_retire ()
{ if (pending_memory) pending_memory->empty (); }

static bool memory_verify_cfg (tree fn, hash_map<gimple *, tree> *reads = NULL,
                               hash_map<gimple *, tree> *writes = NULL)
{
  newtm_current_component_begin (fn);
  tree *saved = memory_contracts ? memory_contracts->get (fn) : NULL;
  if (!saved) return fail (fn, NULL, "missing live-window memory contract");
  newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
  if (!flow.build_cfg (DECL_STRUCT_FUNCTION (fn), &why, &bad)) return fail (fn, bad, why);
  resolver r (fn, flow, true);
  if (!r.frames ()) return fail (fn, NULL, r.failure);
  tree *arithmetic = memory_arithmetic ? memory_arithmetic->get (fn) : NULL;
  if (!arithmetic) return fail (fn, NULL, "missing arithmetic provenance summary");
  hash_set<tree> math;
  for (tree p = *arithmetic; p; p = TREE_CHAIN (p))
    {
      tree record = TREE_VALUE (p), origin = TREE_VEC_ELT (record, 0);
      gimple *definition = NULL;
      for (unsigned i = 0; i < flow.node_count (); ++i)
        if (gimple *s = flow.node (i).statement)
          if (flow.node (i).reachable && is_gimple_assign (s)
              && same (symbol (gimple_assign_lhs (s)), origin))
            { if (definition && definition != s) return fail (fn, s, "duplicated identity arithmetic instance"); definition = s; }
      if (!definition || !same (r.rhs (definition, 0), TREE_VEC_ELT (record, 1))
          || !same (r.type (TREE_TYPE (gimple_assign_lhs (definition))), TREE_VEC_ELT (record, 2)))
        {
          if (flag_transaction_debug && definition)
            { print_gimple_stmt (stderr, definition, 0); difference (TREE_VEC_ELT (record, 1), r.rhs (definition, 0)); }
          return fail (fn, definition, "identity arithmetic changed its operands, index, operator or conversion");
        }
      math.add (gimple_assign_lhs (definition));
      if (!r.guards.arithmetic (definition, &why)) return fail (fn, definition, why);
    }
  hash_set<gimple *> consumed;
  unsigned count = 0;
  for (tree item = *saved; item; item = TREE_CHAIN (item))
    {
      tree proof = TREE_VALUE (item), tuple = TREE_VEC_ELT (proof, TUPLE);
      newtm_memory_role role = (newtm_memory_role) tree_to_uhwi (TREE_VEC_ELT (proof, ROLE));
      gimple *operation = NULL;
      for (unsigned i = 0; i < flow.node_count (); ++i)
        {
          gimple *s = flow.node (i).statement;
          if (!s || !flow.node (i).reachable || consumed.contains (s)) continue;
          tree site = is_gimple_call (s) ? gimple_call_lhs (s)
            : is_gimple_assign (s) ? gimple_assign_lhs (s) : NULL_TREE;
          if (TREE_VEC_ELT (proof, SITE))
            {
              if (!site || !variable (site) || !same (symbol (site), TREE_VEC_ELT (proof, SITE))) continue;
            }
          else
            {
              tree ref = memory_operand (s, role);
              if (ref && !DECL_P (ref) && !REFERENCE_CLASS_P (ref)) continue;
              if (!ref || !same (r.location (ref, s, 0), TREE_VEC_ELT (proof, OPERAND))) continue;
            }
          if (TREE_VEC_ELT (proof, EXPRESSION))
            {
              if (!is_gimple_assign (s)) continue;
              if (!same (r.rhs (s, 0), TREE_VEC_ELT (proof, EXPRESSION)))
                {
                  if (flag_transaction_debug && TREE_VEC_ELT (proof, SITE))
                    { print_gimple_stmt (stderr, s, 0); difference (TREE_VEC_ELT (proof, EXPRESSION), r.rhs (s, 0)); }
                  continue;
                }
              bool matched = true;
              for (unsigned j = 0; j < TUPLE_FIELDS; ++j)
                {
                  tree field = TREE_VEC_ELT (tuple, j);
                  tree v = r.witness (TREE_VEC_ELT (field, 0));
                  if ((TREE_VEC_ELT (field, 0) && !v) || !same (r.value (v, s), TREE_VEC_ELT (field, 1))) matched = false;
                }
              if (!matched) continue;
            }
          if (operation && role <= NEWTM_MEMORY_WRITE)
            return fail (fn, s, "ambiguous current memory operation instance");
          operation = s;
          /* Equal private effects form a multiset.  Their reaching values,
             complete identity tuples and guards are still checked at the
             selected operation; matching an address alone is insufficient. */
          if (role > NEWTM_MEMORY_WRITE) break;
        }
      if (!operation)
        {
          if (flag_transaction_debug)
            {
              fprintf (stderr, "NEWTM missing memory role=%u site=", (unsigned) role);
              print_generic_expr (stderr, TREE_VEC_ELT (proof, SITE));
              fputs (" operand=", stderr); print_generic_expr (stderr, TREE_VEC_ELT (proof, OPERAND)); fputc ('\n', stderr);
              for (unsigned i = 0; i < flow.node_count (); ++i)
                if (gimple *s = flow.node (i).statement)
                  if (tree ref = memory_operand (s, role))
                    if (REFERENCE_CLASS_P (ref)) { print_gimple_stmt (stderr, s, 0); print_generic_expr (stderr, r.location (ref, s, 0)); fputc ('\n', stderr); }
            }
          bool private_access = role == NEWTM_MEMORY_PRIVATE_READ || role == NEWTM_MEMORY_PRIVATE_WRITE;
          return fail (fn, NULL, "memory operation lost its current instance",
                       private_access ? "V04" : "V01",
                       role == NEWTM_MEMORY_PRIVATE_READ ? "PRIVATE read"
                       : role == NEWTM_MEMORY_PRIVATE_WRITE ? "PRIVATE write"
                       : role == NEWTM_MEMORY_WRITE ? "WRITE" : "memory operation");
        }
      consumed.add (operation);
      tree values[TUPLE_FIELDS];
      for (unsigned j = 0; j < TUPLE_FIELDS; ++j)
        {
          tree field = TREE_VEC_ELT (tuple, j);
          values[j] = r.witness (TREE_VEC_ELT (field, 0));
          if ((TREE_VEC_ELT (field, 0) && !values[j])
              || !same (r.value (values[j], operation), TREE_VEC_ELT (field, 1)))
            return fail (fn, operation, "incoherent identity tuple, storage or reaching value");
        }
      if (role == NEWTM_MEMORY_REGISTER || role == NEWTM_MEMORY_READ || role == NEWTM_MEMORY_WRITE)
        {
          unsigned op = role == NEWTM_MEMORY_REGISTER ? BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER
            : role == NEWTM_MEMORY_READ ? BUILT_IN_GOMP_TRANSACTION_STM_V1_READ : BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE;
          tree callee = is_gimple_call (operation) ? gimple_call_fndecl (operation) : NULL_TREE;
          if (!callee || !fndecl_built_in_p (callee, (built_in_function) op))
            return fail (fn, operation, "memory operation changed its role", "V01",
                         role == NEWTM_MEMORY_WRITE ? "WRITE" : "memory operation");
          unsigned fields[] = { OBJECT, role == NEWTM_MEMORY_REGISTER ? EXTENT : OFFSET,
                                role == NEWTM_MEMORY_REGISTER ? KIND : SIZE };
          unsigned args[] = { 1, 2, role == NEWTM_MEMORY_REGISTER ? 3u : 4u };
          for (unsigned i = 0; i < 3; ++i)
            if (!same (r.value (gimple_call_arg (operation, args[i]), operation), TREE_VEC_ELT (TREE_VEC_ELT (tuple, fields[i]), 1)))
              return fail (fn, operation, "memory operands no longer denote their logical tuple");
          r.dependencies |= NEWTM_MEMORY_STATUS;
        }
      else if (role == NEWTM_MEMORY_ADDRESS)
        {
          if (!is_gimple_assign (operation) || !same (r.rhs (operation, 0), TREE_VEC_ELT (proof, EXPRESSION)))
            return fail (fn, operation, "address expression changed its logical index or conversion");
        }
      else
        {
          tree ref = memory_operand (operation, role);
          if (!ref || !same (r.location (ref, operation, 0), TREE_VEC_ELT (proof, OPERAND)))
            return fail (fn, operation, "private access changed its object or frame instance", "V04", "PRIVATE access");
        }
      if (!r.ranges (operation, values, role))
        return memory_range_fail (r, operation);
      for (unsigned j = 0; j < TUPLE_FIELDS; ++j)
        if (!r.arithmetic_dependencies (operation, values[j], math)) return fail (fn, operation, r.failure);
      if (role == NEWTM_MEMORY_ADDRESS && is_gimple_assign (operation))
        for (unsigned j = 1; j < gimple_num_ops (operation); ++j)
          if (!r.arithmetic_dependencies (operation, gimple_op (operation, j), math)) return fail (fn, operation, r.failure);
      if (role == NEWTM_MEMORY_PRIVATE_READ && reads)
        reads->put (operation, memory_operand (operation, role));
      if (role == NEWTM_MEMORY_PRIVATE_WRITE && writes)
        writes->put (operation, memory_operand (operation, role));
      ++count;
    }
  if (r.failure) return fail (fn, NULL, r.failure);
  if (!memory_results) memory_results = hash_map<tree, tree>::create_ggc ();
  memory_results->put (fn, list (number (NEWTM_MEMORY_DEPENDENT), number (count), number (r.dependencies)));
  return true;
}

bool newtm_memory_verify_cfg (tree fn) { return memory_verify_cfg (fn); }

bool newtm_memory_get (tree fn, newtm_memory_view *out)
{
  tree *p = memory_results ? memory_results->get (fn) : NULL;
  if (!p || !out) return false;
  out->state = (newtm_memory_state) tree_to_uhwi (TREE_VEC_ELT (*p, 0));
  out->operations = tree_to_uhwi (TREE_VEC_ELT (*p, 1));
  out->dependencies = tree_to_uhwi (TREE_VEC_ELT (*p, 2));
  return true;
}

bool newtm_channels_verify_cfg (tree fn)
{
  newtm_current_component_begin (fn);
  if (!channel_results) channel_results = hash_map<tree, tree>::create_ggc ();
  channel_results->put (fn, list (number (NEWTM_MEMORY_REFUSED), number (0), number (0)));
  tree *saved = channel_contracts ? channel_contracts->get (fn) : NULL;
  if (!saved) return channel_fail (fn, NULL, "missing live-window channel contract");
  newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
  if (!flow.build_cfg (DECL_STRUCT_FUNCTION (fn), &why, &bad)) return channel_fail (fn, bad, why);
  resolver r (fn, flow, true);
  newtm_lifetime_analysis life (flow, fn, true);
  for (unsigned channel = 2; channel <= 4; ++channel)
    if (tree p = newtm_clone_channel (fn, channel)) life.borrowed (p);
  if (tree p = newtm_clone_channel (fn, 2)) life.readonly (p);
  for (auto entry : r.current_decls)
    {
      tree d = entry.second;
      if (!d || d == error_mark_node || !VAR_P (d)) continue;
      tree type = TREE_TYPE (d);
      while (TREE_CODE (type) == ARRAY_TYPE) type = TREE_TYPE (type);
      if (type == newtm_abi_clone_binding_type () || type == newtm_abi_binding_type ()) life.watch (d);
    }
  hash_set<gimple *> consumed;
  for (tree p = *saved; p; p = TREE_CHAIN (p))
    {
      gimple *found = NULL;
      for (unsigned i = 0; i < flow.node_count (); ++i)
        if (gimple *s = flow.node (i).statement)
          if (flow.node (i).reachable && channel_call (s)
              && same (symbol (gimple_call_lhs (s)), TREE_PURPOSE (p)))
            { if (found) return channel_fail (fn, s, "duplicated channel operation instance"); found = s; }
      if (!found || consumed.add (found)) return channel_fail (fn, found, "channel operation lost its current instance");
      tree actual = channel_call_term (r, found);
      if (r.failure || !same (actual, TREE_VALUE (p)))
        {
          if (flag_transaction_debug) difference (TREE_VALUE (p), actual);
          return channel_fail (fn, found, r.failure ? r.failure : "channel arguments, slots or reaching producers changed");
        }
      if (!channel_configure (r, life, found)) return false;
    }
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      {
        if (!channel_accesses (r, s)) return false;
        if (flow.node (i).reachable && channel_call (s) && !consumed.contains (s))
          return channel_fail (fn, s, "unmatched channel operation");
      }
  tree *publications = channel_publications ? channel_publications->get (fn) : NULL;
  if (!publications) return channel_fail (fn, NULL, "missing result production contract");
  hash_set<gimple *> published;
  for (tree p = *publications; p; p = TREE_CHAIN (p))
    {
      gimple *found = NULL;
      for (unsigned i = 0; i < flow.node_count (); ++i)
        if (gimple *s = flow.node (i).statement)
          if (flow.node (i).reachable && !published.contains (s)
              && channel_destination (r, s) == tree_to_uhwi (TREE_PURPOSE (p)))
            {
              tree actual = list (r.location (gimple_assign_lhs (s), s, 0), channel_fields (r, gimple_assign_rhs1 (s), s));
              if (same (actual, TREE_VALUE (p))) { found = s; break; }
              if (flag_transaction_debug) difference (TREE_VALUE (p), actual);
            }
      if (!found) return channel_fail (fn, NULL, "result value/binding lost its reaching producer");
      published.add (found);
    }
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      if (flow.node (i).reachable && channel_destination (r, s) && !published.contains (s))
        return channel_fail (fn, s, "unmatched result publication");
  if (!life.verify (&why, &bad)) return channel_fail (fn, bad, why);
  channel_results->put (fn, list (number (NEWTM_MEMORY_DEPENDENT),
    number (consumed.elements () + published.elements ()),
    number (NEWTM_MEMORY_STATUS | NEWTM_MEMORY_OWNER)));
  return true;
}

/* The entry is located by the actual unique outlined call, not by a retained
   instruction anchor. Its captures are independently matched by block 1. */
bool newtm_channels_verify_entry (tree fn, gimple *call)
{
  newtm_current_component_begin (fn);
  newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
  if (!flow.build_cfg (DECL_STRUCT_FUNCTION (fn), &why, &bad, true)) return channel_fail (fn, bad, why);
  resolver r (fn, flow, true);
  newtm_lifetime_analysis life (flow, fn, true);
  life.transport_only ();
  tree *entry = channel_entries ? channel_entries->get (gimple_call_fndecl (call)) : NULL;
  if (!entry) return channel_fail (fn, call, "missing controller incoming production contract");
  r.proof_owner = TREE_VEC_ELT (*entry, 0);
  r.entry_frame = newtm_clone_entry_frame (fn, call);
  tree current = channel_call_term (r, call);
  if (r.failure) return channel_fail (fn, call, r.failure);
  tree incoming = list (TREE_VEC_ELT (TREE_VEC_ELT (current, 1), 1), TREE_VEC_ELT (current, 2));
  if (!same_entry (TREE_VEC_ELT (*entry, 1), incoming))
    {
      if (flag_transaction_debug) difference (TREE_VEC_ELT (*entry, 1), incoming);
      return channel_fail (fn, call, r.failure ? r.failure : "controller incoming changed its slot producers");
    }
  if (!channel_configure (r, life, call)) return false;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      if (is_gimple_call (s) && !gimple_call_internal_p (s))
        {
          tree callee = gimple_call_fndecl (s);
          if (callee && fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN))
            {
              tree ref = channel_address (r, gimple_call_arg (s, 1), s);
              if (!ref) return channel_fail (fn, s, "BEGIN has no outermost output buffer");
              life.borrow (s, 1); life.output (s, ref);
            }
        }
  return life.verify (&why, &bad) || channel_fail (fn, bad, why);
}

bool newtm_channels_snapshot_entry (tree fn, gimple_seq sequence, gimple *call)
{
  newtm_current_invalidate (fn);
  newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
  if (!flow.build (sequence, &why, &bad)) return channel_fail (fn, bad, why);
  resolver r (fn, flow);
  tree value = channel_call_term (r, call);
  if (r.failure) return channel_fail (fn, call, r.failure);
  if (!channel_entries) channel_entries = hash_map<tree, tree>::create_ggc ();
  channel_entries->put (gimple_call_fndecl (call),
    list (fn, list (TREE_VEC_ELT (TREE_VEC_ELT (value, 1), 1), TREE_VEC_ELT (value, 2))));
  return true;
}

bool newtm_channels_get (tree fn, newtm_memory_view *out)
{
  tree *p = channel_results ? channel_results->get (fn) : NULL;
  if (!p || !out) return false;
  out->state = (newtm_memory_state) tree_to_uhwi (TREE_VEC_ELT (*p, 0));
  out->operations = tree_to_uhwi (TREE_VEC_ELT (*p, 1));
  out->dependencies = tree_to_uhwi (TREE_VEC_ELT (*p, 2));
  return true;
}

namespace {
static bool status_private (resolver &r, newtm_status_analysis &status,
                             gimple *call, tree result)
{
  tree *saved = memory_contracts ? memory_contracts->get (r.owner) : NULL;
  if (!saved) return false;
  tree runtime = NULL_TREE;
  for (tree p = *saved; p; p = TREE_CHAIN (p))
    if (tree_to_uhwi (TREE_VEC_ELT (TREE_VALUE (p), ROLE)) == NEWTM_MEMORY_READ
        && same (TREE_VEC_ELT (TREE_VALUE (p), SITE), symbol (gimple_call_lhs (call))))
      runtime = TREE_VEC_ELT (TREE_VALUE (p), TUPLE);
  if (!runtime) return false;
  for (unsigned i = 0; i < r.flow.node_count (); ++i)
    if (gimple *s = r.flow.node (i).statement)
      if (is_gimple_assign (s) && gimple_assign_single_p (s)
          && gimple_assign_lhs (s) == result && !gimple_clobber_p (s))
        {
          gimple *load = s;
          for (unsigned depth = 0; depth < 64; ++depth)
            {
              bool matched = false;
              for (tree p = *saved; p; p = TREE_CHAIN (p))
                {
                  tree proof = TREE_VALUE (p);
                  if (tree_to_uhwi (TREE_VEC_ELT (proof, ROLE)) != NEWTM_MEMORY_PRIVATE_READ
                      || !same (symbol (gimple_assign_lhs (load)), TREE_VEC_ELT (proof, SITE))
                      || !same (r.rhs (load, 0), TREE_VEC_ELT (proof, EXPRESSION))) continue;
                  tree values[TUPLE_FIELDS], tuple = TREE_VEC_ELT (proof, TUPLE);
                  bool valid = true;
                  for (unsigned j = 0; j < TUPLE_FIELDS; ++j)
                    {
                      tree field = TREE_VEC_ELT (tuple, j);
                      if (j < LOGICAL)
                        valid &= same (TREE_VEC_ELT (field, 0), TREE_VEC_ELT (TREE_VEC_ELT (runtime, j), 0));
                      values[j] = r.witness (TREE_VEC_ELT (field, 0));
                      valid &= same (r.value (values[j], load), TREE_VEC_ELT (field, 1));
                    }
                  if (valid && r.ranges (load, values, NEWTM_MEMORY_PRIVATE_READ))
                    { status.alternative (call, s); matched = true; }
                }
              if (matched) break;
              tree rhs = gimple_assign_rhs1 (load);
              gimple *d = variable (rhs) ? r.guards.definition_at (load, rhs) : NULL;
              if (!d || !is_gimple_assign (d) || !gimple_assign_single_p (d)) break;
              load = d;
            }
        }
  return !r.failure;
}

static bool status_configure (resolver &r, newtm_status_analysis &status, gimple *s)
{
  tree callee = gimple_call_fndecl (s), result = NULL_TREE, binding = NULL_TREE;
  if (tree original = newtm_clone_original (callee))
    {
      tree type = TREE_TYPE (TREE_TYPE (original));
      if (TREE_CODE (type) != VOID_TYPE)
        {
          bool pointer = TREE_CODE (type) == POINTER_TYPE;
          unsigned arg = gimple_call_num_args (s) - (pointer ? 2 : 1);
          result = channel_address (r, gimple_call_arg (s, arg), s);
          if (pointer) binding = channel_address (r, gimple_call_arg (s, arg + 1), s);
          if (!result || (pointer && !binding)) return status_fail (r.owner, s, "call outputs have no current allocation");
        }
    }
  else if (fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE)
           || fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_DISCARD))
    { status.reset (s); return true; }
  else if (fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL)) return true;
  else if (fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_READ))
    result = channel_address (r, gimple_call_arg (s, 3), s);
  else if (fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN))
    result = channel_address (r, gimple_call_arg (s, 1), s);
  if (!result && (fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_READ)
                  || fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN)))
    return status_fail (r.owner, s, "runtime output lacks a current allocation");
  /* This availability domain tracks complete local buffer objects.  A
     subobject must never silently become an untracked output: byte-range
     ownership would require a separate proof, not just an ABI pointer type. */
  if ((result && (!VAR_P (result) || DECL_CONTEXT (result) != r.owner))
      || (binding && (!VAR_P (binding) || DECL_CONTEXT (binding) != r.owner)))
    return status_fail (r.owner, s, "output is not a resolved local buffer object");
  if (!status.operation (s, result, binding)) return false;
  if (result && fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_READ))
    return status_private (r, status, s, result);
  return true;
}

static bool status_no_later_call (const newtm_flow_graph &flow, gimple *last, gimple *use)
{
  auto_vec<unsigned char> seen; seen.safe_grow_cleared (flow.node_count ());
  auto_vec<unsigned> work;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (flow.node (i).statement == use)
      for (unsigned p : flow.node (i).predecessors) work.safe_push (p);
  for (unsigned at = 0; at < work.length (); ++at)
    {
      unsigned n = work[at]; if (seen[n]) continue; seen[n] = 1;
      gimple *s = flow.node (n).statement;
      if (s == last) continue;
      if (s && is_gimple_call (s) && !gimple_call_internal_p (s)) return false;
      for (unsigned p : flow.node (n).predecessors) work.safe_push (p);
    }
  return true;
}
}

bool newtm_status_verify_cfg (tree fn)
{
  newtm_current_component_begin (fn);
  if (!status_results) status_results = hash_map<tree, tree>::create_ggc ();
  status_results->put (fn, list (number (NEWTM_MEMORY_REFUSED), number (0), number (0)));
  tree *saved = status_contracts ? status_contracts->get (fn) : NULL;
  tree *finals = status_publications ? status_publications->get (fn) : NULL;
  tree *guards = status_guards ? status_guards->get (fn) : NULL;
  if (!saved || !finals || !guards) return status_fail (fn, NULL, "missing live-window status obligations");
  newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
  if (!flow.build_cfg (DECL_STRUCT_FUNCTION (fn), &why, &bad)) return status_fail (fn, bad, why);
  resolver r (fn, flow, true);
  newtm_status_analysis status (flow);
  status.strict_outputs ();
  newtm_failure_analysis failure (flow, newtm_clone_context (fn));
  newtm_lifetime_analysis life (flow, fn, true);
  life.publication (newtm_clone_channel (fn, 3), newtm_clone_channel (fn, 4));
  for (unsigned channel = 2; channel <= 4; ++channel)
    if (tree p = newtm_clone_channel (fn, channel)) life.borrowed (p);
  hash_set<gimple *> consumed;
  for (tree p = *saved; p; p = TREE_CHAIN (p))
    {
      gimple *found = NULL;
      for (unsigned i = 0; i < flow.node_count (); ++i)
        if (gimple *s = flow.node (i).statement)
          if (flow.node (i).reachable && status_call (s)
              && same (symbol (gimple_call_lhs (s)), TREE_PURPOSE (p))
              && gimple_call_fndecl (s) == TREE_VEC_ELT (TREE_VALUE (p), 0))
            { if (found) return status_fail (fn, s, "status operation has ambiguous current instances"); found = s; }
      if (!found || consumed.add (found)) return status_fail (fn, found, "status operation lost its current instance");
      if (gimple_call_num_args (found) != tree_to_uhwi (TREE_VEC_ELT (TREE_VALUE (p), 1)))
        return status_fail (fn, found, "status operation changed its signature");
      if (!status_configure (r, status, found))
        {
          if (r.failure && r.failure_rule == NEWTM_V04)
            {
              status_fail (fn, found, r.failure, r.failure_rule_name ());
              if (r.failure_detail)
                inform (newtm_current_diagnostic_location (fn, found),
                        "underlying guard analysis: %s", r.failure_detail);
              return false;
            }
          return status_fail (fn, found, "status output or PRIVATE alternative is not proven");
        }
      if (channel_call (found) && !channel_configure (r, life, found)) return false;
    }
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      if (flow.node (i).reachable && status_call (s) && !consumed.contains (s))
        return status_fail (fn, s, "unmatched current status operation");
  for (tree p = *guards; p; p = TREE_CHAIN (p))
    {
      bool found = false;
      for (unsigned i = 0; i < flow.node_count (); ++i)
        if (gimple *s = flow.node (i).statement)
          if (flow.node (i).reachable && gimple_code (s) == GIMPLE_COND)
            {
              bool inverse; tree predicate = status_predicate (r, s, &inverse);
              if (same (predicate, TREE_VEC_ELT (TREE_VALUE (p), 0)))
                { failure.invalid (s, inverse != bool (tree_to_uhwi (TREE_VEC_ELT (TREE_VALUE (p), 1)))); found = true; }
            }
      if (!found && !integer_zerop (TREE_VEC_ELT (TREE_VALUE (p), 2)))
        return status_fail (fn, NULL, "required validation decision disappeared");
    }
  for (tree p = *finals; p; p = TREE_CHAIN (p))
    {
      gimple *last = NULL; tree site = TREE_VEC_ELT (TREE_VALUE (p), 1);
      for (unsigned i = 0; i < flow.node_count (); ++i)
        if (gimple *s = flow.node (i).statement)
          if (is_gimple_call (s) && same (symbol (gimple_call_lhs (s)), site))
            { if (last) return status_fail (fn, s, "publication final status has ambiguous instances"); last = s; }
      bool found = false;
      for (unsigned i = 0; i < flow.node_count (); ++i)
        if (gimple *s = flow.node (i).statement)
          if (channel_destination (r, s) == tree_to_uhwi (TREE_PURPOSE (p)))
            {
              tree current = list (r.location (gimple_assign_lhs (s), s, 0), channel_fields (r, gimple_assign_rhs1 (s), s));
              if (!same (current, TREE_VEC_ELT (TREE_VALUE (p), 0))) continue;
              if (!last || !status_dominates (flow, last, s) || !status_no_later_call (flow, last, s))
                return status_fail (fn, s, "publication does not follow its own final status operation");
              status.require_success (last, s); found = true;
              if (VAR_P (gimple_assign_rhs1 (s))) life.watch (gimple_assign_rhs1 (s));
            }
      if (!found) return status_fail (fn, NULL, "publication lost its current value/binding instance");
    }
  if (!status.verify (&why, &bad) || !failure.verify (&why, &bad) || !life.verify (&why, &bad))
    return status_fail (fn, bad, why);
  status_results->put (fn, list (number (NEWTM_MEMORY_DEPENDENT), number (consumed.elements ()), number (NEWTM_MEMORY_OWNER)));
  return true;
}

bool newtm_status_verify_entry (tree fn, gimple *)
{
  newtm_current_component_begin (fn);
  newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
  if (!flow.build_cfg (DECL_STRUCT_FUNCTION (fn), &why, &bad, true)) return status_fail (fn, bad, why);
  resolver r (fn, flow, true); newtm_status_analysis status (flow);
  status.strict_outputs ();
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      if (status_call (s) && !status_configure (r, status, s)) return status_fail (fn, s, "controller status has no current definition");
  return status.verify (&why, &bad) || status_fail (fn, bad, why);
}

bool newtm_status_get (tree fn, newtm_memory_view *out)
{
  tree *p = status_results ? status_results->get (fn) : NULL;
  if (!p || !out) return false;
  out->state = (newtm_memory_state) tree_to_uhwi (TREE_VEC_ELT (*p, 0));
  out->operations = tree_to_uhwi (TREE_VEC_ELT (*p, 1));
  out->dependencies = tree_to_uhwi (TREE_VEC_ELT (*p, 2));
  return true;
}

namespace {
enum owner_field { OWNER_FN, OWNER_VARS, OWNER_SLOT, OWNER_COUNTER,
                   OWNER_GUARDS, OWNER_FIELDS };
static bool owner_fail (tree fn, gimple *s, const char *why)
{
  newtm_current_diagnose (fn, s, "V07", "owner", why);
  return false;
}
static bool owner_builtin (gimple *s, built_in_function code)
{ return s && is_gimple_call (s) && gimple_call_fndecl (s)
    && fndecl_built_in_p (gimple_call_fndecl (s), code); }
}

bool newtm_owner_snapshot_entry (tree fn, gimple_seq sequence, gimple *call,
                                 tree vars)
{
  newtm_current_invalidate (fn);
  newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
  if (!flow.build (sequence, &why, &bad)) return owner_fail (fn, bad, why);
  resolver r (fn, flow);
  tree proof = make_tree_vec (OWNER_FIELDS), declarations = NULL_TREE, guards = NULL_TREE;
  TREE_VEC_ELT (proof, OWNER_FN) = fn;
  for (tree d = vars; d; d = DECL_CHAIN (d))
    declarations = tree_cons (NULL_TREE, newtm_clone_memory_origin (d), declarations);
  TREE_VEC_ELT (proof, OWNER_VARS) = declarations;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      {
        if (owner_builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN))
          TREE_VEC_ELT (proof, OWNER_SLOT) = newtm_clone_memory_origin (
            get_base_address (TREE_OPERAND (gimple_call_arg (s, 0), 0)));
        if (owner_builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE))
          TREE_VEC_ELT (proof, OWNER_COUNTER) = newtm_clone_memory_origin (gimple_call_arg (s, 1));
        if (newtm_obligation_abort_p (s))
          {
            bool inverse; tree predicate = status_predicate (r, s, &inverse);
            tree actual = fold_build2 (gimple_cond_code (s), boolean_type_node,
                                      gimple_cond_lhs (s), gimple_cond_rhs (s));
            const char *unused = NULL;
            bool required = !r.guards.require (s, actual, &unused);
            guards = tree_cons (NULL_TREE, list (predicate, number (inverse), number (required)), guards);
          }
      }
  TREE_VEC_ELT (proof, OWNER_GUARDS) = guards;
  if (r.failure) return owner_fail (fn, call, r.failure);
  if (!owner_entries) owner_entries = hash_map<tree, tree>::create_ggc ();
  owner_entries->put (gimple_call_fndecl (call), proof);
  return coverage_snapshot (fn, sequence, vars, gimple_call_fndecl (call), true);
}

static bool owner_region (tree fn, gimple *call, tree proof,
                          newtm_flow_graph &whole, newtm_flow_graph &flow)
{
  const char *why = NULL; gimple *bad = NULL;
  if (!whole.build_cfg (DECL_STRUCT_FUNCTION (fn), &why, &bad, true)) return owner_fail (fn, bad, why);
  unsigned start = UINT_MAX, end = UINT_MAX;
  for (unsigned i = 0; i < whole.node_count (); ++i)
    if (gimple *s = whole.node (i).statement)
      {
        if (is_gimple_assign (s) && gimple_clobber_p (s, CLOBBER_STORAGE_BEGIN)
            && VAR_P (gimple_assign_lhs (s))
            && newtm_clone_memory_origin (gimple_assign_lhs (s)) == TREE_VEC_ELT (proof, OWNER_SLOT))
          { if (start != UINT_MAX) return owner_fail (fn, s, "owning slot has multiple lifetime entry instances"); start = i; }
        if (is_gimple_assign (s) && gimple_clobber_p (s, CLOBBER_STORAGE_END)
            && VAR_P (gimple_assign_lhs (s))
            && newtm_clone_memory_origin (gimple_assign_lhs (s)) == TREE_VEC_ELT (proof, OWNER_SLOT))
          {
            if (end != UINT_MAX)
              return owner_fail (fn, s, "owning lifetime has multiple current boundaries");
            end = i;
          }
      }
  if (start == UINT_MAX || end == UINT_MAX)
    return owner_fail (fn, call, "controller lost its current initialization or lifetime boundary");
  if (!whole.node (start).reachable || !whole.node (end).reachable || !whole.reachable (call))
    return owner_fail (fn, call, "controller initialization, body or continuation is unreachable");
  if (DECL_CONTEXT (gimple_assign_lhs (whole.node (start).statement)) != fn)
    return owner_fail (fn, call, "owning storage belongs to a different current function frame");
  if (!flow.build_region (whole, start, end, &why, &bad)) return owner_fail (fn, bad, why);
  if (!flow.contains (call)) return owner_fail (fn, call, "delegated body lies outside its owning controller");
  return true;
}

bool newtm_owner_verify_entry (tree fn, gimple *call)
{
  newtm_current_component_begin (fn);
  tree helper = call && is_gimple_call (call) ? gimple_call_fndecl (call) : NULL_TREE;
  if (!owner_results) owner_results = hash_map<tree, tree>::create_ggc ();
  if (helper) owner_results->put (helper, list (number (NEWTM_MEMORY_REFUSED), number (0), number (0)));
  tree *saved = owner_entries && helper ? owner_entries->get (helper) : NULL;
  if (!saved) return owner_fail (fn, call, "missing live-window controller contract");
  tree proof = *saved;
  const char *why = NULL; gimple *bad = NULL;
  newtm_flow_graph whole, flow;
  if (!owner_region (fn, call, proof, whole, flow)) return false;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (gimple *s = flow.node (i).statement)
      if (owner_builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE)
          && (gimple_call_num_args (s) != 2
              || newtm_clone_memory_origin (gimple_call_arg (s, 1)) != TREE_VEC_ELT (proof, OWNER_COUNTER)))
        return owner_fail (fn, s, "cleanup changes the identity of its conflict counter");
  resolver r (fn, flow, true); r.proof_owner = TREE_VEC_ELT (proof, OWNER_FN);
  r.entry_frame = newtm_clone_entry_frame (fn, call);
  newtm_owner_analysis owner (flow, helper, true);
  for (tree p = TREE_VEC_ELT (proof, OWNER_GUARDS); p; p = TREE_CHAIN (p))
    {
      bool found = false;
      for (unsigned i = 0; i < flow.node_count (); ++i)
        if (gimple *s = flow.node (i).statement)
          if (gimple_code (s) == GIMPLE_COND)
            {
              bool inverse; tree predicate = status_predicate (r, s, &inverse);
              if (same (predicate, TREE_VEC_ELT (TREE_VALUE (p), 0)))
                { owner.invalid (s, inverse != bool (tree_to_uhwi (TREE_VEC_ELT (TREE_VALUE (p), 1)))); found = true; }
            }
      if (!found && !integer_zerop (TREE_VEC_ELT (TREE_VALUE (p), 2)))
        return owner_fail (fn, call, "required controller validation disappeared");
    }
  if (r.failure) return owner_fail (fn, call, r.failure);
  if (!owner.verify (&why, &bad)) return owner_fail (fn, bad, why);
  /* Nothing may use a controller carrier outside the delimited protocol.
     EOL clobbers are allowed after the successful continuation.  Operands are
     inspected independently of TREE_ADDRESSABLE and debug names. */
  hash_set<tree> private_decls;
  for (tree p = TREE_VEC_ELT (proof, OWNER_VARS); p; p = TREE_CHAIN (p)) private_decls.add (TREE_VALUE (p));
  for (unsigned i = 0; i < whole.node_count (); ++i)
    if (gimple *s = whole.node (i).statement)
      if (!flow.contains (s) && !is_gimple_debug (s)
          && !(is_gimple_assign (s) && gimple_clobber_p (s)
               && VAR_P (gimple_assign_lhs (s))))
        {
          struct scan {
            hash_set<tree> &decls;
            newtm_owner_analysis &owner;
            static tree visit (tree *p, int *walk, void *data)
            {
              walk_stmt_info *wi = (walk_stmt_info *) data;
              scan *check = (scan *) wi->info;
              if (variable (*p) && (check->decls.contains (newtm_clone_memory_origin (*p))
                                    || check->owner.protects (*p))) return *p;
              if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
              return NULL_TREE;
            }
          };
          scan check = { private_decls, owner };
          walk_stmt_info wi = {}; wi.info = &check;
          if (walk_gimple_op (s, scan::visit, &wi))
            return owner_fail (fn, s, "controller carrier is used outside its owning protocol");
        }
  unsigned operations = 0;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    if (flow.node (i).statement && is_gimple_call (flow.node (i).statement)) ++operations;
  owner_results->put (helper, list (number (NEWTM_MEMORY_DEPENDENT), number (operations),
    number (NEWTM_MEMORY_CHANNELS | NEWTM_MEMORY_STATUS)));
  return true;
}

bool newtm_owner_get (tree helper, newtm_memory_view *out)
{
  tree *p = owner_results ? owner_results->get (helper) : NULL;
  if (!p || !out) return false;
  out->state = (newtm_memory_state) tree_to_uhwi (TREE_VEC_ELT (*p, 0));
  out->operations = tree_to_uhwi (TREE_VEC_ELT (*p, 1));
  out->dependencies = tree_to_uhwi (TREE_VEC_ELT (*p, 2));
  return true;
}
namespace {
static bool coverage_fail (tree fn, gimple *s, const char *why)
{
  newtm_current_diagnose (fn, s, "V02", "coverage", why);
  if (flag_transaction_debug && s) print_gimple_stmt (stderr, s, 0);
  return false;
}

struct coverage
{
  resolver &r;
  tree proof, helper;
  hash_set<tree> roots, calls;
  hash_map<gimple *, tree> *reads, *writes;
  unsigned effects = 0;
  coverage (resolver &resolver, tree p, tree h,
            hash_map<gimple *, tree> *rd, hash_map<gimple *, tree> *wr)
    : r (resolver), proof (p), helper (h), reads (rd), writes (wr)
  { for (tree q = TREE_VEC_ELT (p, COVER_ROOTS); q; q = TREE_CHAIN (q)) roots.add (TREE_VALUE (q)); }
  bool refuse (gimple *s, const char *why) { return coverage_fail (r.owner, s, why); }
  bool local (tree d)
  { return d && (VAR_P (d) || TREE_CODE (d) == PARM_DECL)
      && DECL_CONTEXT (d) == r.owner && !TREE_STATIC (d) && !DECL_EXTERNAL (d); }
  bool member (tree d)
  { return d && roots.contains (newtm_clone_memory_origin (d)); }
  tree frame_record (tree base)
  {
    if (!base || !local (base) || !VAR_P (base)
        || TREE_CODE (TREE_TYPE (base)) != RECORD_TYPE) return NULL_TREE;
    tree record = newtm_clone_native_frame (base);
    if (!record || newtm_clone_memory_origin (TREE_VEC_ELT (record, 0)) != newtm_clone_memory_origin (r.owner)) return NULL_TREE;
    for (tree f = TYPE_FIELDS (TREE_TYPE (base)); f; f = DECL_CHAIN (f))
      if (TREE_CODE (f) == FIELD_DECL && f != TREE_VEC_ELT (record, 1) && f != TREE_VEC_ELT (record, 2))
        {
          tree source = newtm_clone_memory_field (f);
          if (!source || !member (TREE_VALUE (source))) return NULL_TREE;
        }
    return record;
  }
  bool cfa (gimple *s)
  {
    tree lhs = gimple_call_lhs (s);
    if (helper || !lhs || TREE_TYPE (lhs) != ptr_type_node || gimple_call_num_args (s) != 1
        || !integer_zerop (gimple_call_arg (s, 0))) return false;
    for (unsigned i = 0; i < r.flow.node_count (); ++i)
      if (gimple *use = r.flow.node (i).statement)
        if (is_gimple_assign (use) && gimple_assign_single_p (use)
            && TREE_CODE (gimple_assign_lhs (use)) == COMPONENT_REF)
          {
            tree ref = gimple_assign_lhs (use), record = frame_record (TREE_OPERAND (ref, 0));
            if (record && TREE_OPERAND (ref, 1) == TREE_VEC_ELT (record, 1)
                && gimple_assign_rhs1 (use) == lhs && r.guards.definition_at (use, lhs) == s)
              return true;
          }
    return false;
  }
  bool scalar (tree d)
  {
    /* A current SSA name is a value, including anonymous gimplifier values.
       Its optional debug declaration is not a storage allocation. */
    if (TREE_CODE (d) == SSA_NAME) return is_gimple_reg_type (TREE_TYPE (d));
    return d && local (d) && is_gimple_reg (d);
  }
  bool signature (gimple *s, tree fn)
  {
    unsigned i = 0; tree formal = TYPE_ARG_TYPES (TREE_TYPE (fn));
    for (; formal && TREE_VALUE (formal) != void_type_node; formal = TREE_CHAIN (formal), ++i)
      {
        if (i >= gimple_call_num_args (s)) return false;
        tree expected = TREE_VALUE (formal), actual = TREE_TYPE (gimple_call_arg (s, i));
        if (!types_compatible_p (expected, actual)
            && !(POINTER_TYPE_P (expected) && POINTER_TYPE_P (actual)
                 && TYPE_MAIN_VARIANT (TREE_TYPE (expected)) == void_type_node)) return false;
      }
    tree lhs = gimple_call_lhs (s), result = TREE_TYPE (TREE_TYPE (fn));
    return formal && i == gimple_call_num_args (s)
      && (result == void_type_node ? !lhs : lhs && types_compatible_p (TREE_TYPE (lhs), result));
  }
  bool call (gimple *s)
  {
    if (gimple_call_internal_p (s))
      {
        internal_fn code = gimple_call_internal_fn (s); tree lhs = gimple_call_lhs (s);
        bool valid = (code == IFN_ADD_OVERFLOW || code == IFN_SUB_OVERFLOW || code == IFN_MUL_OVERFLOW)
          && gimple_call_num_args (s) == 2 && lhs && TREE_CODE (TREE_TYPE (lhs)) == COMPLEX_TYPE
          && INTEGRAL_TYPE_P (TREE_TYPE (TREE_TYPE (lhs)));
        for (unsigned i = 0; valid && i < 2; ++i) valid &= INTEGRAL_TYPE_P (TREE_TYPE (gimple_call_arg (s, i)));
        return valid || refuse (s, "unclassified or ill-typed internal call");
      }
    tree fn = gimple_call_fndecl (s);
    if (!fn) return refuse (s, "indirect call remains in the transaction");
    if (fndecl_built_in_p (fn, BUILT_IN_DWARF_CFA))
      return cfa (s) || refuse (s, "frame helper lacks its current native frame producer");
    if (!signature (s, fn)) return refuse (s, "call has an incompatible current signature");
    bool admitted = newtm_clone_original (fn) != NULL_TREE;
    if (fndecl_built_in_p (fn, BUILT_IN_NORMAL))
      switch (DECL_FUNCTION_CODE (fn))
        {
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_READ:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END: admitted = !helper; break;
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_END:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE:
        case BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL: admitted = helper != NULL_TREE; break;
        default: admitted = false; break;
        }
    if (helper && (fn == helper || owner_builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS)
                  || owner_builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL))) admitted = true;
    if (!admitted) return refuse (s, "unprepared or unclassified call remains in the transaction");
    for (tree p = TREE_VEC_ELT (proof, COVER_CALLS); p; p = TREE_CHAIN (p))
      {
        tree q = TREE_VALUE (p);
        if (!calls.contains (p) && TREE_VEC_ELT (q, 1) == fn
            && tree_to_uhwi (TREE_VEC_ELT (q, 2)) == gimple_call_num_args (s)
            && same (TREE_VEC_ELT (q, 0), symbol (gimple_call_lhs (s))))
          { calls.add (p); return true; }
      }
    return refuse (s, "call has no unmatched source operation identity");
  }
  bool interval (tree ref, gimple *s)
  {
    HOST_WIDE_INT at, size; bool reverse;
    tree base = get_ref_base_and_extent_hwi (ref, &at, &size, &reverse);
    if (!base || reverse || at < 0 || size <= 0) return false;
    if (TREE_CODE (base) == MEM_REF)
      {
        tree delta = TREE_OPERAND (base, 1);
        tree object = channel_address (r, TREE_OPERAND (base, 0), s);
        if (!object || !tree_fits_shwi_p (delta) || tree_to_shwi (delta) < 0
            || tree_to_shwi (delta) > HOST_WIDE_INT_MAX / BITS_PER_UNIT) return false;
        HOST_WIDE_INT offset, extent; bool reversed;
        tree allocation = get_ref_base_and_extent_hwi (object, &offset, &extent, &reversed);
        HOST_WIDE_INT extra = tree_to_shwi (delta) * BITS_PER_UNIT;
        if (!allocation || reversed || offset < 0 || extent < 0 || extra > extent
            || at > extent - extra || size > extent - extra - at
            || offset > HOST_WIDE_INT_MAX - extra - at) return false;
        base = allocation; at += extra + offset;
      }
    tree total = DECL_P (base) ? DECL_SIZE (base) : NULL_TREE;
    return local (base) && (member (base) || scalar (base) || frame_record (base)) && total && tree_fits_shwi_p (total)
      && at <= tree_to_shwi (total) && size <= tree_to_shwi (total) - at;
  }
  bool mapped (tree ref, gimple *s, bool write)
  {
    if (TREE_CODE (ref) != COMPONENT_REF) return false;
    tree f = TREE_OPERAND (ref, 1), source = newtm_clone_memory_field (f);
    if (!source) return false;
    if (!r.frame (TREE_OPERAND (ref, 0), f, TREE_VALUE (source), s)) return false;
    if (member (TREE_VALUE (source))) return true;
    /* Outlining receiver fields hold addresses, not the captured objects.
       Their actual producers are independently reconstructed by block 1. */
    if (helper && !write && integer_onep (TREE_PURPOSE (source)))
      for (tree p = newtm_clone_captures (helper); p; p = TREE_CHAIN (p))
        if (newtm_clone_memory_origin (TREE_VALUE (p)) == newtm_clone_memory_origin (TREE_VALUE (source))) return true;
    return false;
  }
  bool reference_copy (tree ref, gimple *s, bool write)
  {
    if (write || !helper || !is_gimple_assign (s) || !gimple_assign_single_p (s)
        || ref != gimple_assign_rhs1 (s)) return false;
    tree source = DECL_P (ref) ? newtm_clone_memory_origin (ref) : NULL_TREE;
    if (TREE_CODE (ref) == COMPONENT_REF)
      if (tree p = newtm_clone_memory_field (TREE_OPERAND (ref, 1)))
        if (integer_zerop (TREE_PURPOSE (p)) && r.frame (TREE_OPERAND (ref, 0), TREE_OPERAND (ref, 1), TREE_VALUE (p), s))
          source = newtm_clone_memory_origin (TREE_VALUE (p));
    for (tree p = TREE_VEC_ELT (proof, COVER_REFERENCES); p; p = TREE_CHAIN (p))
      if (source && source == TREE_VALUE (p) && same (symbol (gimple_assign_lhs (s)), TREE_PURPOSE (p))) return true;
    return false;
  }
  bool memory (tree ref, gimple *s, bool write)
  {
    ++effects;
    hash_map<gimple *, tree> *map = write ? writes : reads;
    if (map)
      if (tree *p = map->get (s))
        if (operand_equal_p (*p, ref, 0)) return true;
    if (interval (ref, s) || mapped (ref, s, write) || reference_copy (ref, s, write)) return true;
    tree base = get_base_address (ref);
    if (!helper && base && TREE_CODE (base) == MEM_REF)
      {
        HOST_WIDE_INT offset = 0;
        if (channel_pointer (r, TREE_OPERAND (base, 0), s, offset))
          return channel_access (r, ref, s, write);
      }
    return refuse (s, write ? "uncovered direct write" : "uncovered direct read");
  }
  bool address (tree t, gimple *s, unsigned depth)
  {
    if (!t || depth > 64) return refuse (s, "address exceeds coverage proof depth");
    if (DECL_P (t) || CONSTANT_CLASS_P (t)) return true;
    if (!REFERENCE_CLASS_P (t)) return refuse (s, "unclassified address expression");
    for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
      {
        tree p = TREE_OPERAND (t, i); if (!p || TYPE_P (p) || TREE_CODE (p) == FIELD_DECL) continue;
        if (i == 0 && TREE_CODE (t) != MEM_REF && TREE_CODE (t) != INDIRECT_REF)
          { if (!address (p, s, depth + 1)) return false; }
        else if (!operand (p, s, false, depth + 1)) return false;
      }
    return true;
  }
  bool operand (tree t, gimple *s, bool write, unsigned depth = 0)
  {
    if (!t) return true;
    if (depth > 64) return refuse (s, "operand exceeds coverage proof depth");
    if (TYPE_P (t) || CONSTANT_CLASS_P (t) || TREE_CODE (t) == FIELD_DECL
        || TREE_CODE (t) == LABEL_DECL || TREE_CODE (t) == FUNCTION_DECL) return true;
    if (TREE_CODE (t) == ADDR_EXPR) return address (TREE_OPERAND (t, 0), s, depth + 1);
    if (variable (t))
      return (TREE_THIS_VOLATILE (t) || TYPE_VOLATILE (TREE_TYPE (t))) ? refuse (s, "volatile scalar effect remains")
        : scalar (t) || memory (t, s, write);
    if (TREE_CODE (t) == CONSTRUCTOR)
      {
        unsigned HOST_WIDE_INT i; tree v;
        FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (t), i, v)
          if (!operand (v, s, false, depth + 1)) return false;
        return true;
      }
    if (REFERENCE_CLASS_P (t))
      {
        if (TREE_THIS_VOLATILE (t) || TYPE_VOLATILE (TREE_TYPE (t))) return refuse (s, "volatile memory effect remains");
        return memory (t, s, write) && address (t, s, depth + 1);
      }
    if (!EXPR_P (t)) return refuse (s, "unclassified operand remains");
    tree_code code = TREE_CODE (t);
    if (TREE_SIDE_EFFECTS (t) || TREE_CODE_CLASS (code) == tcc_statement
        || TREE_CODE_CLASS (code) == tcc_vl_exp || code == MODIFY_EXPR || code == INIT_EXPR
        || code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR
        || code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR
        || code == TARGET_EXPR || code == BIND_EXPR || code == VA_ARG_EXPR)
      return refuse (s, "unclassified side effect in an operand");
    for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
      if (!operand (TREE_OPERAND (t, i), s, false, depth + 1)) return false;
    return true;
  }
  bool verify ()
  {
    if (!r.frames ()) return refuse (NULL, r.failure);
    hash_set<gimple *> seen;
    for (unsigned i = 0; i < r.flow.node_count (); ++i)
      if (gimple *s = r.flow.node (i).statement)
        {
          if (seen.add (s)) return refuse (s, "instruction instance occurs twice");
          /* SSA marks a clobber constructor volatile to keep it alive.
             Its destination still undergoes the ordinary volatile/storage
             checks below; this flag alone is not a volatile memory access. */
          if (gimple_has_volatile_ops (s) && !gimple_clobber_p (s))
            return refuse (s, "volatile effect remains");
          if (is_gimple_debug (s) || gimple_code (s) == GIMPLE_LABEL
              || gimple_code (s) == GIMPLE_NOP || gimple_code (s) == GIMPLE_PREDICT) continue;
          if (is_gimple_assign (s))
            {
              if (!operand (gimple_assign_lhs (s), s, true)) return false;
              if (!gimple_clobber_p (s))
                for (unsigned j = 1; j < gimple_num_ops (s); ++j)
                  if (!operand (gimple_op (s, j), s, false)) return false;
            }
          else if (is_gimple_call (s))
            {
              ++effects; if (!call (s) || !operand (gimple_call_lhs (s), s, true)) return false;
              for (unsigned j = 0; j < gimple_call_num_args (s); ++j)
                if (!operand (gimple_call_arg (s, j), s, false)) return false;
              if (!operand (gimple_call_chain (s), s, false)) return false;
            }
          else switch (gimple_code (s))
            {
            case GIMPLE_RETURN:
              if (!operand (gimple_return_retval (as_a<greturn *> (s)), s, false)) return false;
              break;
            case GIMPLE_COND:
              if (!operand (gimple_cond_lhs (s), s, false) || !operand (gimple_cond_rhs (s), s, false)) return false;
              break;
            case GIMPLE_SWITCH:
              if (!operand (gimple_switch_index (as_a<gswitch *> (s)), s, false)) return false;
              break;
            case GIMPLE_GOTO:
              if (TREE_CODE (gimple_goto_dest (s)) != LABEL_DECL) return refuse (s, "computed goto remains");
              break;
            default: return refuse (s, "unclassified effect or structured operation remains");
            }
        }
    for (tree p = TREE_VEC_ELT (proof, COVER_CALLS); p; p = TREE_CHAIN (p))
      if (!calls.contains (p)) return refuse (NULL, "source call lost its current classified operation");
    return !r.failure || refuse (NULL, r.failure);
  }
};
static void coverage_result (tree key, newtm_memory_state state, unsigned effects = 0, bool entry = false)
{
  hash_map<tree, tree> *&results = entry ? coverage_entry_results : coverage_results;
  if (!results) results = hash_map<tree, tree>::create_ggc ();
  results->put (key, list (number (state), number (effects),
    number (state == NEWTM_MEMORY_DEPENDENT ? NEWTM_MEMORY_IDENTITY | NEWTM_MEMORY_CHANNELS
            | NEWTM_MEMORY_STATUS | NEWTM_MEMORY_OWNER : 0)));
}
}

bool newtm_coverage_verify_cfg (tree fn)
{
  newtm_current_component_begin (fn);
  coverage_result (fn, NEWTM_MEMORY_REFUSED);
  tree *p = coverage_contracts ? coverage_contracts->get (fn) : NULL;
  if (!p) return coverage_fail (fn, NULL, "missing live-window coverage contract");
  hash_map<gimple *, tree> reads, writes;
  if (!memory_verify_cfg (fn, &reads, &writes)) return coverage_fail (fn, NULL, "private effects lack a current identity proof");
  newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
  if (!flow.build_cfg (DECL_STRUCT_FUNCTION (fn), &why, &bad)) return coverage_fail (fn, bad, why);
  resolver r (fn, flow, true);
  coverage check (r, *p, NULL_TREE, &reads, &writes);
  if (!check.verify ()) return false;
  coverage_result (fn, NEWTM_MEMORY_DEPENDENT, check.effects); return true;
}

bool newtm_coverage_verify_entry (tree fn, gimple *call)
{
  newtm_current_component_begin (fn);
  tree helper = call && is_gimple_call (call) ? gimple_call_fndecl (call) : NULL_TREE;
  if (!helper) return coverage_fail (fn, call, "missing current controller invocation");
  coverage_result (helper, NEWTM_MEMORY_REFUSED, 0, true);
  tree *p = coverage_entries ? coverage_entries->get (helper) : NULL;
  tree *owner = owner_entries ? owner_entries->get (helper) : NULL;
  if (!p || !owner) return coverage_fail (fn, call, "missing live-window controller coverage contract");
  newtm_flow_graph whole, flow;
  if (!owner_region (fn, call, *owner, whole, flow)) return coverage_fail (fn, call, "current controller boundary is not proven");
  resolver r (fn, flow, true); r.proof_owner = TREE_VEC_ELT (*owner, OWNER_FN);
  r.entry_frame = newtm_clone_entry_frame (fn, call);
  coverage check (r, *p, helper, NULL, NULL);
  if (!check.verify ()) return false;
  coverage_result (helper, NEWTM_MEMORY_DEPENDENT, check.effects, true); return true;
}

bool newtm_coverage_get (tree key, newtm_memory_view *out)
{
  tree *p = coverage_results ? coverage_results->get (key) : NULL;
  if (!p || !out) return false;
  out->state = (newtm_memory_state) tree_to_uhwi (TREE_VEC_ELT (*p, 0));
  out->operations = tree_to_uhwi (TREE_VEC_ELT (*p, 1));
  out->dependencies = tree_to_uhwi (TREE_VEC_ELT (*p, 2)); return true;
}
bool newtm_coverage_entry_get (tree key, newtm_memory_view *out)
{
  tree *p = coverage_entry_results ? coverage_entry_results->get (key) : NULL;
  if (!p || !out) return false;
  out->state = (newtm_memory_state) tree_to_uhwi (TREE_VEC_ELT (*p, 0));
  out->operations = tree_to_uhwi (TREE_VEC_ELT (*p, 1));
  out->dependencies = tree_to_uhwi (TREE_VEC_ELT (*p, 2)); return true;
}

/* Persistent contracts only: reconstructed proofs are never serialized. */
tree newtm_memory_export ()
{
  tree data = make_tree_vec (11);
  TREE_VEC_ELT (data, 0) = newtm_lto_pack (memory_contracts);
  TREE_VEC_ELT (data, 1) = newtm_lto_pack (memory_arithmetic);
  TREE_VEC_ELT (data, 2) = newtm_lto_pack (channel_contracts);
  TREE_VEC_ELT (data, 3) = newtm_lto_pack (channel_publications);
  TREE_VEC_ELT (data, 4) = newtm_lto_pack (channel_entries);
  TREE_VEC_ELT (data, 5) = newtm_lto_pack (status_contracts);
  TREE_VEC_ELT (data, 6) = newtm_lto_pack (status_publications);
  TREE_VEC_ELT (data, 7) = newtm_lto_pack (status_guards);
  TREE_VEC_ELT (data, 8) = newtm_lto_pack (owner_entries);
  TREE_VEC_ELT (data, 9) = newtm_lto_pack (coverage_contracts);
  TREE_VEC_ELT (data, 10) = newtm_lto_pack (coverage_entries);
  return data;
}
void newtm_memory_import (tree data)
{
  gcc_assert (data && TREE_CODE (data) == TREE_VEC && TREE_VEC_LENGTH (data) == 11);
  newtm_lto_unpack (memory_contracts, TREE_VEC_ELT (data, 0));
  newtm_lto_unpack (memory_arithmetic, TREE_VEC_ELT (data, 1));
  newtm_lto_unpack (channel_contracts, TREE_VEC_ELT (data, 2));
  newtm_lto_unpack (channel_publications, TREE_VEC_ELT (data, 3));
  newtm_lto_unpack (channel_entries, TREE_VEC_ELT (data, 4));
  newtm_lto_unpack (status_contracts, TREE_VEC_ELT (data, 5));
  newtm_lto_unpack (status_publications, TREE_VEC_ELT (data, 6));
  newtm_lto_unpack (status_guards, TREE_VEC_ELT (data, 7));
  newtm_lto_unpack (owner_entries, TREE_VEC_ELT (data, 8));
  newtm_lto_unpack (coverage_contracts, TREE_VEC_ELT (data, 9));
  newtm_lto_unpack (coverage_entries, TREE_VEC_ELT (data, 10));
}

#include "gt-newtm-memory.h"
