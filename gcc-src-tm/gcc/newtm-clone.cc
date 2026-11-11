/* Prepared functions from remapped GENERIC, before High-GIMPLE retirement.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "cgraph.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "gimplify.h"
#include "tree-inline.h"
#include "tree-iterator.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "stringpool.h"
#include "diagnostic-core.h"
#include "omp-general.h"
#include "newtm-abi.h"
#include "newtm-clone.h"
#include "newtm-lto.h"
#include "newtm-descriptor.h"
#include "newtm-occurrence.h"
#include "newtm-access.h"
#include "newtm-obligation.h"
#include "bitmap.h"
#include "newtm-flow.h"
#include "newtm-guard.h"
#include "newtm-memory.h"
#include "newtm-current.h"
#include "internal-fn.h"

/* Info slots: original, context, bindings, result-out, binding-out, publication
   marker, failure status and failure label. Prototypes exist for the complete
   selected graph before body copies.
   These maps are compiler roots only; no lookup is emitted into a program. */
static GTY(()) hash_map<tree, tree> *newtm_clones;
static GTY(()) hash_map<tree, tree> *newtm_clone_info;
static GTY(()) hash_map<gimple *, tree> *newtm_clone_results;
static GTY(()) hash_map<tree, tree> *newtm_capture_lists;
static GTY(()) hash_map<tree, tree> *newtm_lexical_decls;
static GTY(()) hash_map<tree, tree> *newtm_region_owners;
static GTY(()) hash_map<tree, tree> *newtm_region_sources;
static GTY(()) hash_map<tree, tree> *newtm_region_parameters;
static GTY(()) hash_map<gimple *, tree> *newtm_region_bodies;
static GTY(()) hash_map<tree, tree> *newtm_region_rejections;
static GTY(()) hash_map<tree, tree> *newtm_call_dependencies;
static GTY(()) tree newtm_activated_regions;
/* Declaration provenance only: never retains a statement or descriptor. */
static GTY(()) hash_map<tree, tree> *newtm_outline_parents;
static GTY(()) hash_map<tree, tree> *newtm_prepared_prototypes;
static GTY(()) hash_map<tree, tree> *newtm_prepared_parameter_origins;

static GTY(()) hash_map<tree, tree> *newtm_remap_decls;
static GTY(()) hash_map<tree, tree> *newtm_remap_fields;
static GTY(()) hash_map<tree, tree> *newtm_native_frames;
static GTY(()) hash_map<tree, tree> *newtm_capture_origins;
/* Per-call proof summaries: status declaration, origin, static extent/kind. */
static GTY(()) hash_map<tree, tree> *newtm_registration_origins;
static GTY(()) hash_map<tree, tree> *newtm_memory_origins;
static GTY(()) hash_map<tree, tree> *newtm_offset_arithmetic;
static tree remap_origin (tree);
void newtm_clone_record_decl (tree replacement, tree source)
{
  newtm_current_invalidate (NULL_TREE);
  if (!flag_openmp_transaction_stm || !replacement || !source || replacement == source
      || !DECL_P (replacement) || !DECL_P (source)) return;
  if (!newtm_remap_decls) newtm_remap_decls = hash_map<tree, tree>::create_ggc ();
  if (tree *old = newtm_remap_decls->get (replacement))
    { if (remap_origin (*old) != remap_origin (source)) *old = error_mark_node; }
  else newtm_remap_decls->put (replacement, source);
}
void newtm_clone_record_field (tree field, tree source, bool indirect)
{
  newtm_current_invalidate (NULL_TREE);
  if (!flag_openmp_transaction_stm || !field || !source) return;
  if (!newtm_remap_fields) newtm_remap_fields = hash_map<tree, tree>::create_ggc ();
  newtm_remap_fields->put (field, tree_cons (indirect ? integer_one_node : integer_zero_node, source, NULL_TREE));
}
static tree remap_origin (tree value)
{
  hash_set<tree> seen;
  while (value && value != error_mark_node && !seen.add (value))
    {
      tree *p = newtm_remap_decls ? newtm_remap_decls->get (value) : NULL;
      if (!p) return value;
      value = *p;
    }
  return NULL_TREE;
}
tree newtm_clone_memory_origin (tree value) { return remap_origin (value); }
void newtm_clone_record_frame (tree frame, tree owner, tree debug, tree chain)
{
  newtm_current_invalidate (NULL_TREE);
  if (!flag_openmp_transaction_stm) return;
  if (!newtm_native_frames) newtm_native_frames = hash_map<tree, tree>::create_ggc ();
  tree record = make_tree_vec (3);
  TREE_VEC_ELT (record, 0) = owner; TREE_VEC_ELT (record, 1) = debug;
  TREE_VEC_ELT (record, 2) = chain;
  newtm_native_frames->put (remap_origin (frame), record);
}
tree newtm_clone_native_frame (tree frame)
{
  tree *p = newtm_native_frames ? newtm_native_frames->get (remap_origin (frame)) : NULL;
  return p ? *p : NULL_TREE;
}
tree newtm_clone_memory_field (tree field)
{
  tree *p = newtm_remap_fields ? newtm_remap_fields->get (field) : NULL;
  return p ? *p : NULL_TREE;
}
/* A proof token is (address-or-value, declaration). Never an access recipe. */
static tree capture_token (tree object, bool address)
{
  object = remap_origin (object);
  return object ? tree_cons (address ? integer_one_node : integer_zero_node, object, NULL_TREE) : NULL_TREE;
}
static tree capture_value (tree value, hash_map<tree, gimple *> &definitions, unsigned depth = 0);
static tree capture_address (tree object, hash_map<tree, gimple *> &definitions, unsigned depth)
{
  if (!object || depth > 64) return NULL_TREE;
  if (DECL_P (object)) return capture_token (object, true);
  if (TREE_CODE (object) == COMPONENT_REF)
    {
      tree *field = newtm_remap_fields ? newtm_remap_fields->get (TREE_OPERAND (object, 1)) : NULL;
      if (field && integer_zerop (TREE_PURPOSE (*field))) return capture_token (TREE_VALUE (*field), true);
    }
  if (TREE_CODE (object) == MEM_REF && integer_zerop (TREE_OPERAND (object, 1)))
    return capture_value (TREE_OPERAND (object, 0), definitions, depth + 1);
  return NULL_TREE;
}
static tree capture_value (tree value, hash_map<tree, gimple *> &definitions, unsigned depth)
{
  if (!value || depth > 64) return NULL_TREE;
  if (TREE_CODE (value) == ADDR_EXPR) return capture_address (TREE_OPERAND (value, 0), definitions, depth + 1);
  if (TREE_CODE (value) == COMPONENT_REF)
    {
      tree *field = newtm_remap_fields ? newtm_remap_fields->get (TREE_OPERAND (value, 1)) : NULL;
      return field ? capture_token (TREE_VALUE (*field), !integer_zerop (TREE_PURPOSE (*field))) : NULL_TREE;
    }
  if (TREE_CODE (value) == MEM_REF && integer_zerop (TREE_OPERAND (value, 1)))
    {
      tree token = capture_value (TREE_OPERAND (value, 0), definitions, depth + 1);
      return token && integer_onep (TREE_PURPOSE (token)) ? capture_token (TREE_VALUE (token), false) : NULL_TREE;
    }
  if (DECL_P (value) || TREE_CODE (value) == SSA_NAME)
    {
      if (gimple **def = definitions.get (value))
        {
          if (!*def) return NULL_TREE;
          gimple *stmt = *def;
          enum tree_code code = gimple_assign_rhs_code (stmt);
          if (CONVERT_EXPR_CODE_P (code))
            {
              tree from = TREE_TYPE (gimple_assign_rhs1 (stmt)), to = TREE_TYPE (gimple_assign_lhs (stmt));
              if ((!POINTER_TYPE_P (from) && !INTEGRAL_TYPE_P (from))
                  || (!POINTER_TYPE_P (to) && !INTEGRAL_TYPE_P (to))
                  || TYPE_PRECISION (from) < TYPE_PRECISION (ptr_type_node)
                  || TYPE_PRECISION (to) < TYPE_PRECISION (ptr_type_node)) return NULL_TREE;
            }
          if (gimple_assign_single_p (stmt) || CONVERT_EXPR_CODE_P (code))
            return capture_value (gimple_assign_rhs1 (stmt), definitions, depth + 1);
          if ((code == POINTER_PLUS_EXPR || code == PLUS_EXPR) && integer_zerop (gimple_assign_rhs2 (stmt)))
            return capture_value (gimple_assign_rhs1 (stmt), definitions, depth + 1);
          return NULL_TREE;
        }
      return DECL_P (value) ? capture_token (value, false) : NULL_TREE;
    }
  return NULL_TREE;
}
static void capture_definition (gimple *stmt, hash_map<tree, gimple *> &definitions)
{
  if (is_gimple_call (stmt))
    {
      tree lhs = gimple_call_lhs (stmt);
      if (lhs && (DECL_P (lhs) || TREE_CODE (lhs) == SSA_NAME)) definitions.put (lhs, NULL);
      return;
    }
  if (!is_gimple_assign (stmt)) return;
  tree lhs = gimple_assign_lhs (stmt);
  if (!DECL_P (lhs) && TREE_CODE (lhs) != SSA_NAME) return;
  if (definitions.get (lhs)) definitions.put (lhs, NULL);
  else definitions.put (lhs, stmt);
}
static tree registration_constant (tree value, hash_map<tree, gimple *> &definitions, unsigned depth = 0)
{
  if (!value || depth > 64) return NULL_TREE;
  if (TREE_CODE (value) == INTEGER_CST) return value;
  gimple **def = definitions.get (value);
  if (!def || !*def || !is_gimple_assign (*def)) return NULL_TREE;
  if (gimple_assign_single_p (*def) || gimple_assign_cast_p (*def))
    {
      tree result = registration_constant (gimple_assign_rhs1 (*def), definitions, depth + 1);
      return result ? fold_convert (TREE_TYPE (value), result) : NULL_TREE;
    }
  return NULL_TREE;
}
static tree offset_arithmetic_record (tree records, tree result)
{
  result = remap_origin (result);
  for (tree p = records; p; p = TREE_CHAIN (p))
    if (TREE_VEC_ELT (TREE_VALUE (p), 0) == result) return TREE_VALUE (p);
  return NULL_TREE;
}
static bool offset_requirements (tree value, hash_map<tree, gimple *> &definitions,
                                 tree arithmetic, tree &required, hash_set<gimple *> &seen, unsigned depth = 0)
{
  if (!value) return true;
  if (depth > 64) return false;
  if (DECL_P (value) || TREE_CODE (value) == SSA_NAME)
    {
      gimple **def = definitions.get (value);
      if (!def || !*def || !is_gimple_assign (*def) || seen.add (*def)) return true;
      if (tree record = offset_arithmetic_record (arithmetic, value))
        required = tree_cons (NULL_TREE, TREE_VEC_ELT (record, 0), required);
      for (unsigned i = 1; i < gimple_num_ops (*def); ++i)
        if (!offset_requirements (gimple_op (*def, i), definitions, arithmetic, required, seen, depth + 1)) return false;
    }
  else if (EXPR_P (value) && TREE_CODE (value) != ADDR_EXPR
           && (!REFERENCE_CLASS_P (value) || TREE_CODE (value) == REALPART_EXPR || TREE_CODE (value) == IMAGPART_EXPR))
    for (int i = 0; i < TREE_OPERAND_LENGTH (value); ++i)
      if (!offset_requirements (TREE_OPERAND (value, i), definitions, arithmetic, required, seen, depth + 1)) return false;
  return true;
}
static void record_memory_origins (tree fn, gimple_seq sequence)
{
  hash_map<tree, gimple *> definitions;
  auto_vec<gimple *> calls;
  hash_map<tree, gimple *> checked_calls;
  tree arithmetic = NULL_TREE;
  struct scan {
    hash_map<tree, gimple *> *definitions;
    auto_vec<gimple *> *calls;
    hash_map<tree, gimple *> *checked_calls;
    tree *arithmetic;
    static tree stmt (gimple_stmt_iterator *it, bool *, walk_stmt_info *wi)
      {
        scan *self = (scan *) wi->info;
        gimple *s = gsi_stmt (*it); capture_definition (s, *self->definitions);
        if (is_gimple_call (s) && gimple_call_internal_p (s) && gimple_call_lhs (s))
          {
            internal_fn op = gimple_call_internal_fn (s);
            if (op == IFN_ADD_OVERFLOW || op == IFN_SUB_OVERFLOW || op == IFN_MUL_OVERFLOW)
              self->checked_calls->put (gimple_call_lhs (s), s);
          }
        if (is_gimple_assign (s) && gimple_assign_rhs_code (s) == REALPART_EXPR && newtm_obligation_math_p (s))
          {
            tree pair = TREE_OPERAND (gimple_assign_rhs1 (s), 0);
            gimple **producer = self->checked_calls->get (pair);
            if (producer && *producer)
              {
                tree record = make_tree_vec (5), type = TREE_TYPE (TREE_TYPE (pair));
                TREE_VEC_ELT (record, 0) = remap_origin (gimple_assign_lhs (s));
                TREE_VEC_ELT (record, 1) = remap_origin (pair);
                TREE_VEC_ELT (record, 2) = build_int_cst (unsigned_type_node, gimple_call_internal_fn (*producer));
                TREE_VEC_ELT (record, 3) = build_int_cst (unsigned_type_node, TYPE_PRECISION (type));
                TREE_VEC_ELT (record, 4) = TYPE_UNSIGNED (type) ? integer_one_node : integer_zero_node;
                *self->arithmetic = tree_cons (NULL_TREE, record, *self->arithmetic);
              }
          }
        if (is_gimple_call (s) && gimple_call_fndecl (s)
            && fndecl_built_in_p (gimple_call_fndecl (s), BUILT_IN_NORMAL))
          {
            built_in_function op = DECL_FUNCTION_CODE (gimple_call_fndecl (s));
            if (op == BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER
                || op == BUILT_IN_GOMP_TRANSACTION_STM_V1_READ
                || op == BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE) self->calls->safe_push (s);
          }
        return NULL_TREE;
      }
  } data = { &definitions, &calls, &checked_calls, &arithmetic };
  walk_stmt_info wi = {}; wi.info = &data; walk_gimple_seq (sequence, scan::stmt, NULL, &wi);
  tree records = NULL_TREE, accesses = NULL_TREE;
  for (gimple *call : calls)
    {
      built_in_function op = DECL_FUNCTION_CODE (gimple_call_fndecl (call));
      if (op != BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER)
        {
          tree proof = make_tree_vec (8);
          TREE_VEC_ELT (proof, 0) = remap_origin (gimple_call_lhs (call));
          TREE_VEC_ELT (proof, 5) = build_int_cst (unsigned_type_node, op);
          tree required = NULL_TREE; hash_set<gimple *> seen;
          if (!offset_requirements (gimple_call_arg (call, 2), definitions, arithmetic, required, seen)) required = error_mark_node;
          TREE_VEC_ELT (proof, 7) = required;
          tree object = capture_value (gimple_call_arg (call, 1), definitions);
          tree size = registration_constant (gimple_call_arg (call, 4), definitions);
          if (object && integer_onep (TREE_PURPOSE (object)) && size)
            {
              tree type = TREE_TYPE (TREE_VALUE (object)), extent = TYPE_SIZE_UNIT (type);
              if (extent && TREE_CODE (extent) == INTEGER_CST && !integer_zerop (extent))
                {
                  TREE_VEC_ELT (proof, 1) = object;
                  TREE_VEC_ELT (proof, 2) = fold_convert (size_type_node, extent);
                  TREE_VEC_ELT (proof, 3) = size;
                  TREE_VEC_ELT (proof, 4) = build_int_cst (uint32_type_node, TREE_CODE (type) == ARRAY_TYPE || TREE_CODE (type) == RECORD_TYPE);
                  TREE_VEC_ELT (proof, 6) = registration_constant (gimple_call_arg (call, 2), definitions);
                }
            }
          accesses = tree_cons (NULL_TREE, proof, accesses);
          continue;
        }
      tree proof = make_tree_vec (4);
      TREE_VEC_ELT (proof, 0) = remap_origin (gimple_call_lhs (call));
      tree object = capture_value (gimple_call_arg (call, 1), definitions);
      tree extent = registration_constant (gimple_call_arg (call, 2), definitions);
      tree kind = registration_constant (gimple_call_arg (call, 3), definitions);
      if (object && integer_onep (TREE_PURPOSE (object)) && extent && kind)
        { TREE_VEC_ELT (proof, 1) = object; TREE_VEC_ELT (proof, 2) = extent; TREE_VEC_ELT (proof, 3) = kind; }
      records = tree_cons (NULL_TREE, proof, records);
    }
  if (!newtm_registration_origins) newtm_registration_origins = hash_map<tree, tree>::create_ggc ();
  newtm_registration_origins->put (fn, records);
  if (!newtm_memory_origins) newtm_memory_origins = hash_map<tree, tree>::create_ggc ();
  newtm_memory_origins->put (fn, accesses);
  if (!newtm_offset_arithmetic) newtm_offset_arithmetic = hash_map<tree, tree>::create_ggc ();
  newtm_offset_arithmetic->put (fn, arithmetic);
}
static tree current_registration_constant (tree value, gimple *use, newtm_guard_analysis &guards, unsigned depth = 0)
{
  if (!value || depth > 64) return NULL_TREE;
  if (TREE_CODE (value) == INTEGER_CST) return value;
  gimple *def = guards.definition_at (use, value);
  if (!def || !is_gimple_assign (def)) return NULL_TREE;
  if (gimple_assign_single_p (def) || gimple_assign_cast_p (def))
    {
      tree result = current_registration_constant (gimple_assign_rhs1 (def), def, guards, depth + 1);
      return result ? fold_convert (TREE_TYPE (value), result) : NULL_TREE;
    }
  return NULL_TREE;
}
static tree current_registration_object (tree, gimple *, newtm_guard_analysis &, unsigned = 0);
static tree current_registration_address (tree object, gimple *use, newtm_guard_analysis &guards, unsigned depth)
{
  if (!object || depth > 64) return NULL_TREE;
  if (DECL_P (object)) return capture_token (object, true);
  if (TREE_CODE (object) == COMPONENT_REF)
    {
      tree *field = newtm_remap_fields ? newtm_remap_fields->get (TREE_OPERAND (object, 1)) : NULL;
      if (field && integer_zerop (TREE_PURPOSE (*field))) return capture_token (TREE_VALUE (*field), true);
    }
  if (TREE_CODE (object) == MEM_REF && integer_zerop (TREE_OPERAND (object, 1)))
    return current_registration_object (TREE_OPERAND (object, 0), use, guards, depth + 1);
  return NULL_TREE;
}
static tree current_registration_object (tree value, gimple *use, newtm_guard_analysis &guards, unsigned depth)
{
  if (!value || depth > 64) return NULL_TREE;
  if (TREE_CODE (value) == ADDR_EXPR)
    return current_registration_address (TREE_OPERAND (value, 0), use, guards, depth + 1);
  if (TREE_CODE (value) == COMPONENT_REF)
    {
      tree *field = newtm_remap_fields ? newtm_remap_fields->get (TREE_OPERAND (value, 1)) : NULL;
      return field ? capture_token (TREE_VALUE (*field), !integer_zerop (TREE_PURPOSE (*field))) : NULL_TREE;
    }
  if (TREE_CODE (value) == MEM_REF && integer_zerop (TREE_OPERAND (value, 1)))
    {
      tree token = current_registration_object (TREE_OPERAND (value, 0), use, guards, depth + 1);
      return token && integer_onep (TREE_PURPOSE (token)) ? capture_token (TREE_VALUE (token), false) : NULL_TREE;
    }
  if (!DECL_P (value) && TREE_CODE (value) != SSA_NAME) return NULL_TREE;
  gimple *def = guards.definition_at (use, value);
  if (!def || !is_gimple_assign (def)) return DECL_P (value) ? capture_token (value, false) : NULL_TREE;
  enum tree_code code = gimple_assign_rhs_code (def);
  if (CONVERT_EXPR_CODE_P (code))
    {
      tree from = TREE_TYPE (gimple_assign_rhs1 (def)), to = TREE_TYPE (value);
      if ((!POINTER_TYPE_P (from) && !INTEGRAL_TYPE_P (from))
          || (!POINTER_TYPE_P (to) && !INTEGRAL_TYPE_P (to))
          || TYPE_PRECISION (from) < TYPE_PRECISION (ptr_type_node)
          || TYPE_PRECISION (to) < TYPE_PRECISION (ptr_type_node)) return NULL_TREE;
    }
  if (gimple_assign_single_p (def) || CONVERT_EXPR_CODE_P (code)
      || ((code == POINTER_PLUS_EXPR || code == PLUS_EXPR) && integer_zerop (gimple_assign_rhs2 (def))))
    return current_registration_object (gimple_assign_rhs1 (def), def, guards, depth + 1);
  return NULL_TREE;
}
enum registration_origin_state { REGISTRATION_INVALID, REGISTRATION_STATIC_MATCH, REGISTRATION_TRANSPORTED };
static registration_origin_state registration_origin_matches (tree fn, gimple *call, newtm_guard_analysis &guards, hash_set<tree> &seen)
{
  tree *records = newtm_registration_origins ? newtm_registration_origins->get (fn) : NULL;
  tree status = remap_origin (gimple_call_lhs (call)), proof = NULL_TREE;
  if (!status || !records || seen.add (status)) return REGISTRATION_INVALID;
  for (tree p = *records; p; p = TREE_CHAIN (p))
    if (TREE_VEC_ELT (TREE_VALUE (p), 0) == status) { proof = TREE_VALUE (p); break; }
  if (!proof) return REGISTRATION_INVALID;
  /* Transported tuples are checked by newtm_memory_verify_cfg, below. */
  if (!TREE_VEC_ELT (proof, 1)) return REGISTRATION_TRANSPORTED;
  tree actual = current_registration_object (gimple_call_arg (call, 1), call, guards);
  tree expected = TREE_VEC_ELT (proof, 1);
  tree extent = current_registration_constant (gimple_call_arg (call, 2), call, guards);
  tree kind = current_registration_constant (gimple_call_arg (call, 3), call, guards);
  bool matched = actual && integer_onep (TREE_PURPOSE (actual))
    && TREE_VALUE (actual) == remap_origin (TREE_VALUE (expected))
    && extent && kind && tree_int_cst_equal (extent, TREE_VEC_ELT (proof, 2))
    && tree_int_cst_equal (kind, TREE_VEC_ELT (proof, 3));
  return matched ? REGISTRATION_STATIC_MATCH : REGISTRATION_INVALID;
}
static bool record_capture_origins (tree helper, gimple_seq sequence, auto_vec<tree> &args)
{
  hash_map<tree, gimple *> definitions;
  struct scan {
    static tree stmt (gimple_stmt_iterator *it, bool *, walk_stmt_info *wi)
      { capture_definition (gsi_stmt (*it), *(hash_map<tree, gimple *> *) wi->info); return NULL_TREE; }
  };
  walk_stmt_info wi = {}; wi.info = &definitions;
  walk_gimple_seq (sequence, scan::stmt, NULL, &wi);
  tree tokens = NULL_TREE, *tail = &tokens;
  for (unsigned i = 2; i < args.length (); ++i)
    {
      tree token = capture_value (args[i], definitions);
      if (!token) { error_at (DECL_SOURCE_LOCATION (helper), "transaction capture lacks a remapping origin"); return false; }
      *tail = tree_cons (NULL_TREE, token, NULL_TREE); tail = &TREE_CHAIN (*tail);
    }
  if (!newtm_capture_origins) newtm_capture_origins = hash_map<tree, tree>::create_ggc ();
  newtm_capture_origins->put (helper, tokens);
  return true;
}
/* A receiver field is evidence only when its producer stores the address of
   the declaration that the lowering associated with that field. */
static bool check_capture_producers (tree expected)
{
  hash_set<tree> origins;
  for (tree p = expected; p; p = TREE_CHAIN (p)) origins.add (remap_origin (TREE_VALUE (TREE_VALUE (p))));
  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    {
      function *body = DECL_STRUCT_FUNCTION (node->decl);
      if (!body || !body->cfg) continue;
      hash_map<tree, gimple *> definitions;
      basic_block bb;
      FOR_EACH_BB_FN (bb, body)
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it)) capture_definition (gsi_stmt (it), definitions);
      FOR_EACH_BB_FN (bb, body)
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          {
            gimple *stmt = gsi_stmt (it);
            if (!is_gimple_assign (stmt)) continue;
            tree lhs = gimple_assign_lhs (stmt);
            if (TREE_CODE (lhs) != COMPONENT_REF) continue;
            tree *field = newtm_remap_fields ? newtm_remap_fields->get (TREE_OPERAND (lhs, 1)) : NULL;
            if (!field || !integer_onep (TREE_PURPOSE (*field))) continue;
            tree source = remap_origin (TREE_VALUE (*field));
            if (!origins.contains (source)) continue;
            tree value = gimple_assign_single_p (stmt) ? capture_value (gimple_assign_rhs1 (stmt), definitions) : NULL_TREE;
            if (!value || !integer_onep (TREE_PURPOSE (value)) || TREE_VALUE (value) != source) return false;
          }
    }
  return true;
}
static bool check_capture_origins (tree helper, function *body, gimple *call)
{
  tree *saved = newtm_capture_origins ? newtm_capture_origins->get (helper) : NULL;
  if (!saved) return false;
  hash_map<tree, gimple *> definitions;
  basic_block bb;
  FOR_EACH_BB_FN (bb, body)
    for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it)) capture_definition (gsi_stmt (it), definitions);
  tree expected = *saved;
  for (unsigned i = 2; i < gimple_call_num_args (call); ++i)
    {
      tree actual = capture_value (gimple_call_arg (call, i), definitions);
      if (!expected || !actual || TREE_PURPOSE (actual) != TREE_PURPOSE (TREE_VALUE (expected))
          || TREE_VALUE (actual) != remap_origin (TREE_VALUE (TREE_VALUE (expected)))) return false;
      expected = TREE_CHAIN (expected);
    }
  return !expected && check_capture_producers (*saved);
}
void newtm_clone_record_outline (tree child, tree parent)
{
  newtm_current_invalidate (NULL_TREE);
  if (!newtm_outline_parents) newtm_outline_parents = hash_map<tree, tree>::create_ggc ();
  if (tree *old = newtm_outline_parents->get (child))
    { if (*old != parent) *old = error_mark_node; }
  else newtm_outline_parents->put (child, parent);
}
static bool lowered_owner_matches (tree fn, tree owner)
{
  hash_set<tree> seen;
  while (fn && fn != error_mark_node && !seen.add (fn))
    {
      if (fn == owner) return true;
      tree *p = newtm_outline_parents ? newtm_outline_parents->get (fn) : NULL;
      fn = p ? *p : NULL_TREE;
    }
  return false;
}
tree newtm_clone_entry_frame (tree fn, gimple *call)
{
  tree callee = gimple_call_fndecl (call);
  tree owner = callee ? newtm_clone_region_owner (callee) : NULL_TREE;
  tree parameter = DECL_ARGUMENTS (fn);
  if (!owner || fn == owner || !lowered_owner_matches (fn, owner)
      || !parameter || DECL_CHAIN (parameter) || DECL_CONTEXT (parameter) != fn
      || !POINTER_TYPE_P (TREE_TYPE (parameter))
      || TREE_CODE (TREE_TYPE (TREE_TYPE (parameter))) != RECORD_TYPE
      || !check_capture_origins (callee, DECL_STRUCT_FUNCTION (fn), call)) return NULL_TREE;
  return parameter;
}
static bool check_prepared_signature (tree fn)
{
  tree *saved = newtm_prepared_prototypes ? newtm_prepared_prototypes->get (fn) : NULL;
  tree type = TREE_TYPE (fn);
  bool valid = saved && TREE_CODE (type) == FUNCTION_TYPE
    && types_compatible_p (TREE_TYPE (type), uint32_type_node);
  tree arg = valid ? TYPE_ARG_TYPES (type) : NULL_TREE;
  tree expected = saved ? *saved : NULL_TREE;
  tree parameter = DECL_ARGUMENTS (fn);
  tree *saved_parameters = newtm_prepared_parameter_origins ? newtm_prepared_parameter_origins->get (fn) : NULL;
  tree parameter_origins = saved_parameters ? *saved_parameters : NULL_TREE;
  valid &= saved_parameters != NULL;
  for (; valid && expected; expected = TREE_CHAIN (expected))
    {
      tree wanted = TREE_VALUE (expected);
      if (!arg || !types_compatible_p (TREE_VALUE (arg), wanted)) { valid = false; break; }
      if (wanted == void_type_node)
        { valid = !TREE_CHAIN (expected) && !TREE_CHAIN (arg) && !parameter; arg = NULL_TREE; break; }
      if (!parameter || !parameter_origins || !remap_origin (parameter)
          || remap_origin (parameter) != remap_origin (TREE_VALUE (parameter_origins))
          || DECL_CONTEXT (parameter) != fn
          || !types_compatible_p (TREE_TYPE (parameter), wanted)) { valid = false; break; }
      arg = TREE_CHAIN (arg); parameter = DECL_CHAIN (parameter); parameter_origins = TREE_CHAIN (parameter_origins);
    }
  if (!valid || arg || parameter || parameter_origins)
    { error_at (newtm_current_diagnostic_location (fn, NULL), "transaction prepared signature lost its parameter correspondence");
      newtm_current_diagnostic_context (fn, NULL, "V06", "prepared signature"); return false; }
  return true;
}
static bool lowered_call_signature (gimple *stmt, tree callee)
{
  tree type = TREE_TYPE (callee);
  bool compatible = types_compatible_p (type, gimple_call_fntype (stmt));
  tree arg = TYPE_ARG_TYPES (type);
  for (unsigned i = 0; compatible && i < gimple_call_num_args (stmt); ++i)
    {
      if (!arg || TREE_VALUE (arg) == void_type_node
          || !types_compatible_p (TREE_VALUE (arg), TREE_TYPE (gimple_call_arg (stmt, i)))) compatible = false;
      else arg = TREE_CHAIN (arg);
    }
  compatible &= arg && TREE_VALUE (arg) == void_type_node && !TREE_CHAIN (arg);
  tree lhs = gimple_call_lhs (stmt);
  if (lhs && !types_compatible_p (TREE_TYPE (lhs), TREE_TYPE (type))) compatible = false;
  return compatible;
}
/* Check declaration and field references against the owning transformed body,
   rather than preserving pre-outlining parameter/field links as approval. */
static bool check_remapped_operands (tree fn)
{
  function *body = DECL_STRUCT_FUNCTION (fn);
  if (!body || !body->cfg) return false;
  struct inspection {
    tree owner;
    gimple *statement;
    bool valid;
    static tree visit (tree *node, int *descend, void *data)
    {
      walk_stmt_info *wi = (walk_stmt_info *) data;
      inspection *self = (inspection *) wi->info;
      bool valid = true;
      if (TREE_CODE (*node) == PARM_DECL) valid = DECL_CONTEXT (*node) == self->owner;
      if (TREE_CODE (*node) == COMPONENT_REF)
        {
          tree type = TREE_TYPE (TREE_OPERAND (*node, 0));
          tree field = TREE_OPERAND (*node, 1);
          valid = type && (TREE_CODE (type) == RECORD_TYPE || TREE_CODE (type) == UNION_TYPE);
          bool found = false;
          if (valid)
            for (tree p = TYPE_FIELDS (type); p; p = DECL_CHAIN (p))
              if (p == field) { found = true; break; }
          valid &= found;
        }
      if (!valid)
        { error_at (newtm_current_diagnostic_location (self->owner, self->statement), "transaction transformed operand lost its frame correspondence");
          newtm_current_diagnostic_context (self->owner, self->statement, "V03", "frame operand"); self->valid = false; }
      if (DECL_P (*node) || TYPE_P (*node)) *descend = 0;
      return NULL_TREE;
    }
  } v = { fn, NULL, true };
  basic_block bb;
  FOR_EACH_BB_FN (bb, body)
    for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
      {
        v.statement = gsi_stmt (it);
        if (is_gimple_debug (v.statement)) continue;
        walk_stmt_info wi = {}; wi.info = &v;
        walk_gimple_op (v.statement, inspection::visit, &wi);
      }
  return v.valid;
}
static bool check_lowered_controller (tree helper, tree owner,
                                      tree *actual = NULL, tree *snapshot = NULL)
{
  unsigned count = 0;
  bool valid = true;
  hash_set<tree> checked;
  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    {
      function *body = DECL_STRUCT_FUNCTION (node->decl);
      if (!body || !body->cfg) continue;
      basic_block bb;
      FOR_EACH_BB_FN (bb, body)
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          {
            gimple *stmt = gsi_stmt (it);
            if (!is_gimple_call (stmt) || gimple_call_fndecl (stmt) != helper) continue;
            ++count;
            tree before = snapshot ? newtm_current_ir (node->decl) : NULL_TREE;
            if (snapshot && !before)
              { error_at (newtm_current_diagnostic_location (node->decl, stmt), "transaction current controller cannot be snapshotted");
                newtm_current_diagnostic_context (node->decl, stmt, "V08", "controller snapshot"); return false; }
            if (!checked.add (node->decl) && !check_remapped_operands (node->decl)) valid = false;
            if (!lowered_owner_matches (node->decl, owner))
              { error_at (newtm_current_diagnostic_location (node->decl, stmt), "transaction controller lost its lowered body correspondence: owner mismatch");
                newtm_current_diagnostic_context (node->decl, stmt, "V07", "controller ownership"); valid = false; }
            else if (!lowered_call_signature (stmt, helper))
              { error_at (newtm_current_diagnostic_location (node->decl, stmt), "transaction controller lost its lowered body correspondence: incompatible prepared signature");
                newtm_current_diagnostic_context (node->decl, stmt, "V06", "prepared signature"); valid = false; }
            else if (!check_capture_origins (helper, body, stmt))
              { error_at (newtm_current_diagnostic_location (node->decl, stmt), "transaction capture changed its remapping origin");
                newtm_current_diagnostic_context (node->decl, stmt, "V06", "capture origin"); valid = false; }
            else if (!newtm_channels_verify_entry (node->decl, stmt)) valid = false;
            else if (!newtm_status_verify_entry (node->decl, stmt)) valid = false;
            else if (!newtm_owner_verify_entry (node->decl, stmt)) valid = false;
            else if (!newtm_coverage_verify_entry (node->decl, stmt)) valid = false;
            if (valid && snapshot)
              {
                tree after = newtm_current_ir (node->decl);
                if (!after || TREE_STRING_LENGTH (before) != TREE_STRING_LENGTH (after)
                    || memcmp (TREE_STRING_POINTER (before), TREE_STRING_POINTER (after), TREE_STRING_LENGTH (before)))
                  { error_at (newtm_current_diagnostic_location (node->decl, stmt), "transaction controller changed during current verification");
                    newtm_current_diagnostic_context (node->decl, stmt, "V08", "controller proof freshness"); return false; }
                *actual = node->decl; *snapshot = after;
              }
          }
    }
  if (count != 1)
    { error_at (newtm_current_diagnostic_location (helper, NULL), "transaction controller lacks a unique lowered body invocation");
      newtm_current_diagnostic_context (helper, NULL, "V07", "unique body invocation"); valid = false; }
  return valid;
}

bool
newtm_clone_matches_preflight (tree fn, tree actual)
{
  if (!fn || !newtm_access_prepared_p (fn) || newtm_clone_region_rejection (fn)) return false;
  tree expected = NULL_TREE;
  if (newtm_call_dependencies)
    if (tree *p = newtm_call_dependencies->get (fn)) expected = *p;
  for (tree p = actual; p; p = TREE_CHAIN (p))
    { tree q; for (q = expected; q && TREE_VALUE (q) != TREE_VALUE (p); q = TREE_CHAIN (q)) ; if (!q) return false; }
  for (tree p = expected; p; p = TREE_CHAIN (p))
    { tree q; for (q = actual; q && TREE_VALUE (q) != TREE_VALUE (p); q = TREE_CHAIN (q)) ; if (!q) return false; }
  return true;
}

tree newtm_clone_region_owner (tree fn)
{
  if (tree original = newtm_clone_original (fn)) fn = original;
  tree *p = newtm_region_owners ? newtm_region_owners->get (fn) : NULL;
  return p ? *p : NULL_TREE;
}

const char *newtm_clone_region_rejection (tree fn)
{
  tree *p = newtm_region_rejections ? newtm_region_rejections->get (fn) : NULL;
  return p ? TREE_STRING_POINTER (*p) : NULL;
}

static bool require_prepared_graph (tree fn, hash_set<tree> &visited)
{
  if (visited.add (fn)) return true; /* Recursive edges are checked once. */
  if (!newtm_access_prepared_p (fn))
    { error_at (DECL_SOURCE_LOCATION (fn), "transaction body has not been prepared"); return false; }
  if (const char *reason = newtm_clone_region_rejection (fn))
    { error_at (DECL_SOURCE_LOCATION (fn), "transaction body cannot be prepared: %s", reason); return false; }
  if (newtm_call_dependencies)
    if (tree *list = newtm_call_dependencies->get (fn))
      for (tree p = *list; p; p = TREE_CHAIN (p))
        if (!require_prepared_graph (TREE_VALUE (p), visited)) return false;
  return true;
}

/* Context identity over the current non-SSA CFG. Only unexposed local pointer
   values participate; descriptor origins and old parameter links are unused. */
static bool check_lowered_context (tree fn)
{
  function *body = DECL_STRUCT_FUNCTION (fn);
  if (!body || !body->cfg) return false;
  tree context = DECL_ARGUMENTS (fn);
  hash_set<tree> exposed;
  basic_block scan;
  FOR_EACH_BB_FN (scan, body)
    for (gimple_stmt_iterator it = gsi_start_bb (scan); !gsi_end_p (it); gsi_next (&it))
      {
        struct address_scan {
          static tree visit (tree *node, int *descend, void *data)
          {
            walk_stmt_info *wi = (walk_stmt_info *) data;
            hash_set<tree> *objects = (hash_set<tree> *) wi->info;
            if (TREE_CODE (*node) == ADDR_EXPR)
              {
                tree base = get_base_address (TREE_OPERAND (*node, 0));
                if (base && DECL_P (base)) objects->add (base);
              }
            if (DECL_P (*node) || TYPE_P (*node)) *descend = 0;
            return NULL_TREE;
          }
        };
        walk_stmt_info wi = {}; wi.info = &exposed;
        walk_gimple_op (gsi_stmt (it), address_scan::visit, &wi);
      }
  hash_map<tree, unsigned> indices;
  unsigned count = 0;
  if (context && POINTER_TYPE_P (TREE_TYPE (context)) && !TREE_ADDRESSABLE (context) && !exposed.contains (context))
    indices.put (context, count++);
  basic_block bb;
  FOR_EACH_BB_FN (bb, body)
    for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
      if (is_gimple_assign (gsi_stmt (it)))
        {
          tree lhs = gimple_assign_lhs (gsi_stmt (it));
          if ((VAR_P (lhs) || TREE_CODE (lhs) == PARM_DECL)
              && DECL_CONTEXT (lhs) == fn && POINTER_TYPE_P (TREE_TYPE (lhs))
              && !TREE_ADDRESSABLE (lhs) && !exposed.contains (lhs) && !indices.get (lhs)) indices.put (lhs, count++);
        }
  hash_set<basic_block> reachable;
  auto_vec<basic_block> work;
  work.safe_push (ENTRY_BLOCK_PTR_FOR_FN (body));
  while (!work.is_empty ())
    {
      basic_block at = work.pop ();
      if (reachable.add (at)) continue;
      edge e; edge_iterator ei;
      FOR_EACH_EDGE (e, ei, at->succs) work.safe_push (e->dest);
    }
  struct bitmap_arena {
    bitmap_obstack storage;
    bitmap_arena () { bitmap_obstack_initialize (&storage); }
    ~bitmap_arena () { bitmap_obstack_release (&storage); }
  } arena;
  auto_vec<bitmap> output;
  output.safe_grow_cleared (last_basic_block_for_fn (body));
  FOR_EACH_BB_FN (bb, body)
    { output[bb->index] = BITMAP_ALLOC (&arena.storage); bitmap_set_range (output[bb->index], 0, count); }
  auto_bitmap entry (&arena.storage), input (&arena.storage), state (&arena.storage);
  if (unsigned *i = indices.get (context)) bitmap_set_bit (entry, *i);
  auto known = [&] (tree value, bitmap facts) {
    unsigned *i = indices.get (value);
    return i && bitmap_bit_p (facts, *i);
  };
  auto transfer = [&] (gimple *stmt, bitmap facts) {
    if (is_gimple_assign (stmt))
      {
        tree lhs = gimple_assign_lhs (stmt);
        unsigned *i = indices.get (lhs);
        if (!i) return;
        bool copy = (gimple_assign_single_p (stmt)
                     || CONVERT_EXPR_CODE_P (gimple_assign_rhs_code (stmt)))
          && POINTER_TYPE_P (TREE_TYPE (gimple_assign_rhs1 (stmt)))
          && useless_type_conversion_p (TREE_TYPE (lhs), TREE_TYPE (gimple_assign_rhs1 (stmt)))
          && known (gimple_assign_rhs1 (stmt), facts);
        if (copy) bitmap_set_bit (facts, *i); else bitmap_clear_bit (facts, *i);
      }
    else if (is_gimple_call (stmt))
      { if (unsigned *i = indices.get (gimple_call_lhs (stmt))) bitmap_clear_bit (facts, *i); }
    else if (gimple_code (stmt) == GIMPLE_ASM) bitmap_clear (facts);
  };
  auto incoming = [&] (basic_block block) {
    bitmap_clear (input); bitmap_set_range (input, 0, count);
    edge e; edge_iterator ei;
    FOR_EACH_EDGE (e, ei, block->preds)
      if (reachable.contains (e->src))
        bitmap_and_into (input, e->src == ENTRY_BLOCK_PTR_FOR_FN (body) ? entry : output[e->src->index]);
  };
  bool change;
  do
    {
      change = false;
      FOR_EACH_BB_FN (bb, body)
        if (reachable.contains (bb))
          {
            incoming (bb); bitmap_copy (state, input);
            for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it)) transfer (gsi_stmt (it), state);
            if (!bitmap_equal_p (state, output[bb->index]))
              { bitmap_copy (output[bb->index], state); change = true; }
          }
    }
  while (change);
  bool valid = true;
  FOR_EACH_BB_FN (bb, body)
    if (reachable.contains (bb))
      {
        incoming (bb); bitmap_copy (state, input);
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          {
            gimple *stmt = gsi_stmt (it);
            if (is_gimple_call (stmt) && !gimple_call_internal_p (stmt))
              {
                tree callee = gimple_call_fndecl (stmt);
                bool borrowed = callee && newtm_clone_original (callee);
                if (callee && fndecl_built_in_p (callee, BUILT_IN_NORMAL))
                  switch (DECL_FUNCTION_CODE (callee))
                    {
                    case BUILT_IN_GOMP_TRANSACTION_STM_V1_READ:
                    case BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE:
                    case BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER:
                    case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS:
                    case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL:
                    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN:
                    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END: borrowed = true; break;
                    default: break;
                    }
                if (borrowed && (!gimple_call_num_args (stmt) || !known (gimple_call_arg (stmt, 0), state)))
                  { error_at (newtm_current_diagnostic_location (fn, stmt), "transaction transformed call lost its borrowed context");
                    newtm_current_diagnostic_context (fn, stmt, "V06", "borrowed context call"); valid = false; }
              }
            transfer (stmt, state);
          }
      }
  FOR_EACH_BB_FN (bb, body) BITMAP_FREE (output[bb->index]);
  return valid;
}

/* Inspect the actual lowered body.  The declaration-only dependency snapshot
   limits the allowed targets but cannot stand in for inspecting those calls. */
static bool check_lowered_calls (tree fn, tree expected)
{
  function *body = DECL_STRUCT_FUNCTION (fn);
  if (!body || !body->cfg)
    { error_at (newtm_current_diagnostic_location (fn, NULL), "transaction graph lacks a lowered body for %qD", fn);
      newtm_current_diagnostic_context (fn, NULL, "V06", "prepared call"); return false; }
  bool valid = true;
  basic_block bb;
  FOR_EACH_BB_FN (bb, body)
    for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
      {
        gimple *stmt = gsi_stmt (it);
        if (!is_gimple_call (stmt) || gimple_call_internal_p (stmt)) continue;
        tree callee = gimple_call_fndecl (stmt);
        if (callee && fndecl_built_in_p (callee, BUILT_IN_NORMAL)) continue;
        bool allowed = false;
        for (tree p = expected; callee && p; p = TREE_CHAIN (p))
          if (TREE_VALUE (p) == callee && newtm_clone_original (callee)) allowed = true;
        if (!allowed)
          { error_at (newtm_current_diagnostic_location (fn, stmt), "transaction transformed body has an unverified call target");
            newtm_current_diagnostic_context (fn, stmt, "V06", "prepared call"); valid = false; continue; }
        bool compatible = lowered_call_signature (stmt, callee);
        if (!compatible)
          { error_at (newtm_current_diagnostic_location (fn, stmt), "transaction transformed prepared call has incompatible channels");
            newtm_current_diagnostic_context (fn, stmt, "V06", "prepared call"); valid = false; }
      }
  return valid;
}

/* Reconstruct registration requirements from current call operands, not from
   retired descriptor fields. This is the CFG foundation of block 2. */
static bool check_lowered_registrations (tree fn)
{
  function *body = DECL_STRUCT_FUNCTION (fn);
  bool present = false;
  basic_block bb;
  FOR_EACH_BB_FN (bb, body)
    for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
      {
        gimple *stmt = gsi_stmt (it);
        if (is_gimple_call (stmt) && gimple_call_fndecl (stmt)
            && fndecl_built_in_p (gimple_call_fndecl (stmt), BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER)) present = true;
      }
  if (!present) return true;
  newtm_flow_graph flow;
  const char *why = NULL; gimple *bad = NULL;
  if (!flow.build_cfg (body, &why, &bad))
    { error_at (newtm_current_diagnostic_location (fn, bad), "transaction memory CFG cannot be inspected: %s", why);
      newtm_current_diagnostic_context (fn, bad, "V02", "REGISTER CFG inspection"); return false; }
  newtm_guard_analysis guards (flow);
  bool valid = true;
  hash_set<tree> registration_sites;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    {
      gimple *stmt = flow.node (i).statement;
      if (!stmt || !is_gimple_call (stmt) || !gimple_call_fndecl (stmt)
          || !fndecl_built_in_p (gimple_call_fndecl (stmt), BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER)) continue;
      if (gimple_call_num_args (stmt) != 4)
        { error_at (newtm_current_diagnostic_location (fn, stmt), "transaction transformed registration has incompatible operands");
          newtm_current_diagnostic_context (fn, stmt, "V06", "REGISTER service signature"); return false; }
      tree object = gimple_call_arg (stmt, 1), extent = gimple_call_arg (stmt, 2), kind = gimple_call_arg (stmt, 3);
      tree ptruint = unsigned_type_for (pointer_sized_int_node);
      tree wide = build_nonstandard_integer_type (MAX (TYPE_PRECISION (size_type_node), TYPE_PRECISION (ptruint)), true);
      tree address = fold_convert (wide, fold_convert (ptruint, object));
      tree room = fold_build2 (MINUS_EXPR, wide, fold_convert (wide, TYPE_MAX_VALUE (ptruint)), address);
      tree span = fold_build2 (MINUS_EXPR, size_type_node,
        fold_build2 (MAX_EXPR, size_type_node, extent, size_one_node), size_one_node);
      tree tests[] = {
        fold_build2 (NE_EXPR, boolean_type_node, object, build_zero_cst (TREE_TYPE (object))),
        fold_build2 (NE_EXPR, boolean_type_node, extent, size_zero_node),
        fold_build2 (LE_EXPR, boolean_type_node, kind, build_int_cst (TREE_TYPE (kind), NEWTM_ABI_AGGREGATE)),
        fold_build2 (LE_EXPR, boolean_type_node, fold_convert (wide, span), room)
      };
      bool guarded = true;
      for (tree predicate : tests)
        if (!guards.require (stmt, predicate, &why, &bad))
          { error_at (newtm_current_diagnostic_location (fn, stmt), "transaction transformed registration lacks an identity guard: %s", why);
            newtm_current_diagnostic_context (fn, stmt, "V03", "REGISTER"); valid = false; guarded = false; break; }
      if (guarded && registration_origin_matches (fn, stmt, guards, registration_sites) == REGISTRATION_INVALID)
        { error_at (newtm_current_diagnostic_location (fn, stmt), "transaction transformed registration changed its object identity");
          newtm_current_diagnostic_context (fn, stmt, "V03", "REGISTER"); valid = false; }
    }
  return valid;
}

/* Follow the definitions at their use points. A checked result needs the flag
   of its own producer, even when a wrapped value would fit the final range. */
static bool check_offset_arithmetic (tree fn, gimple *call, tree value, tree required, newtm_guard_analysis &guards)
{
  tree *records = newtm_offset_arithmetic ? newtm_offset_arithmetic->get (fn) : NULL;
  if (!records || required == error_mark_node)
    { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed offset lacks arithmetic provenance");
      newtm_current_diagnostic_context (fn, call, "V03", "offset arithmetic"); return false; }
  struct use { tree value; gimple *at; unsigned depth; };
  auto_vec<use> work;
  use root = { value, call, 0 }; work.safe_push (root);
  hash_set<gimple *> seen;
  hash_set<tree> found;
  while (!work.is_empty ())
    {
      use current = work.pop ();
      tree t = current.value;
      if (!t) continue;
      if (current.depth > 64)
        { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed offset exceeds arithmetic proof depth");
          newtm_current_diagnostic_context (fn, call, "V03", "offset arithmetic"); return false; }
      if (DECL_P (t) || TREE_CODE (t) == SSA_NAME)
        {
          tree record = offset_arithmetic_record (*records, t);
          gimple *def = guards.definition_at (current.at, t);
          if (!def)
            {
              if (record) { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed offset lost its checked arithmetic");
              newtm_current_diagnostic_context (fn, call, "V03", "offset arithmetic"); return false; }
              continue; /* Input value; no claim about source-language arithmetic. */
            }
          if (seen.add (def)) continue;
          if (record)
            {
              if (!is_gimple_assign (def) || gimple_assign_rhs_code (def) != REALPART_EXPR)
                { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed offset lost its checked arithmetic");
                  newtm_current_diagnostic_context (fn, call, "V03", "offset arithmetic"); return false; }
              tree pair = TREE_OPERAND (gimple_assign_rhs1 (def), 0);
              gimple *producer = guards.definition_at (def, pair);
              tree type = TREE_TYPE (pair);
              bool compatible = type && TREE_CODE (type) == COMPLEX_TYPE;
              tree part = compatible ? TREE_TYPE (type) : NULL_TREE;
              compatible = compatible && INTEGRAL_TYPE_P (part)
                && remap_origin (pair) == TREE_VEC_ELT (record, 1)
                && TYPE_PRECISION (part) == tree_to_uhwi (TREE_VEC_ELT (record, 3))
                && TYPE_UNSIGNED (part) == bool (integer_onep (TREE_VEC_ELT (record, 4)))
                && types_compatible_p (TREE_TYPE (gimple_assign_lhs (def)), part)
                && producer && is_gimple_call (producer) && gimple_call_internal_p (producer)
                && gimple_call_num_args (producer) == 2
                && (unsigned) gimple_call_internal_fn (producer) == tree_to_uhwi (TREE_VEC_ELT (record, 2));
              if (!compatible)
                { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed offset lost its checked arithmetic");
                  newtm_current_diagnostic_context (fn, call, "V03", "offset arithmetic"); return false; }
              tree flag = build1 (IMAGPART_EXPR, part, pair);
              tree predicate = fold_build2 (EQ_EXPR, boolean_type_node, flag, build_zero_cst (part));
              const char *why = NULL;
              if (!guards.require (call, predicate, &why))
                { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed offset lacks an overflow guard: %s", why);
                  newtm_current_diagnostic_context (fn, call, "V03", "offset arithmetic"); return false; }
              found.add (TREE_VEC_ELT (record, 0));
            }
          if (is_gimple_assign (def))
            for (unsigned i = 1; i < gimple_num_ops (def); ++i)
              { use next = { gimple_op (def, i), def, current.depth + 1 }; work.safe_push (next); }
          else if (is_gimple_call (def) && gimple_call_internal_p (def))
            for (unsigned i = 0; i < gimple_call_num_args (def); ++i)
              { use next = { gimple_call_arg (def, i), def, current.depth + 1 }; work.safe_push (next); }
        }
      else if (EXPR_P (t) && TREE_CODE (t) != ADDR_EXPR
               && (!REFERENCE_CLASS_P (t) || TREE_CODE (t) == REALPART_EXPR || TREE_CODE (t) == IMAGPART_EXPR))
        for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
          { use next = { TREE_OPERAND (t, i), current.at, current.depth + 1 }; work.safe_push (next); }
    }
  for (tree p = required; p; p = TREE_CHAIN (p))
    if (!found.contains (remap_origin (TREE_VALUE (p))))
      { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed offset lost its checked arithmetic");
        newtm_current_diagnostic_context (fn, call, "V03", "offset arithmetic"); return false; }
  return true;
}

/* Additional static-object READ/WRITE checks. The common tuple, storage and
   arithmetic proof (including transported identities) follows this gate. */
static bool check_lowered_memory (tree fn)
{
  tree *records = newtm_memory_origins ? newtm_memory_origins->get (fn) : NULL;
  if (!records)
    { error_at (newtm_current_diagnostic_location (fn, NULL), "transaction transformed access lacks its preparation summary");
      newtm_current_diagnostic_context (fn, NULL, "V08", "preparation summary"); return false; }
  newtm_flow_graph flow;
  const char *why = NULL; gimple *bad = NULL;
  if (!flow.build_cfg (DECL_STRUCT_FUNCTION (fn), &why, &bad))
    { error_at (newtm_current_diagnostic_location (fn, bad), "transaction access CFG cannot be inspected: %s", why);
      newtm_current_diagnostic_context (fn, bad, "V02", "READ/WRITE CFG inspection"); return false; }
  newtm_guard_analysis guards (flow);
  hash_set<tree> seen;
  bool valid = true;
  for (unsigned i = 0; i < flow.node_count (); ++i)
    {
      gimple *call = flow.node (i).statement;
      if (!call || !is_gimple_call (call) || !gimple_call_fndecl (call)
          || !fndecl_built_in_p (gimple_call_fndecl (call), BUILT_IN_NORMAL)) continue;
      built_in_function op = DECL_FUNCTION_CODE (gimple_call_fndecl (call));
      if (op != BUILT_IN_GOMP_TRANSACTION_STM_V1_READ && op != BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE) continue;
      if (gimple_call_num_args (call) != 5)
        { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed access has incompatible operands");
          newtm_current_diagnostic_context (fn, call, "V06", "READ/WRITE service signature"); valid = false; continue; }
      tree site = remap_origin (gimple_call_lhs (call)), proof = NULL_TREE;
      if (site && !seen.add (site))
        for (tree p = *records; p; p = TREE_CHAIN (p))
          if (TREE_VEC_ELT (TREE_VALUE (p), 0) == site) { proof = TREE_VALUE (p); break; }
      if (!proof || tree_to_uhwi (TREE_VEC_ELT (proof, 5)) != (unsigned) op)
        { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed access lost its operation identity");
          newtm_current_diagnostic_context (fn, call, "V01", "required memory operation"); valid = false; continue; }
      if (!TREE_VEC_ELT (proof, 1)) continue; /* Handled by the common memory proof. */
      tree object = gimple_call_arg (call, 1), offset = gimple_call_arg (call, 2), size = gimple_call_arg (call, 4);
      tree actual = current_registration_object (object, call, guards);
      tree expected = TREE_VEC_ELT (proof, 1);
      tree size_value = current_registration_constant (size, call, guards);
      tree offset_value = TREE_VEC_ELT (proof, 6) ? current_registration_constant (offset, call, guards) : NULL_TREE;
      if (!actual || !integer_onep (TREE_PURPOSE (actual)) || TREE_VALUE (actual) != remap_origin (TREE_VALUE (expected))
          || !size_value || !tree_int_cst_equal (size_value, TREE_VEC_ELT (proof, 3))
          || (TREE_VEC_ELT (proof, 6) && (!offset_value || !tree_int_cst_equal (offset_value, TREE_VEC_ELT (proof, 6)))))
        { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed access changed its static identity");
          newtm_current_diagnostic_context (fn, call, "V03", "READ/WRITE"); valid = false; continue; }
      tree extent = TREE_VEC_ELT (proof, 2);
      tree ptruint = unsigned_type_for (pointer_sized_int_node);
      tree wide = build_nonstandard_integer_type (MAX (TYPE_PRECISION (size_type_node), TYPE_PRECISION (ptruint)), true);
      tree address = fold_convert (wide, fold_convert (ptruint, object));
      tree room = fold_build2 (MINUS_EXPR, wide, fold_convert (wide, TYPE_MAX_VALUE (ptruint)), address);
      tree span = fold_build2 (MINUS_EXPR, size_type_node, fold_build2 (MAX_EXPR, size_type_node, extent, size_one_node), size_one_node);
      tree bounded = fold_build2 (MIN_EXPR, size_type_node, offset, extent);
      tree available = fold_build2 (MINUS_EXPR, size_type_node, extent, bounded);
      tree predicates[] = {
        fold_build2 (NE_EXPR, boolean_type_node, object, build_zero_cst (TREE_TYPE (object))),
        fold_build2 (LE_EXPR, boolean_type_node, fold_convert (wide, span), room),
        fold_build2 (LE_EXPR, boolean_type_node, fold_convert (wide, offset), room),
        fold_build2 (LE_EXPR, boolean_type_node, offset, extent),
        fold_build2 (LE_EXPR, boolean_type_node, size, available)
      };
      bool ranged = true;
      for (tree predicate : predicates)
        if (!guards.require (call, predicate, &why, &bad))
          { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed access lacks a range guard: %s", why);
            newtm_current_diagnostic_context (fn, call, "V03", "READ/WRITE"); valid = false; ranged = false; break; }
      if (integer_zerop (TREE_VEC_ELT (proof, 4)) && !integer_zerop (TREE_VEC_ELT (proof, 3)))
        {
          tree whole = fold_build2 (BIT_AND_EXPR, boolean_type_node,
            fold_build2 (EQ_EXPR, boolean_type_node, offset, size_zero_node),
            fold_build2 (EQ_EXPR, boolean_type_node, size, extent));
          if (!guards.require (call, whole, &why, &bad))
            { error_at (newtm_current_diagnostic_location (fn, call), "transaction transformed scalar access is not whole-object");
              newtm_current_diagnostic_context (fn, call, "V03", "READ/WRITE"); valid = false; ranged = false; }
        }
      if (ranged && !check_offset_arithmetic (fn, call, offset, TREE_VEC_ELT (proof, 7), guards)) valid = false;
    }
  return valid;
}

/* Check the complete historical local graph without transferring approval to
   lowered IR. Remapping and activation certificates require a later proof. */
static bool
check_local_graph (tree fn, hash_set<tree> &visited, tree *revisions)
{
  if (visited.add (fn)) return true;
  newtm_local_view local = {};
  bool found = false;
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_local_count (); ++i)
    {
      newtm_local_view candidate;
      if (newtm_obligation_local_get (i, &candidate) && candidate.owner == fn
          && (candidate.scope_kind == NEWTM_OB_CLONE || candidate.scope_kind == NEWTM_OB_REGION))
        { local = candidate; found = true; }
    }
  if (!found || !local.closed || local.state != NEWTM_LOCAL_APPROVED || local.graph_invalid
      || local.required != local.proved)
    {
      error_at (DECL_SOURCE_LOCATION (fn), "transaction graph lacks an approved local verification for %qD", fn);
      return false;
    }
  *revisions = tree_cons (NULL_TREE, build_int_cst (long_long_unsigned_type_node, local.id), *revisions);
  tree expected = NULL_TREE;
  if (newtm_call_dependencies)
    if (tree *p = newtm_call_dependencies->get (fn)) expected = *p;
  for (tree p = local.dependencies; p; p = TREE_CHAIN (p))
    {
      tree callee = TREE_VALUE (p), q;
      for (q = expected; q && TREE_VALUE (q) != callee; q = TREE_CHAIN (q)) ;
      if (!q)
        { error_at (DECL_SOURCE_LOCATION (fn), "transaction graph has an unexpected local dependency from %qD", fn); return false; }
    }
  for (tree p = expected; p; p = TREE_CHAIN (p))
    {
      tree callee = TREE_VALUE (p), q;
      for (q = local.dependencies; q && TREE_VALUE (q) != callee; q = TREE_CHAIN (q)) ;
      if (!q)
        { error_at (DECL_SOURCE_LOCATION (fn), "transaction graph is missing a locally verified dependency from %qD", fn); return false; }
      if (!check_local_graph (callee, visited, revisions)) return false;
    }
  return true;
}

bool newtm_clone_require_region (tree fn)
{
  if (!newtm_clone_region_owner (fn))
    { error_at (DECL_SOURCE_LOCATION (fn), "transaction has no prepared region body"); return false; }
  hash_set<tree> visited;
  return require_prepared_graph (fn, visited);
}

/* Consume the GENERIC association while its source node is still live.
   Instruction copies never inherit this operational association. */
void newtm_clone_bind_region (gimple *stmt, tree source)
{
  tree *p = newtm_region_sources ? newtm_region_sources->get (source) : NULL;
  if (!p) return;
  if (!newtm_region_bodies) newtm_region_bodies = hash_map<gimple *, tree>::create_ggc ();
  newtm_region_bodies->put (stmt, newtm_clone_for (*p));
  newtm_region_sources->remove (source);
}
tree newtm_clone_region_body (const gimple *stmt)
{
  tree *p = newtm_region_bodies ? newtm_region_bodies->get (const_cast<gimple *> (stmt)) : NULL;
  return p ? *p : NULL_TREE;
}
void newtm_clone_forget_region (const gimple *stmt)
{
  if (newtm_region_bodies) newtm_region_bodies->remove (const_cast<gimple *> (stmt));
}

tree newtm_clone_for (tree fn)
{
  tree *p = newtm_clones ? newtm_clones->get (fn) : NULL;
  return p ? *p : NULL_TREE;
}
static tree info (tree fn)
{
  tree *p = newtm_clone_info ? newtm_clone_info->get (fn) : NULL;
  return p ? *p : NULL_TREE;
}
tree newtm_clone_original (tree fn)
{ tree p = info (fn); return p ? TREE_VEC_ELT (p, 0) : NULL_TREE; }
tree newtm_clone_context (tree fn)
{ tree p = info (fn); return p ? TREE_VEC_ELT (p, 1) : NULL_TREE; }
tree newtm_clone_channel (tree fn, unsigned index)
{ tree p = info (fn); return p && index >= 2 && index <= 4 ? TREE_VEC_ELT (p, index) : NULL_TREE; }
bool newtm_clone_publication_p (tree fn, const gimple *stmt)
{
  tree p = info (fn);
  return p && is_gimple_call (stmt)
    && gimple_call_fndecl (stmt) == TREE_VEC_ELT (p, 5);
}

tree newtm_clone_field (tree record, unsigned index)
{
  tree field = TYPE_FIELDS (TREE_TYPE (record));
  while (index--) field = DECL_CHAIN (field);
  return build3 (COMPONENT_REF, TREE_TYPE (field), unshare_expr (record), field, NULL_TREE);
}
tree newtm_clone_identity (tree transport)
{ return newtm_clone_field (transport, 0); }
tree newtm_clone_storage (tree transport)
{ return newtm_clone_field (transport, 1); }


tree newtm_clone_captures (tree fn)
{
  if (tree original = newtm_clone_original (fn)) fn = original;
  tree *p = newtm_capture_lists ? newtm_capture_lists->get (fn) : NULL;
  return p ? *p : NULL_TREE;
}
tree newtm_clone_capture_value (tree fn, tree declaration)
{
  if (newtm_region_parameters)
    if (tree *list = newtm_region_parameters->get (fn))
      for (tree p = *list; p; p = TREE_CHAIN (p))
        if (TREE_PURPOSE (p) == declaration) return build_simple_mem_ref (TREE_VALUE (p));
  if (!newtm_clone_region_owner (fn)
      && newtm_clone_original (fn) && newtm_lexical_decls)
    if (tree *p = newtm_lexical_decls->get (declaration)) return *p;
  return declaration;
}
/* Pointer-object identity and pointee provenance are distinct channels even
   when both originate at the same captured declaration. Never let a nested
   registration of the pointer object replace its pointee's canonical key. */
tree newtm_clone_pointer_root (tree fn, tree declaration)
{
  for (tree p = newtm_clone_captures (fn); p; p = TREE_CHAIN (p))
    if (!integer_zerop (TREE_PURPOSE (p))
        && operand_equal_p (newtm_clone_capture_value (fn, TREE_VALUE (p)), declaration, 0)) return p;
  return declaration;
}

tree newtm_clone_pointer_key (tree fn, tree value)
{
  if (newtm_clone_region_owner (fn))
    for (tree p = newtm_clone_captures (fn); p; p = TREE_CHAIN (p))
      if (operand_equal_p (newtm_clone_capture_value (fn, TREE_VALUE (p)), value, 0)) return p;
  return value;
}

static tree binding_at (tree data, unsigned slot)
{
  tree type = newtm_abi_clone_binding_type ();
  tree pointer = TREE_VEC_ELT (data, 2);
  return build2 (MEM_REF, type, pointer,
                 build_int_cst (TREE_TYPE (pointer),
                                slot * tree_to_uhwi (TYPE_SIZE_UNIT (type))));
}
static unsigned explicit_bindings (tree original)
{
  unsigned count = 0;
  /* An unused original body may already have lost DECL_ARGUMENTS when the
     remapped CFG is checked.  The preserved signature owns the channel count. */
  for (tree p = TYPE_ARG_TYPES (TREE_TYPE (original)); p; p = TREE_CHAIN (p))
    if (POINTER_TYPE_P (TREE_VALUE (p))) ++count;
  return count;
}
static unsigned capture_bindings (tree fn)
{
  unsigned count = 0;
  for (tree p = newtm_clone_captures (fn); p; p = TREE_CHAIN (p))
    count += integer_zerop (TREE_PURPOSE (p)) ? 1 : 2;
  return count;
}

tree newtm_clone_transport_extent (tree fn, tree pointer, bool write)
{
  tree data = info (fn);
  if (!data) return NULL_TREE;
  if (!write && pointer == TREE_VEC_ELT (data, 2))
    return size_int ((explicit_bindings (newtm_clone_original (fn))
                      + capture_bindings (fn))
                     * tree_to_uhwi (TYPE_SIZE_UNIT (newtm_abi_clone_binding_type ())));
  if (write && pointer && (pointer == TREE_VEC_ELT (data, 3)
                           || pointer == TREE_VEC_ELT (data, 4)))
    return TYPE_SIZE_UNIT (TREE_TYPE (TREE_TYPE (pointer)));
  return NULL_TREE;
}

const char *newtm_clone_check_call (const gimple *stmt)
{
  tree callee = gimple_call_fndecl (stmt);
  tree data = info (current_function_decl);
  gcc_assert (data);
  tree clone = newtm_clone_for (callee);
  bool publication = callee && callee == TREE_VEC_ELT (data, 5);
  if (!clone && !publication)
    return "source call has no prepared implementation";
  if (clone)
    {
      tree owner = DECL_CONTEXT (clone), scope = current_function_decl;
      if (owner && TREE_CODE (owner) == FUNCTION_DECL)
        {
          while (scope && TREE_CODE (scope) == FUNCTION_DECL && scope != owner)
            scope = DECL_CONTEXT (scope);
          if (scope != owner)
            return "prepared nested call requires a different lexical frame";
        }
      unsigned count = 0;
      for (tree formal = DECL_ARGUMENTS (callee); formal; formal = DECL_CHAIN (formal)) ++count;
      if (count != gimple_call_num_args (stmt))
        return "prepared call has incompatible argument count";
    }
  auto executable = [&] (unsigned index)
    {
      tree d = newtm_descriptor_find (stmt, NEWTM_DESC_ARGUMENT, index);
      if (!d || tree_to_uhwi (TREE_VEC_ELT (d, NEWTM_D_STATE)) > NEWTM_DESC_RESULT)
        return false;
      const unsigned fields[] = { NEWTM_D_OBJECT, NEWTM_D_EXTENT, NEWTM_D_OFFSET,
                                  NEWTM_D_KIND, NEWTM_D_VALID, NEWTM_D_STORAGE };
      for (unsigned field : fields) if (!TREE_VEC_ELT (d, field)) return false;
      return true;
    };
  for (unsigned i = 0; i < gimple_call_num_args (stmt); ++i)
    if (POINTER_TYPE_P (TREE_TYPE (gimple_call_arg (stmt, i))) && !executable (i))
      return "pointer argument has no executable descriptor";
  unsigned index = gimple_call_num_args (stmt);
  if (clone)
    {
      for (unsigned i = 0; i < capture_bindings (callee); ++i)
        if (!executable (index++))
          return "prepared capture has no executable descriptor";
      tree lhs = gimple_call_lhs (stmt);
      if (lhs && TREE_CODE (TREE_TYPE (lhs)) == POINTER_TYPE
          && !newtm_clone_result_binding (stmt))
        return "prepared pointer result has no binding channel";
      if (!newtm_call_dependencies)
        newtm_call_dependencies = hash_map<tree, tree>::create_ggc ();
      tree *old = newtm_call_dependencies->get (current_function_decl);
      for (tree p = old ? *old : NULL_TREE; p; p = TREE_CHAIN (p))
        if (TREE_VALUE (p) == clone) return NULL;
      newtm_call_dependencies->put (current_function_decl,
        tree_cons (NULL_TREE, clone, old ? *old : NULL_TREE));
    }
  return NULL;
}
tree newtm_clone_capture_object_binding (tree fn, tree declaration)
{
  tree data = info (fn);
  if (!data) return NULL_TREE;
  unsigned slot = explicit_bindings (TREE_VEC_ELT (data, 0));
  for (tree p = newtm_clone_captures (fn); p; p = TREE_CHAIN (p))
    {
      if (newtm_clone_capture_value (fn, TREE_VALUE (p)) == declaration)
        return binding_at (data, slot);
      slot += integer_zerop (TREE_PURPOSE (p)) ? 1 : 2;
    }
  return NULL_TREE;
}
tree newtm_clone_parameter_binding (tree fn, tree parm)
{
  tree p = info (fn);
  if (!p) return NULL_TREE;
  if (newtm_clone_region_owner (fn))
    {
      tree *list = newtm_region_parameters->get (fn);
      unsigned slot = 0;
      for (tree capture = newtm_clone_captures (fn), item = list ? *list : NULL_TREE;
           capture && item; capture = TREE_CHAIN (capture), item = TREE_CHAIN (item))
        {
          if (parm == TREE_VALUE (item)) return binding_at (p, slot);
          if (!integer_zerop (TREE_PURPOSE (capture))
              && operand_equal_p (parm, newtm_clone_capture_value (fn, TREE_VALUE (capture)), 0))
            return binding_at (p, slot + 1);
          slot += integer_zerop (TREE_PURPOSE (capture)) ? 1 : 2;
        }
      return NULL_TREE;
    }
  tree original = TREE_VEC_ELT (p, 0);
  tree actual = DECL_CHAIN (DECL_CHAIN (DECL_ARGUMENTS (fn)));
  unsigned slot = 0;
  for (tree formal = DECL_ARGUMENTS (original); formal;
       formal = DECL_CHAIN (formal), actual = DECL_CHAIN (actual))
    {
      if (actual == parm)
        {
          if (!POINTER_TYPE_P (TREE_TYPE (formal))) return NULL_TREE;
          return binding_at (p, slot);
        }
      if (POINTER_TYPE_P (TREE_TYPE (formal))) ++slot;
    }
  for (tree capture = newtm_clone_captures (fn); capture; capture = TREE_CHAIN (capture))
    {
      if (!integer_zerop (TREE_PURPOSE (capture)))
        {
          if (newtm_clone_capture_value (fn, TREE_VALUE (capture)) == parm)
            return binding_at (p, slot + 1);
          slot += 2;
        }
      else ++slot;
    }
  return NULL_TREE;
}

tree newtm_clone_result_binding (const gimple *stmt)
{
  tree *p = newtm_clone_results
    ? newtm_clone_results->get (const_cast<gimple *> (stmt)) : NULL;
  return p ? *p : NULL_TREE;
}

static tree parameter (tree fn, const char *name, tree type)
{
  tree p = build_decl (DECL_SOURCE_LOCATION (fn), PARM_DECL,
                       get_identifier (name), type);
  DECL_CONTEXT (p) = fn;
  DECL_ARG_TYPE (p) = type;
  DECL_ARTIFICIAL (p) = 1;
  TREE_USED (p) = 1;
  return p;
}

static void prototype (tree original)
{
  tree result_type = TREE_TYPE (TREE_TYPE (original));
  tree binding_pointer = build_pointer_type
    (build_qualified_type (newtm_abi_clone_binding_type (), TYPE_QUAL_CONST));
  auto_vec<tree> types;
  types.safe_push (newtm_abi_context_type ());
  types.safe_push (binding_pointer);
  for (tree p = DECL_ARGUMENTS (original); p; p = DECL_CHAIN (p))
    types.safe_push (TREE_TYPE (p));
  if (newtm_clone_region_owner (original))
    for (tree p = newtm_clone_captures (original); p; p = TREE_CHAIN (p))
      types.safe_push (build_pointer_type (TREE_TYPE (TREE_VALUE (p))));
  if (TREE_CODE (result_type) != VOID_TYPE)
    types.safe_push (build_pointer_type (result_type));
  if (TREE_CODE (result_type) == POINTER_TYPE)
    types.safe_push (build_pointer_type (newtm_abi_clone_binding_type ()));
  const char *name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (original));
  char *clone_name = concat (name, ".newtm.v1", NULL);
  tree clone = build_fn_decl (clone_name,
      build_function_type_array (uint32_type_node, types.length (), types.address ()));
  free (clone_name);
  SET_DECL_ASSEMBLER_NAME (clone, DECL_NAME (clone));
  DECL_EXTERNAL (clone) = 0;
  TREE_PUBLIC (clone) = 0;
  TREE_STATIC (clone) = 1;
  TREE_USED (clone) = 1;
  DECL_PRESERVE_P (clone) = 1;
  DECL_UNINLINABLE (clone) = 1;
  DECL_SOURCE_LOCATION (clone) = DECL_SOURCE_LOCATION (original);
  DECL_CONTEXT (clone) = DECL_CONTEXT (original);
  if (newtm_clone_region_owner (original))
    while (DECL_CONTEXT (clone) && TREE_CODE (DECL_CONTEXT (clone)) == FUNCTION_DECL)
      DECL_CONTEXT (clone) = DECL_CONTEXT (DECL_CONTEXT (clone));
  DECL_FUNCTION_SPECIFIC_TARGET (clone) = DECL_FUNCTION_SPECIFIC_TARGET (original);
  DECL_FUNCTION_SPECIFIC_OPTIMIZATION (clone) = DECL_FUNCTION_SPECIFIC_OPTIMIZATION (original);
  if (!newtm_prepared_prototypes) newtm_prepared_prototypes = hash_map<tree, tree>::create_ggc ();
  newtm_prepared_prototypes->put (clone, copy_list (TYPE_ARG_TYPES (TREE_TYPE (clone))));
  tree data = make_tree_vec (8);
  TREE_VEC_ELT (data, 0) = original;
  tree *tail = &DECL_ARGUMENTS (clone);
  *tail = TREE_VEC_ELT (data, 1) = parameter (clone, "newtm_context", types[0]);
  tail = &DECL_CHAIN (*tail);
  *tail = TREE_VEC_ELT (data, 2) = parameter (clone, "newtm_bindings", types[1]);
  tail = &DECL_CHAIN (*tail);
  for (tree p = DECL_ARGUMENTS (original); p; p = DECL_CHAIN (p))
    {
      *tail = parameter (clone, DECL_NAME (p) ? IDENTIFIER_POINTER (DECL_NAME (p)) : "argument",
                         TREE_TYPE (p));
      TREE_ADDRESSABLE (*tail) = TREE_ADDRESSABLE (p);
      TREE_READONLY (*tail) = TREE_READONLY (p);
      DECL_ARG_TYPE (*tail) = DECL_ARG_TYPE (p);
      DECL_BY_REFERENCE (*tail) = DECL_BY_REFERENCE (p);
      newtm_lexical_decls->put (p, *tail);
      tail = &DECL_CHAIN (*tail);
    }
  if (newtm_clone_region_owner (original))
    {
      tree list = NULL_TREE, *item = &list;
      for (tree p = newtm_clone_captures (original); p; p = TREE_CHAIN (p))
        {
          *tail = parameter (clone, "newtm_capture", build_pointer_type (TREE_TYPE (TREE_VALUE (p))));
          *item = tree_cons (TREE_VALUE (p), *tail, NULL_TREE);
          item = &TREE_CHAIN (*item); tail = &DECL_CHAIN (*tail);
        }
      newtm_region_parameters->put (clone, list);
    }
  if (TREE_CODE (result_type) != VOID_TYPE)
    {
      *tail = TREE_VEC_ELT (data, 3) = parameter (clone, "newtm_result", build_pointer_type (result_type));
      tail = &DECL_CHAIN (*tail);
    }
  if (TREE_CODE (result_type) == POINTER_TYPE)
    *tail = TREE_VEC_ELT (data, 4) = parameter (clone, "newtm_result_binding",
                                      build_pointer_type (newtm_abi_clone_binding_type ()));
  DECL_RESULT (clone) = build_decl (DECL_SOURCE_LOCATION (clone), RESULT_DECL,
                                   NULL_TREE, uint32_type_node);
  DECL_CONTEXT (DECL_RESULT (clone)) = clone;
  /* A temporary marker gives the descriptor builder an evaluated pointer
     result operand. It is removed before callgraph construction/omplower. */
  tree marker_type = TREE_CODE (result_type) == VOID_TYPE
    ? build_function_type_list (uint32_type_node, NULL_TREE)
    : build_function_type_list (uint32_type_node, result_type, NULL_TREE);
  TREE_VEC_ELT (data, 5) = build_fn_decl ("__newtm_publish_result", marker_type);
  newtm_clones->put (original, clone);
  if (!newtm_prepared_parameter_origins) newtm_prepared_parameter_origins = hash_map<tree, tree>::create_ggc ();
  tree parameter_origins = NULL_TREE, *origin_tail = &parameter_origins;
  for (tree p = DECL_ARGUMENTS (clone); p; p = DECL_CHAIN (p))
    { *origin_tail = tree_cons (NULL_TREE, p, NULL_TREE); origin_tail = &TREE_CHAIN (*origin_tail); }
  newtm_prepared_parameter_origins->put (clone, parameter_origins);
  newtm_clone_info->put (clone, data);
}

/* The generic copier handles declaration maps, SAVE_EXPR sharing, block
   scopes, language-specific declarations and reference representations. */
static tree copy_source (tree *tp, int *walk, void *argument)
{
  tree old = *tp;
  copy_body_data *id = (copy_body_data *) argument;
  if (!newtm_clone_region_owner (id->dst_fn)
      && (VAR_P (old) || TREE_CODE (old) == PARM_DECL)
      && DECL_CONTEXT (old) != id->src_fn)
    if (tree *mapped = newtm_lexical_decls->get (old))
      { *tp = *mapped; *walk = 0; return NULL_TREE; }
  tree result = copy_tree_body_r (tp, walk, argument);
  if (TREE_CODE (old) == DEBUG_BEGIN_STMT)
    if (tree read = newtm_source_read (old))
      {
        walk_tree (&read, copy_tree_body_r, argument, NULL);
        newtm_bind_source_marker (*tp, read);
      }
  if (TREE_CODE (*tp) == OMP_TRANSACTION)
    OMP_TRANSACTION_PLAN (*tp) = NULL_TREE; /* No caller-specific recipes. */
  return result;
}

/* The inliner has internal recursive walks (notably ADDR_EXPR) that do not
   invoke COPY_SOURCE for every descendant. Finish lexical capture remapping
   over the copied body, including initializers and source-only witnesses. */
static tree remap_lexical (tree *tp, int *walk, void *argument)
{
  tree t = *tp;
  if (VAR_P (t) || TREE_CODE (t) == PARM_DECL)
    {
      if (argument && newtm_clone_region_owner ((tree) argument))
        for (tree p = newtm_clone_captures ((tree) argument); p; p = TREE_CHAIN (p))
          if (TREE_VALUE (p) == t)
            { *tp = newtm_clone_capture_value ((tree) argument, t); *walk = 0; return NULL_TREE; }
      if (tree *mapped = newtm_lexical_decls->get (t)) *tp = *mapped;
      *walk = 0; return NULL_TREE;
    }
  if (DECL_P (t) || TYPE_P (t)) { *walk = 0; return NULL_TREE; }
  if (TREE_CODE (t) == ADDR_EXPR)
    {
      walk_tree (&TREE_OPERAND (t, 0), remap_lexical, argument, NULL);
      recompute_tree_invariant_for_addr_expr (t);
      *walk = 0;
    }
  if (TREE_CODE (t) == DEBUG_BEGIN_STMT)
    if (tree witness = newtm_source_read (t))
      {
        walk_tree (&witness, remap_lexical, argument, NULL);
        newtm_bind_source_marker (t, witness);
      }
  return NULL_TREE;
}

static tree rewrite_return (tree *tp, int *walk, void *argument)
{
  tree clone = (tree) argument;
  tree t = *tp;
  if (DECL_P (t) || TYPE_P (t)) { *walk = 0; return NULL_TREE; }
  if (TREE_CODE (t) != RETURN_EXPR) return NULL_TREE;
  tree data = info (clone), marker = TREE_VEC_ELT (data, 5);
  tree body = alloc_stmt_list (), value = TREE_OPERAND (t, 0);
  if (value && (TREE_CODE (value) == MODIFY_EXPR || TREE_CODE (value) == INIT_EXPR))
    {
      append_to_statement_list (value, &body);
      value = TREE_OPERAND (value, 0);
    }
  tree call = value ? build_call_expr_loc (EXPR_LOCATION (t), marker, 1, value)
                    : build_call_expr_loc (EXPR_LOCATION (t), marker, 0);
  tree assignment = build2 (MODIFY_EXPR, uint32_type_node, DECL_RESULT (clone), call);
  append_to_statement_list (build1 (RETURN_EXPR, void_type_node, assignment), &body);
  *tp = body; *walk = 0;
  return NULL_TREE;
}

static void body (tree original)
{
  tree clone = newtm_clone_for (original);
  push_struct_function (clone, false);
  copy_body_data id = {};
  hash_map<tree, tree> map;
  id.src_fn = original; id.dst_fn = clone;
  id.src_cfun = DECL_STRUCT_FUNCTION (original);
  id.decl_map = &map; id.copy_decl = copy_decl_no_change;
  id.transform_parameter = true;
  id.do_not_unshare = true; id.do_not_fold = true;
  id.transform_call_graph_edges = CB_CGE_DUPLICATE;
  tree actual = DECL_CHAIN (DECL_CHAIN (DECL_ARGUMENTS (clone)));
  for (tree p = DECL_ARGUMENTS (original); p; p = DECL_CHAIN (p), actual = DECL_CHAIN (actual))
    insert_decl_map (&id, p, actual);
  tree result_type = TREE_TYPE (TREE_TYPE (original));
  tree temporary = NULL_TREE;
  if (TREE_CODE (result_type) != VOID_TYPE)
    {
      temporary = build_decl (DECL_SOURCE_LOCATION (clone), VAR_DECL,
                               get_identifier ("newtm_return_value"), result_type);
      DECL_CONTEXT (temporary) = clone; DECL_ARTIFICIAL (temporary) = 1;
      TREE_ADDRESSABLE (temporary) = AGGREGATE_TYPE_P (result_type);
      tree replacement = DECL_BY_REFERENCE (DECL_RESULT (original))
        ? build_fold_addr_expr (temporary) : temporary;
      insert_decl_map (&id, DECL_RESULT (original), replacement);
    }
  tree copied = DECL_SAVED_TREE (original);
  walk_tree (&copied, copy_source, &id, NULL);
  for (auto it = map.begin (); it != map.end (); ++it)
    if ((VAR_P ((*it).first) || TREE_CODE ((*it).first) == PARM_DECL)
        && DECL_P ((*it).second) && (*it).first != (*it).second)
      newtm_lexical_decls->put ((*it).first, (*it).second);
  /* Generic declaration initializers are not expression-tree children. Copy
     them after all lexical locals have entries in the declaration map. */
  auto_vec<tree> initialized;
  for (auto it = map.begin (); it != map.end (); ++it)
    if (VAR_P ((*it).first) && DECL_P ((*it).second)
        && (*it).first != (*it).second && DECL_INITIAL ((*it).first))
      initialized.safe_push ((*it).first);
  for (tree old : initialized)
    {
      tree initial = DECL_INITIAL (old);
      walk_tree (&initial, copy_source, &id, NULL);
      walk_tree (&initial, remap_lexical, clone, NULL);
      DECL_INITIAL (*map.get (old)) = initial;
    }
  walk_tree (&copied, remap_lexical, clone, NULL);
  walk_tree_without_duplicates (&copied, rewrite_return, clone);
  /* Include an implicit void return, also reached by empty void functions. */
  tree statements = alloc_stmt_list ();
  append_to_statement_list (copied, &statements);
  if (TREE_CODE (result_type) == VOID_TYPE)
    {
      tree ret = build1 (RETURN_EXPR, void_type_node, NULL_TREE);
      int walk = 1; rewrite_return (&ret, &walk, clone);
      append_to_statement_list (ret, &statements);
    }
  else
    {
      /* Falling off a non-void original cannot fabricate a successful value. */
      /* The status epilogue condemns this fallthrough after gimplification.
         Do not expose an ABI context argument as a source memory argument. */
      tree fail = build_int_cst (uint32_type_node, NEWTM_ABI_INVALID);
      tree assign = build2 (MODIFY_EXPR, uint32_type_node, DECL_RESULT (clone), fail);
      append_to_statement_list (build1 (RETURN_EXPR, void_type_node, assign), &statements);
    }
  tree block = make_node (BLOCK);
  BLOCK_SUPERCONTEXT (block) = clone;
  DECL_INITIAL (clone) = block;
  DECL_SAVED_TREE (clone) = build3 (BIND_EXPR, void_type_node, temporary, statements, block);
  pop_cfun ();
  newtm_occurrence_select (clone);
  /* Native nested functions are finalized by unnest_nesting_tree_1 after
     static-chain conversion, just like their originals. Finalizing them here
     would redefine a still-nested function during that pass. */
  if (!DECL_CONTEXT (clone) || TREE_CODE (DECL_CONTEXT (clone)) != FUNCTION_DECL)
    cgraph_node::finalize_function (clone, true);
  cgraph_node::get_create (clone)->force_output = true;
}

/* Captures are implicit references to lexical objects. Pointer-value reads
   additionally carry pointee provenance. Address-taking/write-only captures
   do not fabricate or read an uninitialized pointer value. */
static bool ancestor (tree owner, tree fn)
{
  for (tree p = DECL_CONTEXT (fn); p && TREE_CODE (p) == FUNCTION_DECL;
       p = DECL_CONTEXT (p)) if (p == owner) return true;
  return false;
}
struct capture_scan
{
  tree fn;
  bool changed = false, control = false, statement_definition = false;
  auto_vec<tree> leading_definitions;
  hash_set<tree> *locals = NULL;
  capture_scan (tree f) : fn (f) {}
};
static void add_capture (capture_scan &s, tree decl, bool value)
{
  if (s.locals && s.locals->contains (decl)) return;
  if ((!VAR_P (decl) && TREE_CODE (decl) != PARM_DECL)
      || TREE_STATIC (decl) || DECL_EXTERNAL (decl)
      || !DECL_CONTEXT (decl) || !ancestor (DECL_CONTEXT (decl), s.fn)) return;
  value &= POINTER_TYPE_P (TREE_TYPE (decl));
  for (tree assigned : s.leading_definitions)
    if (assigned == decl) value = false;
  tree *list = newtm_capture_lists->get (s.fn);
  for (tree p = list ? *list : NULL_TREE; p; p = TREE_CHAIN (p))
    if (TREE_VALUE (p) == decl)
      {
        if (value && integer_zerop (TREE_PURPOSE (p)))
          { TREE_PURPOSE (p) = boolean_true_node; s.changed = true; }
        return;
      }
  TREE_ADDRESSABLE (decl) = 1; /* Before any parent/child is gimplified. */
  tree added = tree_cons (value ? boolean_true_node : boolean_false_node,
                         decl, list ? *list : NULL_TREE);
  newtm_capture_lists->put (s.fn, added); s.changed = true;
}
static tree collect_capture (tree *tp, int *walk, void *argument)
{
  capture_scan &s = *(capture_scan *) argument;
  tree t = *tp;
  tree_code code = TREE_CODE (t);
  if (code == STATEMENT_LIST)
    {
      bool saved = s.statement_definition;
      for (tree_stmt_iterator it = tsi_start (t); !tsi_end_p (it); tsi_next (&it))
        {
          tree statement = tsi_stmt (it);
          s.statement_definition = true;
          walk_tree (&statement, collect_capture, argument, NULL);
        }
      s.statement_definition = saved;
      *walk = 0; return NULL_TREE;
    }
  bool leading_statement = s.statement_definition;
  s.statement_definition = false;
  if (code == COND_EXPR || code == IF_STMT || code == LOOP_EXPR
      || code == GOTO_EXPR || code == LABEL_EXPR || code == SWITCH_EXPR
      || code == SWITCH_STMT || code == FOR_STMT || code == WHILE_STMT
      || code == DO_STMT) s.control = true;
  if (code == DEBUG_BEGIN_STMT)
    if (tree witness = newtm_source_read (t))
      walk_tree_without_duplicates (&witness, collect_capture, argument);
  if (TREE_CODE (t) == DECL_EXPR)
    {
      tree d = DECL_EXPR_DECL (t);
      if (VAR_P (d) && DECL_INITIAL (d))
        {
          tree initial = DECL_INITIAL (d);
          walk_tree_without_duplicates (&initial, collect_capture, argument);
        }
    }
  if (TREE_CODE (t) == ADDR_EXPR && DECL_P (TREE_OPERAND (t, 0)))
    { add_capture (s, TREE_OPERAND (t, 0), false); *walk = 0; return NULL_TREE; }
  if ((TREE_CODE (t) == MODIFY_EXPR || TREE_CODE (t) == INIT_EXPR)
      && DECL_P (TREE_OPERAND (t, 0)))
    {
      add_capture (s, TREE_OPERAND (t, 0), false);
      tree rhs = TREE_OPERAND (t, 1);
      walk_tree_without_duplicates (&rhs, collect_capture, argument);
      if (leading_statement && !s.control
          && POINTER_TYPE_P (TREE_TYPE (TREE_OPERAND (t, 0))))
        s.leading_definitions.safe_push (TREE_OPERAND (t, 0));
      *walk = 0; return NULL_TREE;
    }
  if (DECL_P (t)) { add_capture (s, t, true); *walk = 0; }
  if (TYPE_P (t)) *walk = 0;
  if (TREE_CODE (t) == CALL_EXPR)
    if (tree fn = get_callee_fndecl (t))
      for (tree p = newtm_clone_captures (fn); p; p = TREE_CHAIN (p))
        add_capture (s, TREE_VALUE (p), !integer_zerop (TREE_PURPOSE (p)));
  return NULL_TREE;
}
static void lexical_body (tree fn, hash_set<tree> &done)
{
  if (done.contains (fn)) return;
  tree parent = DECL_CONTEXT (fn);
  if (!newtm_clone_region_owner (fn) && parent && TREE_CODE (parent) == FUNCTION_DECL && newtm_clone_for (parent))
    lexical_body (parent, done);
  body (fn); done.add (fn);
}
static int decl_order (const void *a, const void *b)
{
  unsigned x = DECL_UID (*(const tree *) a), y = DECL_UID (*(const tree *) b);
  return x < y ? -1 : x != y;
}
static int capture_order (const void *a, const void *b)
{
  unsigned x = DECL_UID (TREE_VALUE (*(const tree *) a));
  unsigned y = DECL_UID (TREE_VALUE (*(const tree *) b));
  return x < y ? -1 : x != y;
}

/* A source region gets its own synthetic void source function. Only its
   prepared copy is emitted. The original pragma remains on the tracing path
   until the external slot controller is integrated. */
struct region_scan { tree owner; auto_vec<tree> *seeds; };
static tree region_locals (tree *tp, int *walk, void *arg)
{
  tree t = *tp;
  hash_set<tree> &locals = *(hash_set<tree> *) arg;
  if (TREE_CODE (t) == BIND_EXPR)
    for (tree d = BIND_EXPR_VARS (t); d; d = DECL_CHAIN (d)) locals.add (d);
  if (TREE_CODE (t) == DECL_EXPR) locals.add (DECL_EXPR_DECL (t));
  if (TREE_CODE (t) == TARGET_EXPR) locals.add (TARGET_EXPR_SLOT (t));
  if (DECL_P (t) || TYPE_P (t)) *walk = 0;
  return NULL_TREE;
}
static tree collect_region (tree *tp, int *walk, void *arg)
{
  tree t = *tp;
  if (DECL_P (t) || TYPE_P (t)) { *walk = 0; return NULL_TREE; }
  if (TREE_CODE (t) != OMP_TRANSACTION) return NULL_TREE;
  region_scan &s = *(region_scan *) arg;
  char *name = xasprintf ("%s.newtm.body.%u",
                         IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (s.owner)),
                         DECL_UID (s.owner) + s.seeds->length ());
  tree seed = build_fn_decl (name, build_function_type_list (void_type_node, NULL_TREE));
  free (name);
  DECL_CONTEXT (seed) = s.owner;
  DECL_FUNCTION_SPECIFIC_TARGET (seed) = DECL_FUNCTION_SPECIFIC_TARGET (s.owner);
  DECL_FUNCTION_SPECIFIC_OPTIMIZATION (seed) = DECL_FUNCTION_SPECIFIC_OPTIMIZATION (s.owner);
  DECL_SOURCE_LOCATION (seed) = EXPR_LOCATION (t);
  DECL_RESULT (seed) = build_decl (EXPR_LOCATION (t), RESULT_DECL, NULL_TREE, void_type_node);
  DECL_CONTEXT (DECL_RESULT (seed)) = seed;
  newtm_region_owners->put (seed, s.owner);
  newtm_region_sources->put (t, seed);
  hash_set<tree> locals;
  tree source = t;
  walk_tree_without_duplicates (&source, region_locals, &locals);
  capture_scan scan (seed); scan.locals = &locals;
  walk_tree_without_duplicates (&source, collect_capture, &scan);
  /* Retain captures in the original lexical frame; copy only objects whose
     lifetime starts inside this attempt. Never copy their initial values. */
  copy_body_data id = {};
  hash_map<tree, tree> map;
  id.src_fn = s.owner; id.dst_fn = seed;
  id.src_cfun = DECL_STRUCT_FUNCTION (s.owner);
  id.decl_map = &map; id.copy_decl = copy_decl_no_change;
  id.transform_parameter = true; id.do_not_unshare = true; id.do_not_fold = true;
  id.transform_call_graph_edges = CB_CGE_DUPLICATE;
  for (tree p = newtm_clone_captures (seed); p; p = TREE_CHAIN (p))
    insert_decl_map (&id, TREE_VALUE (p), TREE_VALUE (p));
  push_struct_function (seed, false);
  walk_tree (&source, copy_source, &id, NULL);
  for (auto it = map.begin (); it != map.end (); ++it)
    if (VAR_P ((*it).first) && (*it).first != (*it).second && DECL_INITIAL ((*it).first))
      {
        tree initial = DECL_INITIAL ((*it).first);
        walk_tree (&initial, copy_source, &id, NULL);
        DECL_INITIAL ((*it).second) = initial;
      }
  tree block = make_node (BLOCK); BLOCK_SUPERCONTEXT (block) = seed;
  DECL_INITIAL (seed) = block;
  DECL_SAVED_TREE (seed) = build3 (BIND_EXPR, void_type_node, NULL_TREE, source, block);
  pop_cfun ();
  s.seeds->safe_push (seed);
  *walk = 0; /* Nested regions belong to the same attempt, not new roots. */
  return NULL_TREE;
}

void newtm_clone_unit ()
{
  auto_vec<tree> selected;
  newtm_occurrence_selected_functions (selected);
  auto_vec<tree> owners, seeds;
  cgraph_node *node;
  FOR_EACH_FUNCTION (node)
    if (DECL_SAVED_TREE (node->decl)) owners.safe_push (node->decl);
  owners.qsort (decl_order);
  selected.qsort (decl_order);
  newtm_clones = hash_map<tree, tree>::create_ggc ();
  newtm_clone_info = hash_map<tree, tree>::create_ggc ();
  newtm_capture_lists = hash_map<tree, tree>::create_ggc ();
  newtm_lexical_decls = hash_map<tree, tree>::create_ggc ();
  newtm_region_owners = hash_map<tree, tree>::create_ggc ();
  newtm_region_sources = hash_map<tree, tree>::create_ggc ();
  newtm_region_parameters = hash_map<tree, tree>::create_ggc ();
  newtm_region_rejections = hash_map<tree, tree>::create_ggc ();
  for (tree owner : owners)
    {
      tree source = DECL_SAVED_TREE (owner);
      region_scan scan = { owner, &seeds };
      walk_tree_without_duplicates (&source, collect_region, &scan);
    }
  selected.safe_splice (seeds);
  bool changed;
  do
    {
      changed = false;
      for (tree fn : selected)
        {
          capture_scan scan (fn);
          tree source = DECL_SAVED_TREE (fn);
          walk_tree_without_duplicates (&source, collect_capture, &scan);
          changed |= scan.changed;
        }
    }
  while (changed);
  for (tree fn : selected)
    {
      auto_vec<tree> captures;
      for (tree p = newtm_clone_captures (fn); p; p = TREE_CHAIN (p)) captures.safe_push (p);
      captures.qsort (capture_order);
      tree list = NULL_TREE, *tail = &list;
      for (tree p : captures) { *tail = p; tail = &TREE_CHAIN (p); }
      *tail = NULL_TREE;
      newtm_capture_lists->put (fn, list);
    }
  for (tree fn : selected) prototype (fn);
  for (tree fn : selected)
    {
      tree parent = DECL_CONTEXT (fn);
      if (!newtm_clone_region_owner (fn) && parent && TREE_CODE (parent) == FUNCTION_DECL)
        if (tree clone_parent = newtm_clone_for (parent))
          DECL_CONTEXT (newtm_clone_for (fn)) = clone_parent;
    }
  hash_set<tree> done;
  for (tree fn : selected) lexical_body (fn, done);
}

struct result_scan { tree fn; gbind *scope; };
static tree scan_result (gimple_stmt_iterator *it, bool *, walk_stmt_info *wi)
{
  gimple *stmt = gsi_stmt (*it);
  if (!is_gimple_call (stmt) || !newtm_clone_for (gimple_call_fndecl (stmt))) return NULL_TREE;
  tree lhs = gimple_call_lhs (stmt);
  if (!lhs || TREE_CODE (TREE_TYPE (lhs)) != POINTER_TYPE) return NULL_TREE;
  result_scan &s = *(result_scan *) wi->info;
  tree binding = create_tmp_var (newtm_abi_clone_binding_type (), "newtm_return_binding");
  TREE_ADDRESSABLE (binding) = 1;
  suppress_warning (binding, OPT_Wuninitialized);
  DECL_CHAIN (binding) = gimple_bind_vars (s.scope);
  gimple_bind_set_vars (s.scope, binding);
  if (!newtm_clone_results) newtm_clone_results = hash_map<gimple *, tree>::create_ggc ();
  newtm_clone_results->put (stmt, binding);
  return NULL_TREE;
}
void newtm_clone_scan_results (tree fn)
{
  if (!info (fn)) return;
  gbind *scope = as_a<gbind *> (gimple_seq_first_stmt (gimple_body (fn)));
  result_scan s = { fn, scope };
  walk_stmt_info wi = {}; wi.info = &s;
  walk_gimple_seq (gimple_body (fn), scan_result, NULL, &wi);
}

namespace {
struct emission
{
  tree fn, ctx;
  tree logical_address = NULL_TREE;
  gbind *scope;
  gimple_seq seq = NULL;
  location_t loc;
  emission (tree f, gbind *b, location_t l)
    : fn (f), ctx (newtm_clone_context (f)), scope (b), loc (l) {}
  tree temp (tree type, const char *name)
  {
    tree t = create_tmp_var (type, name);
    DECL_CHAIN (t) = gimple_bind_vars (scope); gimple_bind_set_vars (scope, t);
    return t;
  }
  void add (gimple *s)
  { gimple_set_location (s, loc); gimple_seq_add_stmt_without_update (&seq, s); }
  void math (gimple *s) { newtm_obligation_math (s); add (s); }
  tree value (tree expression)
  {
    if (is_gimple_val (expression)) return expression;
    tree result = temp (TREE_TYPE (expression), "newtm_abi_value");
    add (gimple_build_assign (result, expression));
    return result;
  }
  tree cast (tree type, tree expression)
  { return value (fold_convert (type, value (expression))); }
  tree compare (tree_code code, tree a, tree b)
  {
    tree result = temp (boolean_type_node, "newtm_abi_check");
    add (gimple_build_assign (result, code, value (a), value (b)));
    return result;
  }
  tree runtime (built_in_function operation, auto_vec<tree> &args)
  {
    tree status = temp (uint32_type_node, "newtm_status");
    add (newtm_abi_build_call (operation, status, args.length (), args.address ()));
    return status;
  }
  void check (tree status)
  {
    tree yes = create_artificial_label (loc), no = create_artificial_label (loc);
    add (gimple_build_cond (EQ_EXPR, status, build_zero_cst (uint32_type_node), yes, no));
    add (gimple_build_label (no));
    add (gimple_build_assign (TREE_VEC_ELT (info (fn), 6), status));
    add (gimple_build_goto (TREE_VEC_ELT (info (fn), 7)));
    add (gimple_build_label (yes));
  }
  void guard (tree valid)
  {
    tree yes = create_artificial_label (loc), no = create_artificial_label (loc);
    gcond *condition = gimple_build_cond (NE_EXPR, valid, build_zero_cst (TREE_TYPE (valid)), yes, no);
    add (condition); newtm_obligation_abort (condition, no);
    add (gimple_build_label (no));
    add (gimple_build_assign (TREE_VEC_ELT (info (fn), 6),
                             build_int_cst (uint32_type_node, NEWTM_ABI_INVALID)));
    add (gimple_build_goto (TREE_VEC_ELT (info (fn), 7)));
    add (gimple_build_label (yes));
  }
  void status ()
  { auto_vec<tree> args; args.safe_push (ctx); check (runtime (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS, args)); }
  void access_address (const newtm_access &access, const char *name)
  {
    guard (access.valid);
    /* VALID proves the last-byte and subrange arithmetic. Compare integers
       without constructing a canonical pointer outside the object. */
    tree ptruint = unsigned_type_for (pointer_sized_int_node);
    tree wide = build_nonstandard_integer_type
      (MAX (TYPE_PRECISION (size_type_node), TYPE_PRECISION (ptruint)), true);
    tree object = cast (wide, cast (ptruint, access.object));
    tree pointer = value (build_fold_addr_expr (unshare_expr (access.reference)));
    if (gimple *formation = gimple_seq_last_stmt (seq))
      if (is_gimple_assign (formation) && gimple_assign_lhs (formation) == pointer)
        note_access (formation, NEWTM_MEMORY_ADDRESS, access);
    tree logical = cast (wide, cast (ptruint, pointer));
    logical_address = logical;
    tree position = temp (wide, name);
    math (gimple_build_assign (position, PLUS_EXPR, object, cast (wide, access.offset)));
    note_access (gimple_seq_last_stmt (seq), NEWTM_MEMORY_ADDRESS, access);
    guard (compare (EQ_EXPR, logical, position));
  }
  void note_access (gimple *stmt, newtm_memory_role role, const newtm_access &a)
  {
    newtm_memory_note (stmt, role, a.object, a.extent, a.offset, a.size,
                       a.kind, a.storage,
                       role == NEWTM_MEMORY_ADDRESS ? NULL_TREE : logical_address);
  }
  bool fill (tree transport, tree descriptor)
  {
    if (!descriptor || !TREE_VEC_ELT (descriptor, NEWTM_D_VALID)
        || !TREE_VEC_ELT (descriptor, NEWTM_D_STORAGE))
      {
        error_at (loc, "prepared function binding has no executable canonical identity");
        return false;
      }
    guard (TREE_VEC_ELT (descriptor, NEWTM_D_VALID));
    tree identity = newtm_clone_identity (transport);
    const unsigned fields[] = { NEWTM_D_OBJECT, NEWTM_D_EXTENT, NEWTM_D_OFFSET, NEWTM_D_KIND };
    for (unsigned i = 0; i < 4; ++i)
      add (gimple_build_assign (newtm_clone_field (identity, i), TREE_VEC_ELT (descriptor, fields[i])));
    add (gimple_build_assign (newtm_clone_storage (transport), TREE_VEC_ELT (descriptor, NEWTM_D_STORAGE)));
    return true;
  }
};

static void transform_sequence (gimple_seq *, tree, gbind *, unsigned &);
static void transform_address (gimple_stmt_iterator *it, tree fn, gbind *scope)
{
  gimple *s = gsi_stmt (*it);
  tree d = newtm_descriptor_find (s, NEWTM_DESC_ADDRESS);
  if (!d) return;
  if (!TREE_VEC_ELT (d, NEWTM_D_VALID)
      || tree_to_uhwi (TREE_VEC_ELT (d, NEWTM_D_STATE)) > NEWTM_DESC_RESULT)
    { error_at (gimple_location (s), "transaction address has no executable identity"); return; }
  emission e (fn, scope, gimple_location (s));
  e.guard (TREE_VEC_ELT (d, NEWTM_D_VALID));
  if (is_gimple_assign (s))
    newtm_memory_note (s, NEWTM_MEMORY_ADDRESS,
      TREE_VEC_ELT (d, NEWTM_D_OBJECT), TREE_VEC_ELT (d, NEWTM_D_EXTENT),
      TREE_VEC_ELT (d, NEWTM_D_OFFSET), TREE_VEC_ELT (d, NEWTM_D_SIZE),
      TREE_VEC_ELT (d, NEWTM_D_KIND), TREE_VEC_ELT (d, NEWTM_D_STORAGE));
  newtm_obligation_emitted (e.seq, s);
  gsi_insert_seq_before_without_update (it, e.seq, GSI_SAME_STMT);
}
static void transform_call (gimple_stmt_iterator *it, tree fn, gbind *scope, unsigned &depth)
{
  gimple *stmt = gsi_stmt (*it);
  tree callee = gimple_call_fndecl (stmt), data = info (fn);
  emission e (fn, scope, gimple_location (stmt));
  if (callee == TREE_VEC_ELT (data, 5))
    {
      e.status ();
      tree output = TREE_VEC_ELT (data, 3);
      if (output)
        {
          tree value = gimple_call_arg (stmt, 0);
          tree binding_out = TREE_VEC_ELT (data, 4);
          if (binding_out)
            {
              tree descriptor = newtm_descriptor_find (stmt, NEWTM_DESC_ARGUMENT, 0);
              tree private_binding = e.temp (newtm_abi_clone_binding_type (), "newtm_publication_binding");
              if (e.fill (private_binding, descriptor))
                {
                  tree integer = unsigned_type_for (pointer_sized_int_node);
                  tree canonical = e.temp (integer, "newtm_publication_address");
                  e.math (gimple_build_assign (canonical, PLUS_EXPR,
                    e.cast (integer, TREE_VEC_ELT (descriptor, NEWTM_D_OBJECT)),
                    e.cast (integer, TREE_VEC_ELT (descriptor, NEWTM_D_OFFSET))));
                  e.guard (e.compare (EQ_EXPR, e.cast (integer, value), canonical));
                  e.add (gimple_build_assign (build_simple_mem_ref (binding_out), private_binding));
                }
            }
          e.add (gimple_build_assign (build_simple_mem_ref (output), value));
        }
      tree lhs = gimple_call_lhs (stmt);
      if (lhs) e.add (gimple_build_assign (lhs, build_zero_cst (uint32_type_node)));
    }
  else if (tree clone = newtm_clone_for (callee))
    {
      auto_vec<tree> args;
      args.safe_push (e.ctx);
      unsigned count = explicit_bindings (callee) + capture_bindings (callee);
      tree bindings = NULL_TREE;
      if (count)
        {
          bindings = e.temp (build_array_type_nelts (newtm_abi_clone_binding_type (), count), "newtm_call_bindings");
          TREE_ADDRESSABLE (bindings) = 1;
          unsigned slot = 0, index = 0;
          for (tree p = DECL_ARGUMENTS (callee); p; p = DECL_CHAIN (p), ++index)
            if (POINTER_TYPE_P (TREE_TYPE (p)))
              {
                tree element = build4 (ARRAY_REF, newtm_abi_clone_binding_type (), bindings,
                                        build_int_cst (size_type_node, slot++), NULL_TREE, NULL_TREE);
                e.fill (element, newtm_descriptor_find (stmt, NEWTM_DESC_ARGUMENT, index));
              }
          index = gimple_call_num_args (stmt);
          for (tree capture = newtm_clone_captures (callee); capture; capture = TREE_CHAIN (capture))
            for (unsigned part = 0; part < (integer_zerop (TREE_PURPOSE (capture)) ? 1u : 2u); ++part)
              {
                tree element = build4 (ARRAY_REF, newtm_abi_clone_binding_type (), bindings,
                                        build_int_cst (size_type_node, slot++), NULL_TREE, NULL_TREE);
                e.fill (element, newtm_descriptor_find (stmt, NEWTM_DESC_ARGUMENT, index++));
              }
          tree element = build4 (ARRAY_REF, newtm_abi_clone_binding_type (), bindings,
                                  size_zero_node, NULL_TREE, NULL_TREE);
          args.safe_push (build_fold_addr_expr (element));
        }
      else args.safe_push (build_zero_cst (TREE_TYPE (DECL_CHAIN (DECL_ARGUMENTS (clone)))));
      for (unsigned i = 0; i < gimple_call_num_args (stmt); ++i) args.safe_push (gimple_call_arg (stmt, i));
      tree result_type = TREE_TYPE (TREE_TYPE (callee));
      tree result = NULL_TREE;
      if (TREE_CODE (result_type) != VOID_TYPE)
        {
          result = e.temp (result_type, "newtm_call_result"); TREE_ADDRESSABLE (result) = 1;
          /* Filled only on OK, consumed only after check(status). Preserve
             that contract instead of inventing an initialization value. */
          suppress_warning (result, OPT_Wuninitialized);
          args.safe_push (build_fold_addr_expr (result));
        }
      tree result_binding = newtm_clone_result_binding (stmt);
      if (TREE_CODE (result_type) == POINTER_TYPE)
        {
          if (!result_binding)
            { result_binding = e.temp (newtm_abi_clone_binding_type (), "newtm_ignored_binding"); TREE_ADDRESSABLE (result_binding) = 1; }
          args.safe_push (build_fold_addr_expr (result_binding));
        }
      tree status = e.temp (uint32_type_node, "newtm_call_status");
      gcall *call = gimple_build_call_vec (clone, args);
      gimple_call_set_lhs (call, status); e.add (call); e.check (status);
      if (tree lhs = gimple_call_lhs (stmt))
        {
          gassign *store = gimple_build_assign (lhs, result);
          if (!is_gimple_reg (lhs)) newtm_occurrence_derive_store (stmt, store);
          e.add (store);
        }
    }
  else if (callee && fndecl_built_in_p (callee, BUILT_IN_NORMAL))
    {
      built_in_function operation;
      auto_vec<tree> args; args.safe_push (e.ctx);
      bool invoke = true;
      tree registration_done = NULL_TREE;
      tree registration_descriptor = NULL_TREE;
      switch (DECL_FUNCTION_CODE (callee))
        {
        case BUILT_IN_GOMP_TRANSACTION_BEGIN:
          operation = newtm_clone_region_owner (fn) && depth == 0
            ? BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS : BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN;
          ++depth; break;
        case BUILT_IN_GOMP_TRANSACTION_END:
          gcc_assert (depth);
          --depth;
          operation = newtm_clone_region_owner (fn) && depth == 0
            ? BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS : BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END;
          break;
        case BUILT_IN_GOMP_TRANSACTION_REGISTER:
          {
            operation = BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER;
            tree d = newtm_descriptor_find (stmt, NEWTM_DESC_REGISTER);
            registration_descriptor = d;
            if (!d || !TREE_VEC_ELT (d, NEWTM_D_VALID))
              { error_at (e.loc, "prepared function registration has no executable identity"); return; }
            e.guard (TREE_VEC_ELT (d, NEWTM_D_VALID));
            tree storage = TREE_VEC_ELT (d, NEWTM_D_STORAGE);
            e.guard (e.compare (LE_EXPR, storage,
                               build_int_cst (uint32_type_node, NEWTM_STORAGE_PRIVATE)));
            if (TREE_CODE (storage) == INTEGER_CST)
              invoke = tree_to_uhwi (storage) == NEWTM_STORAGE_REGISTERED;
            else
              {
                tree shared = create_artificial_label (e.loc);
                registration_done = create_artificial_label (e.loc);
                e.add (gimple_build_cond (EQ_EXPR, storage,
                      build_int_cst (uint32_type_node, NEWTM_STORAGE_PRIVATE), registration_done, shared));
                e.add (gimple_build_label (shared));
              }
            args.safe_push (TREE_VEC_ELT (d, NEWTM_D_OBJECT));
            args.safe_push (TREE_VEC_ELT (d, NEWTM_D_EXTENT));
            args.safe_push (TREE_VEC_ELT (d, NEWTM_D_KIND));
            break;
          }
        default: return; /* Compiler internal helpers remain ordinary IR. */
        }
      if (invoke)
        {
          tree status = e.runtime (operation, args);
          if (registration_descriptor)
            {
              tree d = registration_descriptor;
              newtm_memory_note (gimple_seq_last_stmt (e.seq), NEWTM_MEMORY_REGISTER,
                TREE_VEC_ELT (d, NEWTM_D_OBJECT), TREE_VEC_ELT (d, NEWTM_D_EXTENT),
                TREE_VEC_ELT (d, NEWTM_D_OFFSET), size_zero_node,
                TREE_VEC_ELT (d, NEWTM_D_KIND), TREE_VEC_ELT (d, NEWTM_D_STORAGE));
            }
          e.check (status);
        }
      if (registration_done) e.add (gimple_build_label (registration_done));
    }
  else
    {
      if (callee && newtm_occurrence_selected_p (callee))
        error_at (e.loc, "prepared call has no generated clone definition");
      return;
    }
  newtm_obligation_declare_all (stmt,
    callee == TREE_VEC_ELT (data, 5) ? NEWTM_OB_PUBLICATION
    : newtm_clone_for (callee) ? NEWTM_OB_PREPARED_CALL : NEWTM_OB_LIFECYCLE,
    newtm_clone_for (callee));
  newtm_obligation_emitted (e.seq, stmt);
  gsi_insert_seq_before_without_update (it, e.seq, GSI_SAME_STMT);
  newtm_occurrence_forget (stmt);
  if (newtm_clone_results) newtm_clone_results->remove (stmt);
  gsi_replace (it, gimple_build_nop (), false);
}
static bool tracing_cleanup (gimple_seq seq)
{
  if (!seq || gimple_seq_first_stmt (seq) != gimple_seq_last_stmt (seq)) return false;
  gimple *stmt = gimple_seq_first_stmt (seq);
  tree fn = is_gimple_call (stmt) ? gimple_call_fndecl (stmt) : NULL_TREE;
  return fn && fndecl_built_in_p (fn, BUILT_IN_GOMP_TRANSACTION_END);
}

/* Descriptors immediately preceding this statement use only evaluated input
   operands. The checked replacement defines its original result before any
   later descriptor can consume that result, including captured pointer loads.
   No descriptor recipe or source expression is evaluated a second time. */
static void transform_read (gimple_stmt_iterator *it, tree fn, gbind *scope)
{
  gimple *stmt = gsi_stmt (*it);
  tree binding = newtm_occurrence_binding (stmt);
  if (!binding || !(tree_to_uhwi (TREE_VEC_ELT (binding, 7)) & 1)) return;
  newtm_access access;
  if (!newtm_access_describe (stmt, NEWTM_DESC_READ, 0, &access))
    { error_at (gimple_location (stmt), "transaction read cannot be prepared: %s", access.reason); return; }
  emission e (fn, scope, gimple_location (stmt));
  e.access_address (access, "newtm_read_address");
  if (access.disposition == NEWTM_ACCESS_PRIVATE)
    {
      /* Proven attempt-private memory keeps its ordinary read. */
      e.note_access (stmt, NEWTM_MEMORY_PRIVATE_READ, access);
      newtm_obligation_declare (stmt, NEWTM_OB_READ, NEWTM_OB_PRIVATE);
      newtm_obligation_emitted (e.seq, stmt);
      gsi_insert_seq_before_without_update (it, e.seq, GSI_SAME_STMT);
      return;
    }
  tree result = e.temp (TYPE_MAIN_VARIANT (TREE_TYPE (access.reference)), "newtm_read_result");
  TREE_ADDRESSABLE (result) = 1;
  suppress_warning (result, OPT_Wuninitialized);
  tree done = NULL_TREE, shared = NULL_TREE;
  if (access.disposition == NEWTM_ACCESS_DYNAMIC)
    {
      e.guard (e.compare (LE_EXPR, access.storage,
                          build_int_cst (uint32_type_node, NEWTM_STORAGE_PRIVATE)));
      shared = create_artificial_label (e.loc);
      tree local = create_artificial_label (e.loc);
      done = create_artificial_label (e.loc);
      e.add (gimple_build_cond (EQ_EXPR, access.storage,
             build_int_cst (uint32_type_node, NEWTM_STORAGE_PRIVATE), local, shared));
      e.add (gimple_build_label (local));
      tree local_value = e.temp (TREE_TYPE (result), "newtm_private_read");
      e.add (gimple_build_assign (local_value, unshare_expr (access.reference)));
      e.note_access (gimple_seq_last_stmt (e.seq), NEWTM_MEMORY_PRIVATE_READ, access);
      e.add (gimple_build_assign (result, local_value));
      e.add (gimple_build_goto (done));
      e.add (gimple_build_label (shared));
    }
  auto_vec<tree> args;
  args.safe_push (e.ctx); args.safe_push (access.object); args.safe_push (access.offset);
  args.safe_push (e.value (build_fold_addr_expr (result))); args.safe_push (access.size);
  tree read_status = e.runtime (BUILT_IN_GOMP_TRANSACTION_STM_V1_READ, args);
  e.note_access (gimple_seq_last_stmt (e.seq), NEWTM_MEMORY_READ, access);
  e.check (read_status);
  if (done) e.add (gimple_build_label (done));
  /* Consume only after OK (or the proven-private branch). Keep any original
     destination store as a separate source operation for task 5.3. */
  tree checked = e.temp (TREE_TYPE (result), "newtm_checked_read");
  e.add (gimple_build_assign (checked, result));
  gassign *consume = gimple_build_assign (gimple_assign_lhs (stmt), checked);
  gimple_set_location (consume, e.loc);
  if (newtm_descriptor_find (stmt, NEWTM_DESC_WRITE))
    newtm_occurrence_derive_store (stmt, consume);
  newtm_obligation_declare (stmt, NEWTM_OB_READ,
    access.disposition == NEWTM_ACCESS_DYNAMIC ? NEWTM_OB_DYNAMIC : NEWTM_OB_STM);
  newtm_obligation_emitted (e.seq, stmt);
  gimple_seq consumed = NULL;
  gimple_seq_add_stmt_without_update (&consumed, consume);
  newtm_obligation_emitted (consumed, stmt);
  gsi_insert_seq_before_without_update (it, e.seq, GSI_SAME_STMT);
  newtm_occurrence_forget (stmt);
  gsi_replace (it, consume, false);
}

static void transform_sequence (gimple_seq *seq, tree fn, gbind *scope, unsigned &depth)
{
  for (gimple_stmt_iterator it = gsi_start (*seq); !gsi_end_p (it); gsi_next (&it))
    {
      gimple *stmt = gsi_stmt (it);
      if (gimple_code (stmt) == GIMPLE_BIND)
        transform_sequence (gimple_bind_body_ptr (as_a<gbind *> (stmt)), fn, as_a<gbind *> (stmt), depth);
      else if (gimple_code (stmt) == GIMPLE_TRY)
        {
          gtry *t = as_a<gtry *> (stmt);
          bool trace = tracing_cleanup (gimple_try_cleanup (t));
          transform_sequence (gimple_try_eval_ptr (t), fn, scope, depth);
          transform_sequence (gimple_try_cleanup_ptr (t), fn, scope, depth);
          if (trace)
            {
              gimple_seq flat = NULL;
              gimple_seq_add_seq (&flat, gimple_try_eval (t));
              gimple_seq_add_seq (&flat, gimple_try_cleanup (t));
              gimple_try_set_eval (t, NULL); gimple_try_set_cleanup (t, NULL);
              gsi_insert_seq_before_without_update (&it, flat, GSI_SAME_STMT);
              gsi_replace (&it, gimple_build_nop (), false);
            }
        }
      else if (is_gimple_omp (stmt) && gimple_has_substatements (stmt))
        {
          transform_sequence (gimple_omp_body_ptr (stmt), fn, scope, depth);
          if (gimple_code (stmt) == GIMPLE_OMP_TRANSACTION)
            {
              /* The prepared function is a status-returning frame. Its
                 generated failure returns must be outside structured OpenMP
                 markers before diagnose_omp_blocks. The original body still
                 undergoes the ordinary source structured-block diagnostics.
                 Preserve the nested BIND and its remapped temporaries. */
              gimple_seq flat = gimple_omp_body (stmt);
              newtm_obligation_declare (stmt, NEWTM_OB_CONTROL, NEWTM_OB_LIFECYCLE);
              newtm_obligation_derive_control (stmt, flat);
              newtm_obligation_forget (stmt);
              gimple_omp_set_body (stmt, NULL);
              gsi_insert_seq_before_without_update (&it, flat, GSI_SAME_STMT);
              gsi_replace (&it, gimple_build_nop (), false);
            }
        }
      else if (is_gimple_call (stmt)) transform_call (&it, fn, scope, depth);
      else if (is_gimple_assign (stmt))
        { transform_address (&it, fn, scope); transform_read (&it, fn, scope); }
      else if (gimple_code (stmt) == GIMPLE_RETURN)
        {
          emission e (fn, scope, gimple_location (stmt));
          e.check (gimple_return_retval (as_a<greturn *> (stmt)));
          newtm_obligation_declare_all (stmt, NEWTM_OB_PUBLICATION);
          newtm_obligation_emitted (e.seq, stmt);
          gsi_insert_seq_before_without_update (&it, e.seq, GSI_SAME_STMT);
        }
    }
}

/* A second walk consumes original writes AND stores derived from checked
   loads/calls. The descriptor window has not closed and no remapping or
   optimization has intervened. Compiler transport stores carry no source
   occurrence and are not program writes. */
static void transform_write (gimple_stmt_iterator *it, tree fn, gbind *scope)
{
  gimple *stmt = gsi_stmt (*it);
  tree binding = newtm_occurrence_binding (stmt);
  if (!binding || !(tree_to_uhwi (TREE_VEC_ELT (binding, 7)) & 2)) return;
  newtm_access access;
  if (!newtm_access_describe (stmt, NEWTM_DESC_WRITE, 0, &access))
    { error_at (gimple_location (stmt), "transaction write cannot be prepared: %s", access.reason); return; }
  emission e (fn, scope, gimple_location (stmt));
  e.access_address (access, "newtm_write_address");
  if (access.disposition == NEWTM_ACCESS_PRIVATE)
    {
      e.note_access (stmt, NEWTM_MEMORY_PRIVATE_WRITE, access);
      newtm_obligation_declare (stmt, NEWTM_OB_WRITE, NEWTM_OB_PRIVATE);
      newtm_obligation_emitted (e.seq, stmt);
      gsi_insert_seq_before_without_update (it, e.seq, GSI_SAME_STMT);
      return;
    }
  /* Materialize only the current GIMPLE RHS. Any preexisting source read
     has already been checked by the first walk. The stable typed buffer
     cannot alias the destination, incoming bindings or the runtime. */
  tree type = TYPE_MAIN_VARIANT (TREE_TYPE (access.reference));
  tree value = e.temp (type, "newtm_write_value");
  unsigned count = gimple_num_ops (stmt);
  e.add (gimple_build_assign (value, gimple_assign_rhs_code (stmt),
         unshare_expr (gimple_assign_rhs1 (stmt)),
         count > 2 ? unshare_expr (gimple_assign_rhs2 (stmt)) : NULL_TREE,
         count > 3 ? unshare_expr (gimple_assign_rhs3 (stmt)) : NULL_TREE));
  tree buffer = e.temp (type, "newtm_write_buffer");
  TREE_ADDRESSABLE (buffer) = 1;
  e.add (gimple_build_assign (buffer, value));
  tree done = NULL_TREE;
  if (access.disposition == NEWTM_ACCESS_DYNAMIC)
    {
      e.guard (e.compare (LE_EXPR, access.storage,
                          build_int_cst (uint32_type_node, NEWTM_STORAGE_PRIVATE)));
      tree local = create_artificial_label (e.loc), shared = create_artificial_label (e.loc);
      done = create_artificial_label (e.loc);
      e.add (gimple_build_cond (EQ_EXPR, access.storage,
             build_int_cst (uint32_type_node, NEWTM_STORAGE_PRIVATE), local, shared));
      e.add (gimple_build_label (local));
      e.add (gimple_build_assign (unshare_expr (access.reference), value));
      e.note_access (gimple_seq_last_stmt (e.seq), NEWTM_MEMORY_PRIVATE_WRITE, access);
      e.add (gimple_build_goto (done));
      e.add (gimple_build_label (shared));
    }
  auto_vec<tree> args;
  args.safe_push (e.ctx); args.safe_push (access.object); args.safe_push (access.offset);
  args.safe_push (e.value (build_fold_addr_expr (buffer))); args.safe_push (access.size);
  tree write_status = e.runtime (BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE, args);
  e.note_access (gimple_seq_last_stmt (e.seq), NEWTM_MEMORY_WRITE, access);
  e.check (write_status);
  if (done) e.add (gimple_build_label (done));
  newtm_obligation_declare (stmt, NEWTM_OB_WRITE,
    access.disposition == NEWTM_ACCESS_DYNAMIC ? NEWTM_OB_DYNAMIC : NEWTM_OB_STM);
  newtm_obligation_emitted (e.seq, stmt);
  gsi_insert_seq_before_without_update (it, e.seq, GSI_SAME_STMT);
  newtm_occurrence_forget (stmt);
  gsi_replace (it, gimple_build_nop (), false);
}
static void transform_writes (gimple_seq *seq, tree fn, gbind *scope)
{
  for (gimple_stmt_iterator it = gsi_start (*seq); !gsi_end_p (it); gsi_next (&it))
    {
      gimple *stmt = gsi_stmt (it);
      if (gimple_code (stmt) == GIMPLE_BIND)
        transform_writes (gimple_bind_body_ptr (as_a<gbind *> (stmt)), fn, as_a<gbind *> (stmt));
      else if (gimple_code (stmt) == GIMPLE_TRY)
        {
          transform_writes (gimple_try_eval_ptr (as_a<gtry *> (stmt)), fn, scope);
          transform_writes (gimple_try_cleanup_ptr (as_a<gtry *> (stmt)), fn, scope);
        }
      else if (is_gimple_omp (stmt) && gimple_has_substatements (stmt))
        transform_writes (gimple_omp_body_ptr (stmt), fn, scope);
      else if (is_gimple_assign (stmt)) transform_write (&it, fn, scope);
    }
}
} // namespace

void newtm_clone_finish (tree fn)
{
  tree data = info (fn);
  if (!data || seen_error ()) return;
  gimple_seq sequence = gimple_body (fn);
  gbind *scope = as_a<gbind *> (gimple_seq_first_stmt (sequence));
  emission failure (fn, scope, DECL_SOURCE_LOCATION (fn));
  TREE_VEC_ELT (data, 6) = failure.temp (uint32_type_node, "newtm_failure_status");
  TREE_VEC_ELT (data, 7) = create_artificial_label (failure.loc);
  const char *reason = NULL;
  if (!newtm_access_begin (fn))
    { internal_error ("transaction access preparation outside gimplification"); return; }
  if (!newtm_obligation_begin (fn, sequence,
      newtm_clone_region_owner (fn) ? NEWTM_OB_REGION : NEWTM_OB_CLONE))
    internal_error ("transaction obligation inventory already open");
  bool ready = newtm_access_preflight (fn, &reason);
  /* Refuse every incomplete prepared body before instrumenting any prefix. */
  if (!ready)
    {
      newtm_obligation_reject (reason);
      newtm_access_end ();
      newtm_region_rejections->put (fn, build_string (strlen (reason) + 1, reason));
      /* A refused body has no ordinary accesses or speculative effects.
         The future controller must diagnose refusal before activating it. */
      newtm_occurrence_retire (gimple_body (fn));
      newtm_descriptor_retire (gimple_body (fn));
      gimple_bind_set_body (scope, NULL);
      auto_vec<tree> args; args.safe_push (failure.ctx);
      args.safe_push (build_int_cst (uint32_type_node, NEWTM_ABI_INVALID));
      failure.add (gimple_build_return (failure.runtime (BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL, args)));
      gimple_bind_set_body (scope, failure.seq);
      newtm_obligation_emitted (failure.seq);
      const char *inspection_reason = NULL;
      if (!newtm_obligation_verify (gimple_body (fn), &inspection_reason))
        newtm_obligation_report (DECL_SOURCE_LOCATION (fn));
      newtm_obligation_end ();
      return;
    }
  unsigned depth = 0;
  transform_sequence (&sequence, fn, scope, depth);
  transform_writes (&sequence, fn, scope);
  newtm_access_end ();
  gcc_assert (!depth);
  failure.add (gimple_build_label (TREE_VEC_ELT (data, 7)));
  auto_vec<tree> args; args.safe_push (failure.ctx); args.safe_push (TREE_VEC_ELT (data, 6));
  failure.add (gimple_build_return (failure.runtime (BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL, args)));
  newtm_obligation_emitted (failure.seq);
  gimple_seq_add_seq (gimple_bind_body_ptr (scope), failure.seq);
  emission e (fn, scope, DECL_SOURCE_LOCATION (fn));
  e.status ();
  /* These are trusted compiler transport pointers, with caller-owned live,
     nonoverlapping storage. Nulls are permanent errors, not common memory. */
  unsigned count = explicit_bindings (newtm_clone_original (fn)) + capture_bindings (fn);
  for (unsigned slot = count ? 2 : 3; slot <= 4; ++slot)
    if (tree pointer = TREE_VEC_ELT (data, slot)) e.guard (pointer);
  auto validate = [&] (tree transport, tree argument)
    {
        tree binding = newtm_clone_identity (transport);
        tree object = e.value (newtm_clone_field (binding, 0));
        tree extent = e.value (newtm_clone_field (binding, 1));
        tree offset = e.value (newtm_clone_field (binding, 2));
        tree kind = e.value (newtm_clone_field (binding, 3));
        tree storage = e.value (newtm_clone_storage (transport));
        e.guard (object);
        e.guard (extent);
        e.guard (e.compare (LE_EXPR, offset, extent));
        e.guard (e.compare (LE_EXPR, kind,
                           build_int_cst (uint32_type_node, NEWTM_ABI_AGGREGATE)));
        e.guard (e.compare (LE_EXPR, storage,
                           build_int_cst (uint32_type_node, NEWTM_STORAGE_PRIVATE)));
        /* Validate correspondence using integer arithmetic, without forming a
           potentially out-of-range subobject pointer. Both operands are the
           actual values passed at this activation, never source recipes. */
        tree ptruint = unsigned_type_for (pointer_sized_int_node);
        tree integer = build_nonstandard_integer_type
          (MAX (TYPE_PRECISION (size_type_node), TYPE_PRECISION (ptruint)), true);
        tree address = e.cast (integer, e.cast (ptruint, object));
        tree maximum = fold_convert (integer, TYPE_MAX_VALUE (ptruint));
        tree remaining = e.temp (integer, "newtm_address_remaining");
        e.math (gimple_build_assign (remaining, MINUS_EXPR, maximum, address));
        tree displacement = e.cast (integer, offset);
        e.guard (e.compare (LE_EXPR, displacement, remaining));
        tree logical = e.temp (integer, "newtm_logical_address");
        e.math (gimple_build_assign (logical, PLUS_EXPR, address, displacement));
        if (argument)
          e.guard (e.compare (EQ_EXPR, logical, e.cast (integer, e.cast (ptruint, argument))));
        tree span = e.temp (size_type_node, "newtm_extent_span");
        e.math (gimple_build_assign (span, MINUS_EXPR, extent,
                                    build_int_cst (size_type_node, 1)));
        e.guard (e.compare (LE_EXPR, e.cast (integer, span), remaining));
    };
  for (tree p = DECL_ARGUMENTS (fn); p; p = DECL_CHAIN (p))
    if (tree transport = newtm_clone_parameter_binding (fn, p)) validate (transport, p);
  unsigned slot = explicit_bindings (newtm_clone_original (fn));
  for (tree capture = newtm_clone_captures (fn); capture; capture = TREE_CHAIN (capture))
    for (unsigned part = 0; part < (integer_zerop (TREE_PURPOSE (capture)) ? 1u : 2u); ++part)
      validate (binding_at (data, slot++), NULL_TREE);
  gimple_stmt_iterator first = gsi_start (*gimple_bind_body_ptr (scope));
  newtm_obligation_emitted (e.seq);
  gsi_insert_seq_before_without_update (&first, e.seq, GSI_SAME_STMT);
  const char *inspection_reason = NULL;
  gimple *inspection_bad = NULL;
  if (!newtm_obligation_verify (sequence, &inspection_reason, &inspection_bad))
    {
      newtm_obligation_reject (inspection_reason);
      newtm_region_rejections->put (fn, build_string (strlen (inspection_reason) + 1, inspection_reason));
      newtm_obligation_report (DECL_SOURCE_LOCATION (fn));
    }
  if (!seen_error ())
    {
      record_memory_origins (fn, sequence);
      newtm_memory_snapshot (fn, sequence);
    }
  newtm_memory_retire ();
  newtm_obligation_end ();
  gimple_set_body (fn, sequence);
}
namespace {
/* Only this frame owns SLOT. The helper borrows its value for one complete
   attempt; no source body, registration or automatic local is hoisted out. */
struct controller : emission
{
  tree status, failed;
  controller (tree fn, gbind *scope, location_t loc)
    : emission (fn, scope, loc)
  {
    status = temp (uint32_type_node, "newtm_control_status");
    failed = create_artificial_label (loc);
  }
  void check ()
  {
    tree next = create_artificial_label (loc);
    add (gimple_build_cond (EQ_EXPR, status, build_zero_cst (uint32_type_node), next, failed));
    add (gimple_build_label (next));
  }
  void guard (tree valid)
  {
    tree next = create_artificial_label (loc), invalid = create_artificial_label (loc);
    gcond *condition = gimple_build_cond (NE_EXPR, valid, build_zero_cst (TREE_TYPE (valid)), next, invalid);
    add (condition); newtm_obligation_abort (condition, invalid);
    add (gimple_build_label (invalid));
    add (gimple_build_assign (status, build_int_cst (uint32_type_node, NEWTM_ABI_INVALID)));
    add (gimple_build_goto (failed));
    add (gimple_build_label (next));
  }
  void service (built_in_function operation, auto_vec<tree> &args)
  { add (newtm_abi_build_call (operation, status, args.length (), args.address ())); }
};

static void control_sequence (gimple_seq *sequence, tree fn)
{
  for (gimple_stmt_iterator it = gsi_start (*sequence); !gsi_end_p (it); gsi_next (&it))
    {
      gimple *stmt = gsi_stmt (it);
      if (gimple_code (stmt) == GIMPLE_OMP_TRANSACTION)
        {
          tree helper = newtm_clone_region_body (stmt);
          if (!helper)
            { error_at (gimple_location (stmt), "transaction has no prepared external body"); continue; }
          if (!newtm_obligation_begin (fn, gimple_omp_body (stmt), NEWTM_OB_CONTROLLER, stmt))
            internal_error ("transaction obligation inventory already open");
          gbind *scope = gimple_build_bind (NULL_TREE, NULL, NULL_TREE);
          controller e (fn, scope, gimple_location (stmt));
          tree slot = e.temp (newtm_abi_context_type (), "newtm_owner_slot");
          tree outer = e.temp (uint32_type_node, "newtm_outermost");
          TREE_ADDRESSABLE (slot) = TREE_ADDRESSABLE (outer) = 1;
          tree conflicts = e.temp (size_type_node, "newtm_conflicts");
          tree retry = create_artificial_label (e.loc), done = create_artificial_label (e.loc);
          tree fatal = create_artificial_label (e.loc), cleanup = create_artificial_label (e.loc);
          tree saturate = create_artificial_label (e.loc), increment = create_artificial_label (e.loc);
          e.add (gimple_build_assign (slot, build_clobber (TREE_TYPE (slot), CLOBBER_STORAGE_BEGIN)));
          e.add (gimple_build_assign (slot, build_zero_cst (TREE_TYPE (slot))));
          e.add (gimple_build_assign (conflicts, build_zero_cst (size_type_node)));
          e.add (gimple_build_label (retry));
          auto_vec<tree> args;
          args.safe_push (build_fold_addr_expr (slot));
          args.safe_push (build_fold_addr_expr (outer));
          e.service (BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN, args); e.check ();
          if (gbind *prefix = newtm_descriptor_take_prefix (stmt))
            {
              tree vars = gimple_bind_vars (prefix);
              gimple_bind_set_vars (scope, chainon (vars, gimple_bind_vars (scope)));
              gimple_seq_add_seq (&e.seq, gimple_bind_body (prefix));
              gimple_bind_set_body (prefix, NULL); gimple_bind_set_vars (prefix, NULL_TREE);
            }
          e.guard (e.compare (EQ_EXPR, e.value (outer), build_int_cst (uint32_type_node, 1)));
          tree ctx = e.value (slot);
          args.truncate (0); args.safe_push (ctx);
          unsigned count = capture_bindings (helper), index = 0;
          tree bindings = NULL_TREE;
          if (count)
            {
              bindings = e.temp (build_array_type_nelts (newtm_abi_clone_binding_type (), count), "newtm_entry_bindings");
              TREE_ADDRESSABLE (bindings) = 1;
              for (; index < count; ++index)
                {
                  tree d = newtm_descriptor_find (stmt, NEWTM_DESC_ARGUMENT, index);
                  const unsigned fields[] = { NEWTM_D_OBJECT, NEWTM_D_EXTENT, NEWTM_D_OFFSET,
                                              NEWTM_D_KIND, NEWTM_D_VALID, NEWTM_D_STORAGE };
                  bool executable = d && tree_to_uhwi (TREE_VEC_ELT (d, NEWTM_D_STATE)) <= NEWTM_DESC_RESULT;
                  for (unsigned field : fields) executable &= d && TREE_VEC_ELT (d, field);
                  if (!executable)
                    { error_at (e.loc, "transaction capture has no executable entry identity"); break; }
                  e.guard (TREE_VEC_ELT (d, NEWTM_D_VALID));
                  tree element = build4 (ARRAY_REF, newtm_abi_clone_binding_type (), bindings,
                                          build_int_cst (size_type_node, index), NULL_TREE, NULL_TREE);
                  tree identity = newtm_clone_identity (element);
                  for (unsigned field = 0; field < 4; ++field)
                    e.add (gimple_build_assign (newtm_clone_field (identity, field), TREE_VEC_ELT (d, fields[field])));
                  e.add (gimple_build_assign (newtm_clone_storage (element), TREE_VEC_ELT (d, NEWTM_D_STORAGE)));
                }
              tree first = build4 (ARRAY_REF, newtm_abi_clone_binding_type (), bindings,
                                   size_zero_node, NULL_TREE, NULL_TREE);
              args.safe_push (build_fold_addr_expr (first));
            }
          else args.safe_push (build_zero_cst (TREE_TYPE (DECL_CHAIN (DECL_ARGUMENTS (helper)))));
          index = 0;
          for (tree p = newtm_clone_captures (helper); p; p = TREE_CHAIN (p))
            {
              tree d = newtm_descriptor_find (stmt, NEWTM_DESC_ARGUMENT, index);
              if (d && TREE_VEC_ELT (d, NEWTM_D_LOGICAL))
                args.safe_push (e.cast (build_pointer_type (TREE_TYPE (TREE_VALUE (p))),
                                       TREE_VEC_ELT (d, NEWTM_D_LOGICAL)));
              else args.safe_push (build_fold_addr_expr (TREE_VALUE (p)));
              index += integer_zerop (TREE_PURPOSE (p)) ? 1 : 2;
            }
          record_capture_origins (helper, e.seq, args);
          gcall *call = gimple_build_call_vec (helper, args);
          gimple_call_set_lhs (call, e.status); e.add (call); e.check ();
          args.truncate (0); args.safe_push (ctx);
          e.service (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS, args); e.check ();
          args.truncate (0); args.safe_push (build_fold_addr_expr (slot));
          e.service (BUILT_IN_GOMP_TRANSACTION_STM_V1_END, args); e.check ();
          e.add (gimple_build_goto (done));
          e.add (gimple_build_label (e.failed));
          e.add (gimple_build_cond (NE_EXPR, e.value (slot), build_zero_cst (TREE_TYPE (slot)), cleanup, fatal));
          e.add (gimple_build_label (cleanup));
          args.truncate (0); args.safe_push (e.value (slot)); args.safe_push (e.status);
          e.service (BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL, args);
          e.add (gimple_build_cond (LT_EXPR, conflicts, TYPE_MAX_VALUE (size_type_node), increment, saturate));
          e.add (gimple_build_label (increment));
          e.add (gimple_build_assign (conflicts, PLUS_EXPR, conflicts, build_int_cst (size_type_node, 1)));
          e.add (gimple_build_label (saturate));
          args.truncate (0); args.safe_push (build_fold_addr_expr (slot)); args.safe_push (conflicts);
          e.service (BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE, args);
          tree empty = create_artificial_label (e.loc);
          e.add (gimple_build_cond (EQ_EXPR, e.status, build_int_cst (uint32_type_node, NEWTM_ABI_CONFLICT), empty, fatal));
          e.add (gimple_build_label (empty));
          e.add (gimple_build_cond (EQ_EXPR, e.value (slot), build_zero_cst (TREE_TYPE (slot)), retry, fatal));
          e.add (gimple_build_label (fatal));
          args.truncate (0); args.safe_push (e.status);
          e.add (newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL, NULL_TREE,
                                      args.length (), args.address ()));
          e.add (gimple_build_label (done));
          /* An actual lifetime boundary, reconstructed after outlining.  The
             owner proof must establish commit before this storage dies. */
          e.add (gimple_build_assign (slot, build_clobber (TREE_TYPE (slot), CLOBBER_STORAGE_END)));
          gimple_bind_set_body (scope, e.seq);
          newtm_obligation_declare_all (stmt, NEWTM_OB_DELEGATED, helper);
          struct delegation { static tree mark (gimple_stmt_iterator *p, bool *, walk_stmt_info *wi)
            { newtm_obligation_declare_all (gsi_stmt (*p), NEWTM_OB_DELEGATED, (tree) wi->info); return NULL_TREE; } };
          walk_stmt_info wi = {}; wi.info = helper;
          walk_gimple_seq (gimple_omp_body (stmt), delegation::mark, NULL, &wi);
          newtm_obligation_emitted (e.seq, stmt);
          tree activated = tree_cons (NULL_TREE, helper, newtm_activated_regions);
          newtm_activated_regions = activated;
          newtm_occurrence_retire (gimple_omp_body (stmt));
          newtm_descriptor_retire (gimple_omp_body (stmt));
          newtm_occurrence_forget (stmt);
          newtm_descriptor_forget (stmt);
          const char *inspection_reason = NULL;
          gimple *inspection_bad = NULL;
          if (!newtm_obligation_verify (e.seq, &inspection_reason, &inspection_bad,
                                        gimple_bind_vars (scope)))
            {
              newtm_obligation_reject (inspection_reason);
              newtm_obligation_report (e.loc);
            }
          else TREE_PURPOSE (activated) = build_int_cst (long_long_unsigned_type_node,
                                                         newtm_obligation_local_count ());
          if (!seen_error ()) newtm_channels_snapshot_entry (fn, e.seq, call);
          if (!seen_error ()) newtm_owner_snapshot_entry (fn, e.seq, call,
                                                         gimple_bind_vars (scope));
          newtm_obligation_end ();
          gsi_replace (&it, scope, false);
        }
      else if (gimple_code (stmt) == GIMPLE_BIND)
        control_sequence (gimple_bind_body_ptr (as_a<gbind *> (stmt)), fn);
      else if (gimple_code (stmt) == GIMPLE_TRY)
        {
          control_sequence (gimple_try_eval_ptr (as_a<gtry *> (stmt)), fn);
          control_sequence (gimple_try_cleanup_ptr (as_a<gtry *> (stmt)), fn);
        }
      else if (is_gimple_omp (stmt) && gimple_has_substatements (stmt))
        control_sequence (gimple_omp_body_ptr (stmt), fn);
    }
}
} // namespace

void newtm_clone_control (tree fn)
{
  if (!flag_openmp_transaction_stm || !flag_openmp || info (fn) || seen_error ()) return;
  gimple_seq body = gimple_body (fn);
  control_sequence (&body, fn);
  gimple_set_body (fn, body);
}

namespace {
static tree current_frame (tree fn, unsigned mask, unsigned dependencies, tree stamp)
{
  tree f = make_tree_vec (4); TREE_VEC_ELT (f, 0) = fn;
  TREE_VEC_ELT (f, 1) = build_int_cst (unsigned_type_node, mask);
  TREE_VEC_ELT (f, 2) = build_int_cst (unsigned_type_node, dependencies);
  TREE_VEC_ELT (f, 3) = stamp; return f;
}
static bool current_body (tree fn, tree *frames)
{
  tree before = newtm_current_ir (fn);
  if (!before)
    { error_at (newtm_current_diagnostic_location (fn, NULL), "transaction current body cannot be snapshotted");
      newtm_current_diagnostic_context (fn, NULL, "V08", "current body proof"); return false; }
  tree expected = NULL_TREE;
  if (newtm_call_dependencies)
    if (tree *p = newtm_call_dependencies->get (fn)) expected = *p;
  if (!check_prepared_signature (fn) || !check_remapped_operands (fn)
      || !check_lowered_calls (fn, expected) || !check_lowered_context (fn)
      || !check_lowered_registrations (fn) || !check_lowered_memory (fn)
      || !newtm_memory_verify_cfg (fn) || !newtm_channels_verify_cfg (fn)
      || !newtm_status_verify_cfg (fn) || !newtm_coverage_verify_cfg (fn)) return false;
  unsigned dependencies = 0;
  for (unsigned i = 0; i < 4; ++i)
    {
      newtm_memory_view v;
      bool present = i == 0 ? newtm_memory_get (fn, &v) : i == 1 ? newtm_channels_get (fn, &v)
        : i == 2 ? newtm_status_get (fn, &v) : newtm_coverage_get (fn, &v);
      if (!present || v.state != NEWTM_MEMORY_DEPENDENT)
        {
          static const char *const components[] = { "memory", "channels", "status", "coverage" };
          error_at (newtm_current_diagnostic_location (fn, NULL), "transaction current body lacks a component result");
          newtm_current_diagnostic_context (fn, NULL, "V08", "component result");
          inform (DECL_SOURCE_LOCATION (fn), "required transaction component %s is unavailable", components[i]);
          return false;
        }
      dependencies |= v.dependencies;
    }
  tree after = newtm_current_ir (fn);
  if (!after || TREE_STRING_LENGTH (before) != TREE_STRING_LENGTH (after)
      || memcmp (TREE_STRING_POINTER (before), TREE_STRING_POINTER (after), TREE_STRING_LENGTH (before)))
    { error_at (newtm_current_diagnostic_location (fn, NULL), "transaction body changed during current verification");
      newtm_current_diagnostic_context (fn, NULL, "V08", "current body proof"); return false; }
  *frames = tree_cons (NULL_TREE, current_frame (fn, NEWTM_CURRENT_ALL & ~NEWTM_CURRENT_OWNER, dependencies, after), *frames);
  return true;
}
}

bool newtm_clone_verify_current (tree root, tree *frames, tree *controller)
{
  if (!root || !frames || !controller) return false;
  *frames = *controller = NULL_TREE;
  tree owner = newtm_clone_region_owner (root);
  if (!owner)
    { error_at (newtm_current_diagnostic_location (root, NULL), "transaction current graph has no source controller");
      newtm_current_diagnostic_context (root, NULL, "V07", "source controller ownership"); return false; }
  tree staged = NULL_TREE;
  hash_set<tree> checked;
  auto_vec<tree> work; work.safe_push (root);
  for (unsigned at = 0; at < work.length (); ++at)
    {
      tree fn = work[at]; if (checked.add (fn)) continue;
      if (!newtm_access_prepared_p (fn) || newtm_clone_region_rejection (fn))
        {
          error_at (newtm_current_diagnostic_location (fn, NULL), "transaction current graph contains an unprepared member");
          newtm_current_diagnostic_context (fn, NULL, "V06", "prepared graph member");
          if (const char *reason = newtm_clone_region_rejection (fn))
            inform (DECL_SOURCE_LOCATION (fn), "prepared member rejection: %s", reason);
          return false;
        }
      if (!current_body (fn, &staged)) return false;
      if (newtm_call_dependencies)
        if (tree *p = newtm_call_dependencies->get (fn))
          for (tree q = *p; q; q = TREE_CHAIN (q)) work.safe_push (TREE_VALUE (q));
    }
  tree actual = NULL_TREE, stamp = NULL_TREE;
  if (!check_lowered_controller (root, owner, &actual, &stamp)) return false;
  newtm_memory_view own, cover;
  if (!newtm_owner_get (root, &own) || own.state != NEWTM_MEMORY_DEPENDENT)
    {
      error_at (newtm_current_diagnostic_location (root, NULL), "transaction current controller lacks its component results: OWNER unavailable");
      newtm_current_diagnostic_context (root, NULL, "V08", "controller OWNER result");
      return false;
    }
  if (!newtm_coverage_entry_get (root, &cover) || cover.state != NEWTM_MEMORY_DEPENDENT)
    {
      error_at (newtm_current_diagnostic_location (root, NULL), "transaction current controller lacks its component results: entry COVERAGE unavailable");
      newtm_current_diagnostic_context (root, NULL, "V08", "controller entry COVERAGE result");
      return false;
    }
  staged = tree_cons (NULL_TREE, current_frame (actual, NEWTM_CURRENT_ALL, own.dependencies | cover.dependencies, stamp), staged);
  checked.add (actual);
  /* Receiver producer frames belong to the proof too, even though their
     ordinary source code is not instrumented as a prepared body. */
  tree parent = actual;
  hash_set<tree> ancestors;
  while (parent && !ancestors.add (parent))
    {
      tree *p = newtm_outline_parents ? newtm_outline_parents->get (parent) : NULL;
      if (!p) break;
      parent = *p;
      if (!parent || parent == error_mark_node) return false;
      if (!checked.add (parent))
        {
          tree image = newtm_current_ir (parent);
          if (!image)
            { error_at (newtm_current_diagnostic_location (parent, NULL), "transaction current graph lost an outlining producer");
              newtm_current_diagnostic_context (parent, NULL, "V08", "current graph proof"); return false; }
          staged = tree_cons (NULL_TREE, current_frame (parent, 0, 0, image), staged);
        }
    }
  *frames = staged; *controller = actual; return true;
}

void newtm_clone_certify_regions ()
{
  for (tree p = newtm_activated_regions; p; p = TREE_CHAIN (p))
    {
      tree helper = TREE_VALUE (p), id = TREE_PURPOSE (p);
      /* Preserve the original preflight refusal before diagnosing a dependent
         controller; its cause is more specific than the propagated marker. */
      if (!newtm_clone_require_region (helper)) continue;
      newtm_local_view controller = {};
      bool valid = id && tree_fits_uhwi_p (id)
        && newtm_obligation_local_get (tree_to_uhwi (id), &controller)
        && controller.scope_kind == NEWTM_OB_CONTROLLER && controller.closed
        && controller.state == NEWTM_LOCAL_APPROVED && !controller.graph_invalid
        && controller.required == controller.proved;
      bool dependency = false;
      if (valid)
        for (tree q = controller.dependencies; q; q = TREE_CHAIN (q)) dependency |= TREE_VALUE (q) == helper;
      if (!valid || !dependency)
        { error_at (DECL_SOURCE_LOCATION (helper), "transaction graph lacks its verified controller dependency"); continue; }
      {
        hash_set<tree> visited;
        tree revisions = NULL_TREE;
        if (check_local_graph (helper, visited, &revisions)
            && newtm_current_verify (helper)
            && !newtm_obligation_graph_commit (helper, tree_to_uhwi (id), revisions))
          error_at (DECL_SOURCE_LOCATION (helper), "transaction graph changed before local certification");
      }
    }
}


/* Persistent contracts only: reconstructed proofs are never serialized.
   newtm_region_sources is an operational GENERIC-node association, retired
   during preparation; it must never enter a late proof or an LTO stream. */
tree newtm_clone_export ()
{
  tree data = make_tree_vec (17);
  TREE_VEC_ELT (data, 0) = newtm_lto_pack (newtm_clones);
  TREE_VEC_ELT (data, 1) = newtm_lto_pack (newtm_clone_info);
  TREE_VEC_ELT (data, 2) = newtm_lto_pack (newtm_capture_lists);
  TREE_VEC_ELT (data, 3) = newtm_lto_pack (newtm_lexical_decls);
  TREE_VEC_ELT (data, 4) = newtm_lto_pack (newtm_region_owners);
  TREE_VEC_ELT (data, 5) = newtm_lto_pack (newtm_region_parameters);
  TREE_VEC_ELT (data, 6) = newtm_lto_pack (newtm_call_dependencies);
  TREE_VEC_ELT (data, 7) = newtm_lto_pack (newtm_outline_parents);
  TREE_VEC_ELT (data, 8) = newtm_lto_pack (newtm_prepared_prototypes);
  TREE_VEC_ELT (data, 9) = newtm_lto_pack (newtm_prepared_parameter_origins);
  TREE_VEC_ELT (data, 10) = newtm_lto_pack (newtm_remap_decls);
  TREE_VEC_ELT (data, 11) = newtm_lto_pack (newtm_remap_fields);
  TREE_VEC_ELT (data, 12) = newtm_lto_pack (newtm_native_frames);
  TREE_VEC_ELT (data, 13) = newtm_lto_pack (newtm_capture_origins);
  TREE_VEC_ELT (data, 14) = newtm_lto_pack (newtm_registration_origins);
  TREE_VEC_ELT (data, 15) = newtm_lto_pack (newtm_memory_origins);
  TREE_VEC_ELT (data, 16) = newtm_lto_pack (newtm_offset_arithmetic);
  return data;
}
void newtm_clone_import (tree data)
{
  gcc_assert (data && TREE_CODE (data) == TREE_VEC && TREE_VEC_LENGTH (data) == 17);
  newtm_lto_unpack (newtm_clones, TREE_VEC_ELT (data, 0));
  newtm_lto_unpack (newtm_clone_info, TREE_VEC_ELT (data, 1));
  newtm_lto_unpack (newtm_capture_lists, TREE_VEC_ELT (data, 2));
  newtm_lto_unpack (newtm_lexical_decls, TREE_VEC_ELT (data, 3));
  newtm_lto_unpack (newtm_region_owners, TREE_VEC_ELT (data, 4));
  newtm_lto_unpack (newtm_region_parameters, TREE_VEC_ELT (data, 5));
  newtm_lto_unpack (newtm_call_dependencies, TREE_VEC_ELT (data, 6));
  newtm_lto_unpack (newtm_outline_parents, TREE_VEC_ELT (data, 7));
  newtm_lto_unpack (newtm_prepared_prototypes, TREE_VEC_ELT (data, 8));
  newtm_lto_unpack (newtm_prepared_parameter_origins, TREE_VEC_ELT (data, 9));
  newtm_lto_unpack (newtm_remap_decls, TREE_VEC_ELT (data, 10));
  newtm_lto_unpack (newtm_remap_fields, TREE_VEC_ELT (data, 11));
  newtm_lto_unpack (newtm_native_frames, TREE_VEC_ELT (data, 12));
  newtm_lto_unpack (newtm_capture_origins, TREE_VEC_ELT (data, 13));
  newtm_lto_unpack (newtm_registration_origins, TREE_VEC_ELT (data, 14));
  newtm_lto_unpack (newtm_memory_origins, TREE_VEC_ELT (data, 15));
  newtm_lto_unpack (newtm_offset_arithmetic, TREE_VEC_ELT (data, 16));
}

#include "gt-newtm-clone.h"
