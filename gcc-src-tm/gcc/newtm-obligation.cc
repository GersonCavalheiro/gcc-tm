/* An inventory is evidence about work, never permission to skip verification.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-ssa.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "gimplify.h"
#include "function.h"
#include "stringpool.h"
#include "fold-const.h"
#include "tree-dfa.h"
#include "internal-fn.h"
#include "newtm-abi.h"
#include "gimple-pretty-print.h"
#include "options.h"
#include "diagnostic-core.h"
#include "newtm-obligation.h"
#include "newtm-occurrence.h"
#include "newtm-descriptor.h"
#include "newtm-clone.h"
#include "newtm-current.h"
#include "newtm-flow.h"
#include "newtm-guard.h"
#include "newtm-status.h"
#include "newtm-lifetime.h"
#include "newtm-failure.h"
#include "newtm-owner.h"

/* Scalar snapshots in GC-owned vectors. Only LIVE holds instruction anchors;
   it is released on close, before tree-nested/omplower can remap operands. */
enum field
{
  OWNER, INSTANCE, SCOPE, GENERATION, OPERATION, ORIGIN, GROUP, COPY_OPERATION,
  COPY_FROM, PARENT, PRODUCER, CALLEE, DECLARED_CALLEE, REASON, LOCATION, KIND, SCOPE_KIND,
  STATE, EVIDENCE, TREATMENT, ROLE, OPERAND, DESC_STATE, DESC_FIELDS,
  SOURCE, WITNESS, STORAGE, CHILDREN, REQUIRED, PROVED, LOCAL_STATE,
  LOCAL_RESULT, DEPENDENCIES, EXPECTED_SERVICE, FIELDS
};
enum result_field
{
  R_OWNER, R_SCOPE, R_GENERATION, R_SCOPE_KIND, R_STATE, R_REQUIRED, R_PROVED,
  R_DEPENDENCIES, R_OBLIGATION, R_ANCESTOR, R_RULE, R_LOCATION, R_KIND,
  R_REASON, R_CLOSED, R_GRAPH_INVALID, R_GRAPH_DIRTY, R_REMAP_BOUNDARIES, RESULT_FIELDS
};
static GTY(()) vec<tree, va_gc> *records;
static GTY(()) vec<tree, va_gc> *local_results;
/* TREE_VEC(root, controller revision, revision list, valid, edge snapshots,
   component assignments, signature snapshots).
   Only declarations and scalar IDs survive, never instructions/descriptors. */
static GTY(()) vec<tree, va_gc> *graph_results;
static GTY(()) hash_map<gimple *, tree> *live;
static GTY(()) hash_map<tree, tree> *operations;
static GTY(()) tree live_owner;
static GTY(()) tree live_reason;
static GTY(()) tree local_refusal;
/* Operational evidence is deliberately separate from durable ledger facts.
   Keep detached source snapshots only until the final local inspection. */
static GTY(()) hash_map<gimple *, tree> *shapes;
static GTY(()) hash_map<tree, tree> *identities;
static GTY(()) hash_map<tree, tree> *references;
static GTY(()) hash_map<tree, tree> *call_inputs;
static GTY(()) hash_map<tree, tree> *inspection_counts;
static GTY(()) hash_map<tree, tree> *flow_node_counts;
static GTY(()) hash_map<tree, tree> *private_roots;
static GTY(()) hash_map<gimple *, tree> *math_obligations;
static GTY(()) hash_map<gimple *, tree> *emission_sources;
static GTY(()) hash_map<gimple *, tree> *abort_edges;
static unsigned HOST_WIDE_INT next_scope, live_scope, live_generation;
static newtm_obligation_scope live_scope_kind;
static unsigned inventory_depth;
static unsigned HOST_WIDE_INT local_failure_result;

static tree number (unsigned HOST_WIDE_INT n)
{ return build_int_cst (long_long_unsigned_type_node, n); }
static unsigned HOST_WIDE_INT value (tree r, field f)
{ return TREE_VEC_ELT (r, f) ? tree_to_uhwi (TREE_VEC_ELT (r, f)) : 0; }
static void set (tree r, field f, unsigned HOST_WIDE_INT n)
{ TREE_VEC_ELT (r, f) = number (n); }
static tree record_at (unsigned HOST_WIDE_INT id)
{ return id && id <= vec_safe_length (records) ? (*records)[id - 1] : NULL_TREE; }
static unsigned HOST_WIDE_INT result_value (tree r, result_field f)
{ return TREE_VEC_ELT (r, f) ? tree_to_uhwi (TREE_VEC_ELT (r, f)) : 0; }
static void result_set (tree r, result_field f, unsigned HOST_WIDE_INT n)
{ TREE_VEC_ELT (r, f) = number (n); }
static tree result_at (unsigned HOST_WIDE_INT id)
{ return id && id <= vec_safe_length (local_results) ? (*local_results)[id - 1] : NULL_TREE; }

static void invalidate_graph_results ()
{
  for (unsigned i = 0; i < vec_safe_length (graph_results); ++i)
    {
      tree graph = (*graph_results)[i];
      for (tree p = TREE_VEC_ELT (graph, 2); p; p = TREE_CHAIN (p))
        {
          tree r = result_at (tree_to_uhwi (TREE_VALUE (p)));
          if (!r || result_value (r, R_STATE) != NEWTM_LOCAL_APPROVED
              || result_value (r, R_GRAPH_INVALID))
            { TREE_VEC_ELT (graph, 3) = boolean_false_node; break; }
        }
    }
}

/* Invalidate graph dependencies independently of local proof status. A fixed
   point handles recursion without inheriting any approval for copied IR. */
static void invalidate_dependents (tree owner)
{
  if (!owner) return;
  newtm_current_invalidate (owner);
  auto_vec<tree> work;
  hash_set<tree> seen;
  work.safe_push (owner);
  while (!work.is_empty ())
    {
      tree changed = work.pop ();
      if (seen.add (changed)) continue;
      for (unsigned i = 0; i < vec_safe_length (local_results); ++i)
        {
          tree r = (*local_results)[i];
          if (result_value (r, R_SCOPE_KIND) == NEWTM_OB_COPY) continue;
          for (tree p = TREE_VEC_ELT (r, R_DEPENDENCIES); p; p = TREE_CHAIN (p))
            if (TREE_VALUE (p) == changed)
              {
                result_set (r, R_GRAPH_INVALID, 1);
                tree caller = TREE_VEC_ELT (r, R_OWNER);
                if (caller) work.safe_push (caller);
                break;
              }
        }
    }
}
/* Record a crossed IR boundary independently of the historical graph.  The
   closure includes callers and recursion, but never approves a new body. */
void newtm_obligation_remap_boundary (tree owner, unsigned boundary)
{
  if (!owner || !boundary) return;
  newtm_current_invalidate (owner);
  auto_vec<tree> work;
  hash_set<tree> seen;
  work.safe_push (owner);
  while (!work.is_empty ())
    {
      tree changed = work.pop ();
      if (seen.add (changed)) continue;
      for (unsigned i = 0; i < vec_safe_length (local_results); ++i)
        {
          tree r = (*local_results)[i];
          if (result_value (r, R_SCOPE_KIND) == NEWTM_OB_COPY) continue;
          bool affected = TREE_VEC_ELT (r, R_OWNER) == changed;
          for (tree p = TREE_VEC_ELT (r, R_DEPENDENCIES); !affected && p; p = TREE_CHAIN (p))
            affected = TREE_VALUE (p) == changed;
          if (affected)
            {
              result_set (r, R_REMAP_BOUNDARIES, result_value (r, R_REMAP_BOUNDARIES) | boundary);
              tree caller = TREE_VEC_ELT (r, R_OWNER);
              if (caller) work.safe_push (caller);
            }
        }
    }
}
static void revoke_scope (unsigned HOST_WIDE_INT scope, newtm_local_state state = NEWTM_LOCAL_INVALID)
{
  if (!scope) return;
  tree owner = NULL_TREE;
  bool revoked = false;
  bool copy = false;
  for (unsigned i = 0; i < vec_safe_length (records); ++i)
    if (value ((*records)[i], SCOPE) == scope && value ((*records)[i], LOCAL_STATE) == NEWTM_LOCAL_APPROVED)
      { owner = TREE_VEC_ELT ((*records)[i], OWNER); revoked = true;
        copy = value ((*records)[i], SCOPE_KIND) == NEWTM_OB_COPY;
        set ((*records)[i], LOCAL_STATE, state); set ((*records)[i], PROVED, 0); }
  for (unsigned i = 0; i < vec_safe_length (local_results); ++i)
    if (result_value ((*local_results)[i], R_SCOPE) == scope
        && result_value ((*local_results)[i], R_STATE) == NEWTM_LOCAL_APPROVED)
      { owner = TREE_VEC_ELT ((*local_results)[i], R_OWNER); revoked = true;
        copy = result_value ((*local_results)[i], R_SCOPE_KIND) == NEWTM_OB_COPY;
        result_set ((*local_results)[i], R_STATE, state); result_set ((*local_results)[i], R_PROVED, 0);
        result_set ((*local_results)[i], R_GRAPH_INVALID, 1); }
  if (revoked && owner && !copy) invalidate_dependents (owner);
  if (revoked) invalidate_graph_results ();
}
static unsigned requirements (tree r)
{
  unsigned bits = NEWTM_V01 | NEWTM_V02 | NEWTM_V08;
  switch (value (r, KIND))
    {
    case NEWTM_OB_READ: case NEWTM_OB_WRITE: case NEWTM_OB_REGISTER: case NEWTM_OB_IDENTITY:
      bits |= NEWTM_V03 | NEWTM_V04 | NEWTM_V05; break;
    case NEWTM_OB_CALL: bits |= NEWTM_V04 | NEWTM_V05 | NEWTM_V06; break;
    case NEWTM_OB_PUBLISH: bits |= NEWTM_V04 | NEWTM_V05; break;
    case NEWTM_OB_CONTROL: bits |= NEWTM_V05; break;
    }
  if (live_scope_kind == NEWTM_OB_CONTROLLER || newtm_clone_context (live_owner)) bits |= NEWTM_V07;
  return bits;
}

static tree anchor_ids (const gimple *stmt)
{
  tree *p = live ? live->get (const_cast<gimple *> (stmt)) : NULL;
  return p ? *p : NULL_TREE;
}
static unsigned HOST_WIDE_INT find (const gimple *stmt, newtm_obligation_kind kind,
                                    int role = -2, unsigned operand = 0)
{
  for (tree p = anchor_ids (stmt); p; p = TREE_CHAIN (p))
    {
      tree r = record_at (tree_to_uhwi (TREE_VALUE (p)));
      if (value (r, KIND) == kind && (role == -2
          || (int (value (r, ROLE)) - 1 == role && value (r, OPERAND) == operand)))
        return tree_to_uhwi (TREE_VALUE (p));
    }
  return 0;
}

static unsigned HOST_WIDE_INT add (gimple *stmt, newtm_obligation_kind kind,
                                   int role = -1, unsigned operand = 0)
{
  if (unsigned HOST_WIDE_INT old = find (stmt, kind, role, operand)) return old;
  if (records) revoke_scope (live_scope);
  tree r = make_tree_vec (FIELDS), b = newtm_occurrence_binding (stmt);
  tree previous = anchor_ids (stmt);
  set (r, INSTANCE, previous
    ? value (record_at (tree_to_uhwi (TREE_VALUE (previous))), INSTANCE)
    : vec_safe_length (records) + 1);
  TREE_VEC_ELT (r, OWNER) = live_owner;
  set (r, SCOPE, live_scope); set (r, GENERATION, live_generation);
  set (r, KIND, kind); set (r, SCOPE_KIND, live_scope_kind);
  set (r, REQUIRED, requirements (r));
  set (r, STATE, live_reason ? NEWTM_OB_REJECTED : NEWTM_OB_PENDING);
  set (r, EVIDENCE, NEWTM_OB_CURRENT);
  TREE_VEC_ELT (r, REASON) = live_reason;
  set (r, LOCATION, gimple_location (stmt));
  set (r, ROLE, role + 1); set (r, OPERAND, operand);
  set (r, STORAGE, 2); /* Absent (-1), encoded with bias three. */
  if (references && is_gimple_assign (stmt)
      && (kind == NEWTM_OB_READ || kind == NEWTM_OB_WRITE))
    references->put (r, unshare_expr (kind == NEWTM_OB_WRITE ? gimple_assign_lhs (stmt)
                                     : gimple_assign_rhs1 (stmt)));
  if (references && is_gimple_call (stmt) && kind == NEWTM_OB_READ
      && operand < gimple_call_num_args (stmt))
    references->put (r, unshare_expr (gimple_call_arg (stmt, operand)));
  if (references && gimple_code (stmt) == GIMPLE_RETURN && kind == NEWTM_OB_READ)
    references->put (r, unshare_expr (gimple_return_retval (as_a<greturn *> (stmt))));
  if (b)
    {
      set (r, SOURCE, 1);
      const unsigned slots[] = { 0, 1, 3, 8 };
      const field fields[] = { OPERATION, ORIGIN, GROUP, COPY_OPERATION };
      for (unsigned i = 0; i < 4; ++i) TREE_VEC_ELT (r, fields[i]) = TREE_VEC_ELT (b, slots[i]);
      set (r, WITNESS, (tree_to_uhwi (TREE_VEC_ELT (b, 7)) & 32) != 0);
    }
  if (is_gimple_call (stmt)) TREE_VEC_ELT (r, CALLEE) = gimple_call_fndecl (stmt);
  if (gimple_code (stmt) == GIMPLE_OMP_TRANSACTION && kind == NEWTM_OB_CONTROL)
    TREE_VEC_ELT (r, CALLEE) = newtm_clone_region_body (stmt);
  if (call_inputs && is_gimple_call (stmt) && kind == NEWTM_OB_CALL)
    {
      tree args = make_tree_vec (gimple_call_num_args (stmt));
      for (unsigned i = 0; i < gimple_call_num_args (stmt); ++i)
        TREE_VEC_ELT (args, i) = unshare_expr (gimple_call_arg (stmt, i));
      call_inputs->put (r, args);
    }
  if (role >= 0)
    if (tree d = newtm_descriptor_find (stmt, (newtm_descriptor_role) role, operand))
      {
        if (identities)
          {
            tree snapshot = copy_node (d);
            for (unsigned i = 0; i < NEWTM_D_FIELDS; ++i)
              TREE_VEC_ELT (snapshot, i) = unshare_expr (TREE_VEC_ELT (d, i));
            identities->put (r, snapshot);
          }
        set (r, DESC_STATE, tree_to_uhwi (TREE_VEC_ELT (d, NEWTM_D_STATE)) + 1);
        unsigned mask = 0;
        for (unsigned i = 0; i < NEWTM_D_FIELDS; ++i)
          if (TREE_VEC_ELT (d, i)) mask |= 1u << i;
        set (r, DESC_FIELDS, mask);
        if (tree storage = TREE_VEC_ELT (d, NEWTM_D_STORAGE))
          set (r, STORAGE, TREE_CODE (storage) != INTEGER_CST ? 1
            : tree_fits_uhwi_p (storage) && tree_to_uhwi (storage) <= 1
            ? tree_to_uhwi (storage) + 3 : 0);
      }
  vec_safe_push (records, r);
  unsigned HOST_WIDE_INT id = records->length ();
  tree ids = anchor_ids (stmt);
  live->put (stmt, tree_cons (NULL_TREE, number (id), ids));
  if (value (r, OPERATION))
    {
      tree key = TREE_VEC_ELT (r, OPERATION);
      tree *old = operations->get (key);
      operations->put (key, tree_cons (NULL_TREE, number (id), old ? *old : NULL_TREE));
    }
  return id;
}

static bool memory (tree t)
{
  return t && (REFERENCE_CLASS_P (t)
    || ((VAR_P (t) || TREE_CODE (t) == PARM_DECL) && !is_gimple_reg (t)));
}
static bool lifecycle (tree callee)
{
  if (!callee || !fndecl_built_in_p (callee, BUILT_IN_NORMAL)) return false;
  switch (DECL_FUNCTION_CODE (callee))
    {
    case BUILT_IN_GOMP_TRANSACTION_BEGIN: case BUILT_IN_GOMP_TRANSACTION_END:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN: case BUILT_IN_GOMP_TRANSACTION_STM_V1_END:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS: case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_DISCARD: case BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN: case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL: return true;
    default: return false;
    }
}

static void inventory (gimple *stmt)
{
  unsigned expected = 0;
  if (shapes && !shapes->get (stmt) && is_gimple_call (stmt))
    {
      tree fn = gimple_call_fndecl (stmt);
      if (fn && fndecl_built_in_p (fn, BUILT_IN_GOMP_TRANSACTION_BEGIN))
        { expected = newtm_clone_region_owner (live_owner) && !inventory_depth
            ? BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS : BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN;
          ++inventory_depth; }
      else if (fn && fndecl_built_in_p (fn, BUILT_IN_GOMP_TRANSACTION_END))
        { if (inventory_depth) --inventory_depth;
          expected = newtm_clone_region_owner (live_owner) && !inventory_depth
            ? BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS : BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END; }
    }
  if (math_obligations && newtm_descriptor_math_p (stmt)) math_obligations->put (stmt, boolean_true_node);
  if (shapes && !shapes->get (stmt))
    {
      tree shape = make_tree_vec (gimple_num_ops (stmt) + 2);
      TREE_VEC_ELT (shape, 0) = number (gimple_code (stmt));
      TREE_VEC_ELT (shape, 1) = number (is_gimple_assign (stmt)
        ? unsigned (gimple_assign_rhs_code (stmt))
        : is_gimple_call (stmt) && gimple_call_internal_p (stmt)
        ? 1000 + unsigned (gimple_call_internal_fn (stmt)) : 0);
      for (unsigned i = 0; i < gimple_num_ops (stmt); ++i)
        TREE_VEC_ELT (shape, i + 2) = unshare_expr (gimple_op (stmt, i));
      shapes->put (stmt, shape);
    }
  tree b = newtm_occurrence_binding (stmt);
  unsigned mode = b ? tree_to_uhwi (TREE_VEC_ELT (b, 7)) : 0;
  if (mode & 32) add (stmt, NEWTM_OB_READ, NEWTM_DESC_SOURCE);
  if (is_gimple_assign (stmt) && !gimple_clobber_p (stmt))
    {
      tree lhs = gimple_assign_lhs (stmt);
      if (memory (lhs) || (mode & 2)) add (stmt, NEWTM_OB_WRITE, NEWTM_DESC_WRITE);
      for (unsigned i = 1; i < gimple_num_ops (stmt); ++i)
        if (memory (gimple_op (stmt, i))) add (stmt, NEWTM_OB_READ, NEWTM_DESC_READ, i - 1);
      if (mode & 1) add (stmt, NEWTM_OB_READ, NEWTM_DESC_READ);
      if (POINTER_TYPE_P (TREE_TYPE (lhs))) add (stmt, NEWTM_OB_IDENTITY);
    }
  else if (is_gimple_call (stmt))
    {
      tree callee = gimple_call_fndecl (stmt);
      bool registration = callee && (fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_REGISTER)
        || fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER));
      add (stmt, registration ? NEWTM_OB_REGISTER : NEWTM_OB_CALL,
           registration ? NEWTM_DESC_REGISTER : -1);
      if (newtm_clone_publication_p (live_owner, stmt)) add (stmt, NEWTM_OB_PUBLISH);
      if (lifecycle (callee)) add (stmt, NEWTM_OB_CONTROL);
      tree lhs = gimple_call_lhs (stmt);
      if (memory (lhs)) add (stmt, NEWTM_OB_WRITE, NEWTM_DESC_WRITE);
      if (lhs && POINTER_TYPE_P (TREE_TYPE (lhs))) add (stmt, NEWTM_OB_IDENTITY);
      for (unsigned i = 0; i < gimple_call_num_args (stmt); ++i)
        {
          tree arg = gimple_call_arg (stmt, i);
          if (memory (arg)) add (stmt, NEWTM_OB_READ, -1, i);
          if (POINTER_TYPE_P (TREE_TYPE (arg))) add (stmt, NEWTM_OB_IDENTITY, NEWTM_DESC_ARGUMENT, i);
        }
    }
  else if (gimple_code (stmt) == GIMPLE_RETURN)
    {
      add (stmt, NEWTM_OB_PUBLISH); add (stmt, NEWTM_OB_CONTROL);
      if (memory (gimple_return_retval (as_a<greturn *> (stmt)))) add (stmt, NEWTM_OB_READ);
    }
  else if (gimple_code (stmt) == GIMPLE_OMP_TRANSACTION || gimple_code (stmt) == GIMPLE_COND
           || gimple_code (stmt) == GIMPLE_SWITCH || gimple_code (stmt) == GIMPLE_GOTO)
    add (stmt, NEWTM_OB_CONTROL);
  else if (gimple_code (stmt) == GIMPLE_ASM)
    { add (stmt, NEWTM_OB_CALL); add (stmt, NEWTM_OB_CONTROL); }
  else if (!gimple_has_substatements (stmt) && gimple_has_side_effects (stmt))
    add (stmt, NEWTM_OB_CONTROL); /* Unclassified effects must remain visible. */
  for (tree p = newtm_descriptor_entries (stmt); p; p = TREE_CHAIN (p))
    add (stmt, NEWTM_OB_IDENTITY, tree_to_uhwi (TREE_VEC_ELT (TREE_PURPOSE (p), 0)),
         tree_to_uhwi (TREE_VEC_ELT (TREE_PURPOSE (p), 1)));
  if (expected)
    for (tree p = anchor_ids (stmt); p; p = TREE_CHAIN (p))
      {
        tree r = record_at (tree_to_uhwi (TREE_VALUE (p)));
        if (value (r, KIND) == NEWTM_OB_CONTROL || value (r, KIND) == NEWTM_OB_CALL)
          set (r, EXPECTED_SERVICE, expected);
      }
}
static tree walk_inventory (gimple_stmt_iterator *it, bool *, walk_stmt_info *)
{ inventory (gsi_stmt (*it)); return NULL_TREE; }

bool newtm_obligation_begin (tree fn, gimple_seq body, newtm_obligation_scope role, gimple *root)
{
  if (live_owner || !fn) return false;
  if (!operations) operations = hash_map<tree, tree>::create_ggc ();
  live = hash_map<gimple *, tree>::create_ggc ();
  math_obligations = hash_map<gimple *, tree>::create_ggc ();
  emission_sources = hash_map<gimple *, tree>::create_ggc ();
  abort_edges = hash_map<gimple *, tree>::create_ggc ();
  shapes = hash_map<gimple *, tree>::create_ggc ();
  identities = hash_map<tree, tree>::create_ggc ();
  references = hash_map<tree, tree>::create_ggc ();
  call_inputs = hash_map<tree, tree>::create_ggc ();
  live_owner = fn; live_reason = local_refusal = NULL_TREE;
  live_scope = ++next_scope; live_generation = 1; live_scope_kind = role;
  inventory_depth = 0;
  local_failure_result = 0;
  if (root) inventory (root);
  walk_stmt_info wi = {};
  walk_gimple_seq (body, walk_inventory, NULL, &wi);
  return true;
}

static void retire_one (const gimple *stmt)
{
  revoke_scope (live_scope);
  for (tree p = anchor_ids (stmt); p; p = TREE_CHAIN (p))
    {
      tree r = record_at (tree_to_uhwi (TREE_VALUE (p)));
      if (value (r, EVIDENCE) == NEWTM_OB_CURRENT) set (r, EVIDENCE, NEWTM_OB_RETIRED);
    }
  if (live) live->remove (const_cast<gimple *> (stmt));
}
void newtm_obligation_forget (const gimple *stmt) { retire_one (stmt); }
static tree walk_retire (gimple_stmt_iterator *it, bool *, walk_stmt_info *)
{ retire_one (gsi_stmt (*it)); return NULL_TREE; }
void newtm_obligation_retire (gimple_seq seq)
{
  if (!live_owner) return;
  walk_stmt_info wi = {}; walk_gimple_seq (seq, walk_retire, NULL, &wi);
}
void newtm_obligation_end ()
{
  if (!live_owner) return;
  for (unsigned i = 0; i < vec_safe_length (local_results); ++i)
    if (result_value ((*local_results)[i], R_SCOPE) == live_scope) result_set ((*local_results)[i], R_CLOSED, 1);
  for (auto it = live->begin (); it != live->end (); ++it)
    for (tree p = (*it).second; p; p = TREE_CHAIN (p))
      {
        tree r = record_at (tree_to_uhwi (TREE_VALUE (p)));
        if (value (r, EVIDENCE) == NEWTM_OB_CURRENT) set (r, EVIDENCE, NEWTM_OB_RETIRED);
      }
  live = NULL; live_owner = live_reason = local_refusal = NULL_TREE; live_scope = 0;
  local_failure_result = 0;
  shapes = NULL; identities = references = call_inputs = private_roots = NULL;
  math_obligations = NULL;
  emission_sources = NULL;
  abort_edges = NULL;
}
bool newtm_obligation_math_p (const gimple *s)
{ return live_owner && math_obligations && math_obligations->get (const_cast<gimple *> (s)); }
void newtm_obligation_math (gimple *s)
{ if (math_obligations) { revoke_scope (live_scope); math_obligations->put (s, boolean_true_node); } }
void newtm_obligation_abort (gimple *s, tree failure)
{ if (abort_edges) { revoke_scope (live_scope); abort_edges->put (s, failure); } }
bool newtm_obligation_abort_p (const gimple *s)
{ return abort_edges && abort_edges->get (const_cast<gimple *> (s)); }

static void declare_record (tree r, newtm_obligation_treatment treatment, tree callee)
{
  revoke_scope (value (r, SCOPE));
  if (value (r, STATE) == NEWTM_OB_PENDING && value (r, EVIDENCE) == NEWTM_OB_CURRENT)
    set (r, TREATMENT, treatment);
  if (callee) TREE_VEC_ELT (r, DECLARED_CALLEE) = callee;
}
void newtm_obligation_declare (const gimple *stmt, newtm_obligation_kind kind,
                              newtm_obligation_treatment treatment)
{
  for (tree p = anchor_ids (stmt); p; p = TREE_CHAIN (p))
    {
      tree r = record_at (tree_to_uhwi (TREE_VALUE (p)));
      int role = int (value (r, ROLE)) - 1;
      if (value (r, KIND) == kind || (value (r, KIND) == NEWTM_OB_IDENTITY
          && ((kind == NEWTM_OB_READ && role == NEWTM_DESC_READ)
              || (kind == NEWTM_OB_WRITE && role == NEWTM_DESC_WRITE))))
        declare_record (r, treatment, NULL_TREE);
    }
}
void newtm_obligation_declare_all (const gimple *stmt, newtm_obligation_treatment treatment, tree callee)
{
  for (tree p = anchor_ids (stmt); p; p = TREE_CHAIN (p))
    declare_record (record_at (tree_to_uhwi (TREE_VALUE (p))), treatment, callee);
}
void newtm_obligation_emitted (gimple_seq seq, const gimple *producer)
{
  if (!live_owner) return;
  unsigned HOST_WIDE_INT source = 0;
  const newtm_obligation_kind priority[] = { NEWTM_OB_PUBLISH, NEWTM_OB_CONTROL,
    NEWTM_OB_CALL, NEWTM_OB_REGISTER, NEWTM_OB_READ, NEWTM_OB_WRITE, NEWTM_OB_IDENTITY };
  for (newtm_obligation_kind kind : priority)
    if ((source = find (producer, kind))) break;
  struct annotate
  {
    static tree one (gimple_stmt_iterator *p, bool *, walk_stmt_info *wi)
    {
      gimple *s = gsi_stmt (*p);
      if (!emission_sources->get (s)) emission_sources->put (s, (tree) wi->info);
      return NULL_TREE;
    }
  };
  walk_stmt_info annotation = {}; annotation.info = number (source);
  walk_gimple_seq (seq, annotate::one, NULL, &annotation);
  unsigned first = vec_safe_length (records);
  walk_stmt_info wi = {}; walk_gimple_seq (seq, walk_inventory, NULL, &wi);
  for (unsigned i = first; i < vec_safe_length (records); ++i)
    { set ((*records)[i], TREATMENT, live_reason ? NEWTM_OB_FAILURE_STUB : NEWTM_OB_EMITTED);
      set ((*records)[i], PRODUCER, source); }
}
void newtm_obligation_reject (const char *reason)
{
  if (!live_owner) return;
  revoke_scope (live_scope, NEWTM_LOCAL_REFUSED);
  live_reason = build_string (strlen (reason) + 1, reason);
  for (unsigned i = 0; i < vec_safe_length (records); ++i)
    if (value ((*records)[i], SCOPE) == live_scope)
      { set ((*records)[i], STATE, NEWTM_OB_REJECTED); set ((*records)[i], LOCAL_STATE, NEWTM_LOCAL_REFUSED);
        set ((*records)[i], PROVED, 0); TREE_VEC_ELT ((*records)[i], REASON) = live_reason; }
}

static void derive (unsigned HOST_WIDE_INT parent, unsigned HOST_WIDE_INT child,
                    newtm_obligation_treatment treatment)
{
  tree p = record_at (parent), c = record_at (child);
  if (!p || !c || parent >= child || value (c, PARENT)
      || value (p, SCOPE) != value (c, SCOPE)
      || value (p, STATE) == NEWTM_OB_REJECTED
      || value (p, EVIDENCE) != NEWTM_OB_CURRENT
      || value (c, EVIDENCE) != NEWTM_OB_CURRENT) return;
  set (p, STATE, NEWTM_OB_DERIVED); set (p, TREATMENT, treatment);
  revoke_scope (value (p, SCOPE));
  set (c, PARENT, parent);
  TREE_VEC_ELT (p, CHILDREN) = tree_cons (NULL_TREE, number (child), TREE_VEC_ELT (p, CHILDREN));
}
void newtm_obligation_reconcile (const gimple *witness, const gimple *actual)
{
  unsigned HOST_WIDE_INT parent = find (witness, NEWTM_OB_READ), child = find (actual, NEWTM_OB_READ);
  tree p = record_at (parent), c = record_at (child);
  if (p && c && value (p, WITNESS) && !value (c, WITNESS))
    derive (parent, child, NEWTM_OB_RECONCILED);
}
void newtm_obligation_derive_store (const gimple *original, gimple *store)
{
  if (!live_owner) return;
  inventory (store);
  tree lhs = is_gimple_call (original) ? gimple_call_lhs (original)
    : is_gimple_assign (original) ? gimple_assign_lhs (original) : NULL_TREE;
  if (!is_gimple_assign (store) || lhs != gimple_assign_lhs (store)) return;
  derive (find (original, NEWTM_OB_WRITE), find (store, NEWTM_OB_WRITE), NEWTM_OB_SPLIT_STORE);
  derive (find (original, NEWTM_OB_IDENTITY, NEWTM_DESC_WRITE),
          find (store, NEWTM_OB_IDENTITY, NEWTM_DESC_WRITE), NEWTM_OB_SPLIT_STORE);
}

void newtm_obligation_derive_control (const gimple *original, gimple_seq body)
{
  unsigned HOST_WIDE_INT parent = find (original, NEWTM_OB_CONTROL);
  if (!parent) return;
  struct children
  {
    static tree one (gimple_stmt_iterator *p, bool *, walk_stmt_info *wi)
    {
      unsigned HOST_WIDE_INT parent = *(unsigned HOST_WIDE_INT *) wi->info;
      unsigned HOST_WIDE_INT child = find (gsi_stmt (*p), NEWTM_OB_CONTROL);
      if (child && child != parent) derive (parent, child, NEWTM_OB_LIFECYCLE);
      return NULL_TREE;
    }
  };
  walk_stmt_info wi = {}; wi.info = &parent; walk_gimple_seq (body, children::one, NULL, &wi);
}

void newtm_obligation_copy (const gimple *original, gimple *copy)
{
  tree b = newtm_occurrence_binding (original);
  tree *old = b && operations ? operations->get (TREE_VEC_ELT (b, 0)) : NULL;
  tree ids = anchor_ids (original);
  if (!ids && old) ids = *old;
  if (!ids) return; /* A later inventory will discover an untracked copy. */
  unsigned HOST_WIDE_INT instance = value (record_at (tree_to_uhwi (TREE_VALUE (ids))), INSTANCE);
  tree saved_owner = live_owner, saved_reason = live_reason;
  unsigned HOST_WIDE_INT saved_scope = live_scope, saved_generation = live_generation;
  newtm_obligation_scope saved_kind = live_scope_kind;
  bool detached = !live_owner;
  if (detached)
    {
      live = hash_map<gimple *, tree>::create_ggc ();
      live_owner = cfun ? cfun->decl : TREE_VEC_ELT (record_at (tree_to_uhwi (TREE_VALUE (ids))), OWNER);
      live_reason = NULL_TREE; live_scope = ++next_scope; live_scope_kind = NEWTM_OB_COPY;
    }
  for (tree p = ids; p; p = TREE_CHAIN (p))
    {
      unsigned HOST_WIDE_INT source = tree_to_uhwi (TREE_VALUE (p));
      tree r = record_at (source);
      if (value (r, INSTANCE) != instance) continue;
      live_generation = MAX (saved_generation, value (r, GENERATION) + 1);
      unsigned HOST_WIDE_INT id = add (copy, (newtm_obligation_kind) value (r, KIND),
                                       int (value (r, ROLE)) - 1, value (r, OPERAND));
      tree next = record_at (id);
      set (next, COPY_FROM, source); set (next, EVIDENCE, NEWTM_OB_INVALIDATED);
      set (next, LOCAL_STATE, NEWTM_LOCAL_INVALID);
    }
  if (detached) newtm_obligation_end ();
  live_owner = saved_owner; live_reason = saved_reason;
  live_scope = saved_scope; live_generation = saved_generation; live_scope_kind = saved_kind;
}

void newtm_obligation_invalidate (unsigned HOST_WIDE_INT id)
{
  tree r = record_at (id);
  if (!r || value (r, EVIDENCE) == NEWTM_OB_INVALIDATED) return;
  revoke_scope (value (r, SCOPE));
  if (value (r, LOCAL_STATE) != NEWTM_LOCAL_REFUSED) set (r, LOCAL_STATE, NEWTM_LOCAL_INVALID);
  set (r, PROVED, 0);
  set (r, EVIDENCE, NEWTM_OB_INVALIDATED);
  if (value (r, STATE) != NEWTM_OB_REJECTED) set (r, STATE, NEWTM_OB_PENDING);
  if (value (r, PARENT)) newtm_obligation_invalidate (value (r, PARENT));
  for (tree p = TREE_VEC_ELT (r, CHILDREN); p; p = TREE_CHAIN (p))
    newtm_obligation_invalidate (tree_to_uhwi (TREE_VALUE (p)));
  for (unsigned i = 0; i < vec_safe_length (records); ++i)
    if (value ((*records)[i], PRODUCER) == id
        || value ((*records)[i], INSTANCE) == value (r, INSTANCE))
      newtm_obligation_invalidate (i + 1);
}
void newtm_obligation_reset ()
{ newtm_obligation_end (); operations = NULL; }
unsigned HOST_WIDE_INT newtm_obligation_count () { return vec_safe_length (records); }
bool newtm_obligation_get (unsigned HOST_WIDE_INT id, newtm_obligation_view *out)
{
  tree r = record_at (id);
  if (!r || !out) return false;
  *out = {};
  out->id = id;
#define SNAP(N, F) out->N = value (r, F)
  SNAP (instance, INSTANCE); SNAP (scope, SCOPE); SNAP (generation, GENERATION); SNAP (operation, OPERATION);
  SNAP (origin, ORIGIN); SNAP (group, GROUP); SNAP (copied_operation, COPY_OPERATION);
  SNAP (copied_from, COPY_FROM); SNAP (parent, PARENT); SNAP (producer, PRODUCER);
  SNAP (location, LOCATION); SNAP (operand, OPERAND); SNAP (descriptor_fields, DESC_FIELDS);
  SNAP (source, SOURCE); SNAP (witness, WITNESS);
  SNAP (required, REQUIRED); SNAP (proved, PROVED); SNAP (local_result, LOCAL_RESULT);
#undef SNAP
  out->role = int (value (r, ROLE)) - 1;
  out->descriptor_state = int (value (r, DESC_STATE)) - 1;
  out->storage = int (value (r, STORAGE)) - 3;
  out->kind = (newtm_obligation_kind) value (r, KIND);
  out->scope_kind = (newtm_obligation_scope) value (r, SCOPE_KIND);
  out->state = (newtm_obligation_state) value (r, STATE);
  out->evidence = (newtm_obligation_evidence) value (r, EVIDENCE);
  out->treatment = (newtm_obligation_treatment) value (r, TREATMENT);
  out->owner = TREE_VEC_ELT (r, OWNER); out->callee = TREE_VEC_ELT (r, CALLEE);
  out->declared_callee = TREE_VEC_ELT (r, DECLARED_CALLEE);
  out->local_state = (newtm_local_state) value (r, LOCAL_STATE);
  out->dependencies = TREE_VEC_ELT (r, DEPENDENCIES);
  tree reason = TREE_VEC_ELT (r, REASON);
  out->reason = reason ? TREE_STRING_POINTER (reason) : NULL;
  for (tree p = TREE_VEC_ELT (r, CHILDREN); p; p = TREE_CHAIN (p)) ++out->children;
  return true;
}
bool newtm_obligation_has_anchor (unsigned HOST_WIDE_INT id)
{
  if (!live_owner) return false;
  for (auto it = live->begin (); it != live->end (); ++it)
    for (tree p = (*it).second; p; p = TREE_CHAIN (p))
      if (tree_to_uhwi (TREE_VALUE (p)) == id) return true;
  return false;
}

unsigned HOST_WIDE_INT newtm_obligation_local_count () { return vec_safe_length (local_results); }
bool newtm_obligation_local_get (unsigned HOST_WIDE_INT id, newtm_local_view *out)
{
  tree r = result_at (id); if (!r || !out) return false;
  *out = {}; out->id = id;
#define R(N, F) out->N = result_value (r, F)
  R (scope, R_SCOPE); R (generation, R_GENERATION); R (required, R_REQUIRED); R (proved, R_PROVED);
  R (obligation, R_OBLIGATION); R (ancestor, R_ANCESTOR); R (rule, R_RULE);
  R (location, R_LOCATION); R (closed, R_CLOSED);
  R (graph_invalid, R_GRAPH_INVALID);
  R (remap_boundaries, R_REMAP_BOUNDARIES);
#undef R
  out->state = (newtm_local_state) result_value (r, R_STATE);
  out->scope_kind = (newtm_obligation_scope) result_value (r, R_SCOPE_KIND);
  out->kind = int (result_value (r, R_KIND)) - 1;
  out->owner = TREE_VEC_ELT (r, R_OWNER); out->dependencies = TREE_VEC_ELT (r, R_DEPENDENCIES);
  tree why = TREE_VEC_ELT (r, R_REASON); out->reason = why ? TREE_STRING_POINTER (why) : NULL;
  return true;
}
unsigned HOST_WIDE_INT newtm_obligation_graph_count () { return vec_safe_length (graph_results); }
/* Capture signature links while they are available; comparison is deliberately
   identity based and does not confer approval on remapped declarations. */
static tree graph_signature (tree fn)
{
  if (!fn || TREE_CODE (fn) != FUNCTION_DECL) return NULL_TREE;
  tree type = TREE_TYPE (fn);
  if (!type || (TREE_CODE (type) != FUNCTION_TYPE && TREE_CODE (type) != METHOD_TYPE)) return NULL_TREE;
  tree snapshot = make_tree_vec (3);
  TREE_VEC_ELT (snapshot, 0) = type;
  TREE_VEC_ELT (snapshot, 1) = TREE_TYPE (type);
  TREE_VEC_ELT (snapshot, 2) = copy_list (TYPE_ARG_TYPES (type));
  return snapshot;
}
static bool graph_signature_matches (tree fn, tree snapshot)
{
  if (!fn || !snapshot || TREE_CODE (fn) != FUNCTION_DECL) return false;
  tree type = TREE_TYPE (fn);
  if (type != TREE_VEC_ELT (snapshot, 0) || TREE_TYPE (type) != TREE_VEC_ELT (snapshot, 1)) return false;
  tree a = TYPE_ARG_TYPES (type), b = TREE_VEC_ELT (snapshot, 2);
  for (; a && b; a = TREE_CHAIN (a), b = TREE_CHAIN (b))
    if (TREE_VALUE (a) != TREE_VALUE (b) || TREE_PURPOSE (a) != TREE_PURPOSE (b)) return false;
  if (a || b) return false;
  return true;
}
bool newtm_obligation_graph_get (unsigned HOST_WIDE_INT id, newtm_graph_view *out)
{
  if (!out || !id || id > vec_safe_length (graph_results)) return false;
  tree g = (*graph_results)[id - 1];
  for (tree p = TREE_VEC_ELT (g, 6); p; p = TREE_CHAIN (p))
    {
      tree r = result_at (tree_to_uhwi (TREE_PURPOSE (p)));
      if (!r || !graph_signature_matches (TREE_VEC_ELT (r, R_OWNER), TREE_VALUE (p)))
        {
          TREE_VEC_ELT (g, 3) = boolean_false_node;
          if (r)
            { result_set (r, R_GRAPH_DIRTY, 1); result_set (r, R_GRAPH_INVALID, 1); invalidate_dependents (TREE_VEC_ELT (r, R_OWNER)); }
          invalidate_graph_results ();
          break;
        }
    }
  for (tree p = TREE_VEC_ELT (g, 4); p; p = TREE_CHAIN (p))
    {
      tree r = result_at (tree_to_uhwi (TREE_PURPOSE (p)));
      tree old = TREE_VALUE (p), now = r ? TREE_VEC_ELT (r, R_DEPENDENCIES) : NULL_TREE;
      bool equal = r != NULL_TREE;
      for (tree a = old; equal && a; a = TREE_CHAIN (a))
        { tree b; for (b = now; b && TREE_VALUE (b) != TREE_VALUE (a); b = TREE_CHAIN (b)) ; equal = b != NULL_TREE; }
      for (tree a = now; equal && a; a = TREE_CHAIN (a))
        { tree b; for (b = old; b && TREE_VALUE (b) != TREE_VALUE (a); b = TREE_CHAIN (b)) ; equal = b != NULL_TREE; }
      if (!equal || result_value (r, R_STATE) != NEWTM_LOCAL_APPROVED || result_value (r, R_GRAPH_INVALID))
        {
          TREE_VEC_ELT (g, 3) = boolean_false_node;
          if (r)
            { if (!equal) result_set (r, R_GRAPH_DIRTY, 1); result_set (r, R_GRAPH_INVALID, 1); invalidate_dependents (TREE_VEC_ELT (r, R_OWNER)); }
          invalidate_graph_results ();
          break;
        }
    }
  out->root = TREE_VEC_ELT (g, 0); out->controller = tree_to_uhwi (TREE_VEC_ELT (g, 1));
  out->members = list_length (TREE_VEC_ELT (g, 2)); out->valid = integer_onep (TREE_VEC_ELT (g, 3));
  /* A historical certificate does not prove the currently transformed IR.
     A future verifier must publish an independent current approval. */
  out->current_approved = false;
  out->remap_boundaries = 0;
  for (tree p = TREE_VEC_ELT (g, 2); p; p = TREE_CHAIN (p))
    if (tree r = result_at (tree_to_uhwi (TREE_VALUE (p))))
      out->remap_boundaries |= result_value (r, R_REMAP_BOUNDARIES);
  return true;
}
bool newtm_obligation_graph_member (unsigned HOST_WIDE_INT id, unsigned index, unsigned HOST_WIDE_INT *out)
{
  if (!out || !id || id > vec_safe_length (graph_results)) return false;
  tree p = TREE_VEC_ELT ((*graph_results)[id - 1], 2);
  while (p && index--) p = TREE_CHAIN (p);
  if (!p) return false;
  *out = tree_to_uhwi (TREE_VALUE (p)); return true;
}
bool newtm_obligation_graph_component (unsigned HOST_WIDE_INT id, unsigned HOST_WIDE_INT revision, unsigned *out)
{
  if (!out || !id || id > vec_safe_length (graph_results)) return false;
  for (tree p = TREE_VEC_ELT ((*graph_results)[id - 1], 5); p; p = TREE_CHAIN (p))
    if (tree_to_uhwi (TREE_PURPOSE (p)) == revision) { *out = tree_to_uhwi (TREE_VALUE (p)); return true; }
  return false;
}

/* Iterative Kosaraju traversal avoids recursion on a potentially large call
   graph. All nodes have already passed local and preflight checks. */
static tree graph_components (tree revisions)
{
  struct edge { unsigned node, next; };
  struct frame { unsigned node, next; };
  auto_vec<tree> nodes; hash_map<tree, unsigned> indices;
  for (tree p = revisions; p; p = TREE_CHAIN (p))
    { tree r = result_at (tree_to_uhwi (TREE_VALUE (p)));
      indices.put (TREE_VEC_ELT (r, R_OWNER), nodes.length ()); nodes.safe_push (TREE_VALUE (p)); }
  unsigned n = nodes.length ();
  auto_vec<unsigned> forward, reverse, order, stack, component;
  auto_vec<unsigned char> seen;
  auto_vec<edge> edges, reversed;
  forward.safe_grow (n); reverse.safe_grow (n); component.safe_grow_cleared (n); seen.safe_grow_cleared (n);
  for (unsigned i = 0; i < n; ++i) forward[i] = reverse[i] = UINT_MAX;
  for (unsigned i = 0; i < n; ++i)
    {
      tree r = result_at (tree_to_uhwi (nodes[i]));
      for (tree p = TREE_VEC_ELT (r, R_DEPENDENCIES); p; p = TREE_CHAIN (p))
        {
          unsigned *target = indices.get (TREE_VALUE (p));
          if (!target) return NULL_TREE; /* Closure must hold even with checking disabled. */
          edge a = { *target, forward[i] }, b = { i, reverse[*target] };
          forward[i] = edges.length (); edges.safe_push (a);
          reverse[*target] = reversed.length (); reversed.safe_push (b);
        }
    }
  auto_vec<frame> frames;
  for (unsigned i = 0; i < n; ++i)
    if (!seen[i])
      {
        seen[i] = 1; frame first = { i, forward[i] }; frames.safe_push (first);
        while (!frames.is_empty ())
          {
            frame &f = frames.last ();
            if (f.next == UINT_MAX) { order.safe_push (f.node); frames.pop (); continue; }
            edge e = edges[f.next]; f.next = e.next;
            if (!seen[e.node]) { seen[e.node] = 1; frame next = { e.node, forward[e.node] }; frames.safe_push (next); }
          }
      }
  for (unsigned i = 0; i < n; ++i) seen[i] = 0;
  unsigned group = 0;
  while (!order.is_empty ())
    {
      unsigned start = order.pop (); if (seen[start]) continue;
      ++group; seen[start] = 1; stack.safe_push (start);
      while (!stack.is_empty ())
        {
          unsigned node = stack.pop (); component[node] = group;
          for (unsigned e = reverse[node]; e != UINT_MAX; e = reversed[e].next)
            if (!seen[reversed[e].node]) { seen[reversed[e].node] = 1; stack.safe_push (reversed[e].node); }
        }
    }
  tree result = NULL_TREE;
  for (unsigned i = 0; i < n; ++i) result = tree_cons (nodes[i], number (component[i]), result);
  return result;
}
static bool graph_commit (tree root, unsigned HOST_WIDE_INT controller, tree revisions, bool revalidate)
{
  tree c = result_at (controller);
  if (!root || !c || result_value (c, R_SCOPE_KIND) != NEWTM_OB_CONTROLLER
      || result_value (c, R_STATE) != NEWTM_LOCAL_APPROVED || result_value (c, R_GRAPH_DIRTY)
      || (!revalidate && result_value (c, R_GRAPH_INVALID))
      || !result_value (c, R_CLOSED) || result_value (c, R_REQUIRED) != result_value (c, R_PROVED)) return false;
  for (unsigned j = vec_safe_length (local_results); j > controller; --j)
    if (result_value ((*local_results)[j - 1], R_SCOPE) == result_value (c, R_SCOPE)) return false;
  bool controller_edge = false;
  for (tree p = TREE_VEC_ELT (c, R_DEPENDENCIES); p; p = TREE_CHAIN (p))
    { if (TREE_VALUE (p) != root) return false; controller_edge = true; }
  if (!controller_edge) return false;
  hash_set<tree> owners;
  unsigned count = 0;
  for (tree p = revisions; p; p = TREE_CHAIN (p))
    {
      if (++count > vec_safe_length (local_results) || !TREE_VALUE (p)
          || TREE_CODE (TREE_VALUE (p)) != INTEGER_CST || !tree_fits_uhwi_p (TREE_VALUE (p))) return false;
      unsigned HOST_WIDE_INT id = tree_to_uhwi (TREE_VALUE (p));
      tree r = result_at (id);
      if (!r || result_value (r, R_STATE) != NEWTM_LOCAL_APPROVED || result_value (r, R_GRAPH_DIRTY)
          || (!revalidate && result_value (r, R_GRAPH_INVALID))
          || !result_value (r, R_CLOSED) || result_value (r, R_REQUIRED) != result_value (r, R_PROVED)
          || (result_value (r, R_SCOPE_KIND) != NEWTM_OB_CLONE && result_value (r, R_SCOPE_KIND) != NEWTM_OB_REGION)
          || !newtm_clone_matches_preflight (TREE_VEC_ELT (r, R_OWNER), TREE_VEC_ELT (r, R_DEPENDENCIES))
          || owners.add (TREE_VEC_ELT (r, R_OWNER))) return false;
      for (unsigned j = vec_safe_length (local_results); j > id; --j)
        {
          tree later = (*local_results)[j - 1];
          unsigned role = result_value (later, R_SCOPE_KIND);
          if (TREE_VEC_ELT (later, R_OWNER) == TREE_VEC_ELT (r, R_OWNER)
              && (role == NEWTM_OB_CLONE || role == NEWTM_OB_REGION)) return false;
        }
    }
  hash_set<tree> visited;
  auto_vec<tree> work; work.safe_push (root);
  while (!work.is_empty ())
    {
      tree fn = work.pop (); if (visited.add (fn)) continue;
      tree found = NULL_TREE;
      for (tree p = revisions; p; p = TREE_CHAIN (p))
        {
          tree r = result_at (tree_to_uhwi (TREE_VALUE (p)));
          if (TREE_VEC_ELT (r, R_OWNER) == fn) { found = r; break; }
        }
      if (!found) return false;
      for (tree p = TREE_VEC_ELT (found, R_DEPENDENCIES); p; p = TREE_CHAIN (p)) work.safe_push (TREE_VALUE (p));
    }
  for (tree p = revisions; p; p = TREE_CHAIN (p))
    if (!visited.contains (TREE_VEC_ELT (result_at (tree_to_uhwi (TREE_VALUE (p))), R_OWNER))) return false;
  tree components = graph_components (revisions);
  if (!components) return false;
  tree g = make_tree_vec (7);
  TREE_VEC_ELT (g, 0) = root; TREE_VEC_ELT (g, 1) = number (controller);
  TREE_VEC_ELT (g, 2) = tree_cons (NULL_TREE, number (controller), copy_list (revisions));
  TREE_VEC_ELT (g, 3) = boolean_true_node;
  TREE_VEC_ELT (g, 5) = components;
  for (tree p = TREE_VEC_ELT (g, 2); p; p = TREE_CHAIN (p))
    {
      tree r = result_at (tree_to_uhwi (TREE_VALUE (p)));
      TREE_VEC_ELT (g, 4) = tree_cons (TREE_VALUE (p), copy_list (TREE_VEC_ELT (r, R_DEPENDENCIES)), TREE_VEC_ELT (g, 4));
      tree signature = graph_signature (TREE_VEC_ELT (r, R_OWNER));
      if (!signature) return false;
      TREE_VEC_ELT (g, 6) = tree_cons (TREE_VALUE (p), signature, TREE_VEC_ELT (g, 6));
    }
  /* Publish only after every revision, edge and component has passed. */
  if (revalidate)
    for (tree p = TREE_VEC_ELT (g, 2); p; p = TREE_CHAIN (p))
      result_set (result_at (tree_to_uhwi (TREE_VALUE (p))), R_GRAPH_INVALID, 0);
  vec_safe_push (graph_results, g);
  return true;
}
bool newtm_obligation_graph_commit (tree root, unsigned HOST_WIDE_INT controller, tree revisions)
{ return graph_commit (root, controller, revisions, false); }

/* Propagation alone does not revoke a local proof. Direct changes must use
   the local invalidation API instead. */
void newtm_obligation_graph_invalidate_callers (tree owner)
{ invalidate_dependents (owner); invalidate_graph_results (); }

bool newtm_obligation_graph_revalidate (unsigned HOST_WIDE_INT id,
                                        unsigned HOST_WIDE_INT controller, tree revisions)
{
  newtm_graph_view view;
  if (!newtm_obligation_graph_get (id, &view) || controller != view.controller) return false;
  tree old = (*graph_results)[id - 1];
  /* Check all old signatures, even when another invalid member was found
     first. Changed interfaces require fresh local evidence. */
  for (tree p = TREE_VEC_ELT (old, 6); p; p = TREE_CHAIN (p))
    {
      tree r = result_at (tree_to_uhwi (TREE_PURPOSE (p)));
      if (!r || !graph_signature_matches (TREE_VEC_ELT (r, R_OWNER), TREE_VALUE (p)))
        {
          if (r) { result_set (r, R_GRAPH_DIRTY, 1); result_set (r, R_GRAPH_INVALID, 1);
                   invalidate_dependents (TREE_VEC_ELT (r, R_OWNER)); }
          invalidate_graph_results (); return false;
        }
    }
  /* Reused revisions must still have exactly their recorded edges. */
  for (tree p = TREE_VEC_ELT (old, 4); p; p = TREE_CHAIN (p))
    {
      unsigned HOST_WIDE_INT rev = tree_to_uhwi (TREE_PURPOSE (p));
      bool reused = rev == controller;
      unsigned count = 0;
      for (tree q = revisions; q; q = TREE_CHAIN (q))
        {
          if (++count > vec_safe_length (local_results) || !TREE_VALUE (q)
              || TREE_CODE (TREE_VALUE (q)) != INTEGER_CST || !tree_fits_uhwi_p (TREE_VALUE (q))) return false;
          reused |= tree_to_uhwi (TREE_VALUE (q)) == rev;
        }
      if (!reused) continue;
      tree r = result_at (rev);
      if (!r) return false;
      tree before = TREE_VALUE (p), now = TREE_VEC_ELT (r, R_DEPENDENCIES);
      bool same = true;
      for (tree a = before; same && a; a = TREE_CHAIN (a))
        { tree b; for (b = now; b && TREE_VALUE (b) != TREE_VALUE (a); b = TREE_CHAIN (b)) ; same = b != NULL_TREE; }
      for (tree a = now; same && a; a = TREE_CHAIN (a))
        { tree b; for (b = before; b && TREE_VALUE (b) != TREE_VALUE (a); b = TREE_CHAIN (b)) ; same = b != NULL_TREE; }
      if (!same)
        { result_set (r, R_GRAPH_DIRTY, 1); result_set (r, R_GRAPH_INVALID, 1);
          invalidate_dependents (TREE_VEC_ELT (r, R_OWNER)); invalidate_graph_results (); return false; }
    }
  return graph_commit (view.root, controller, revisions, true);
}
void newtm_obligation_report (location_t fallback)
{
  newtm_local_view v;
  if (!newtm_obligation_local_get (newtm_obligation_local_count (), &v) || !v.reason) return;
  static const char *const kinds[] = { "read", "write", "call", "register", "identity", "publication", "control" };
  const char *kind = v.kind >= 0 && v.kind < 7 ? kinds[v.kind] : "scope/instruction";
  location_t loc = v.location != UNKNOWN_LOCATION ? v.location : fallback;
  if (v.owner)
    error_at (loc, "transaction verification failed in %qD (operation %s, obligation %wu, scope %wu, generation %wu): %s",
              v.owner, kind, v.obligation, v.scope, v.generation, v.reason);
  else error_at (loc, "transaction verification failed (operation %s): %s", kind, v.reason);
  if (v.ancestor && v.ancestor != v.obligation)
    inform (loc, "derived from obligation %wu", v.ancestor);
}

namespace {
/* This walk never asks the emitter whether an instruction is "internal".
   Names, DECL_ARTIFICIAL and EMITTED are intentionally not consulted. */
struct inspection
{
  auto_vec<gimple *> statements;
  hash_set<gimple *> present;
  const char *reason = NULL;
  gimple *bad = NULL;
  unsigned HOST_WIDE_INT obligation = 0;
  bool refuse (gimple *s, const char *why, unsigned HOST_WIDE_INT id = 0)
  {
    if (!reason)
      {
        reason = why; bad = s; obligation = id;
        if (!obligation)
          for (tree p = anchor_ids (s); p; p = TREE_CHAIN (p))
            {
              unsigned HOST_WIDE_INT n = tree_to_uhwi (TREE_VALUE (p));
              if (!obligation || n < obligation) obligation = n;
            }
      }
    return false;
  }
};
static tree collect_actual (gimple_stmt_iterator *it, bool *, walk_stmt_info *wi)
{
  inspection &v = *(inspection *) wi->info;
  gimple *s = gsi_stmt (*it);
  v.statements.safe_push (s);
  if (v.present.add (s)) v.refuse (s, "V08: transaction instruction instance appears twice");
  if (gimple_code (s) == GIMPLE_BIND)
    for (tree d = gimple_bind_vars (as_a<gbind *> (s)); d; d = DECL_CHAIN (d))
      private_roots->put (d, d);
  return NULL_TREE;
}
static bool same (tree a, tree b)
{
  if (a == b) return true;
  if (!a || !b) return false;
  /* operand_equal_p compares expressions, not statement-class case labels.
     Snapshots intentionally unshare these trees: compare their actual range
     and destination rather than accepting or rejecting by tree identity. */
  if (TREE_CODE (a) == CASE_LABEL_EXPR && TREE_CODE (b) == CASE_LABEL_EXPR)
    return TREE_TYPE (a) == TREE_TYPE (b)
      && same (CASE_LOW (a), CASE_LOW (b)) && same (CASE_HIGH (a), CASE_HIGH (b))
      && CASE_LABEL (a) == CASE_LABEL (b) && CASE_CHAIN (a) == CASE_CHAIN (b);
  return operand_equal_p (a, b, 0);
}
static tree identity (tree r)
{ tree *p = identities ? identities->get (r) : NULL; return p ? *p : NULL_TREE; }
static bool automatic (tree t)
{
  return t && (VAR_P (t) || TREE_CODE (t) == PARM_DECL)
    && !TREE_STATIC (t) && !DECL_EXTERNAL (t)
    && DECL_CONTEXT (t) == live_owner
    && (live_scope_kind == NEWTM_OB_COPY || (private_roots && private_roots->get (t)));
}
/* Exact constant subobject interval. Dynamic program indexing uses its own
   checked identity, never this transport rule. Sizes here are bits. */
static bool private_interval (tree ref, bool write)
{
  HOST_WIDE_INT offset, size;
  bool reverse;
  tree base = get_ref_base_and_extent_hwi (ref, &offset, &size, &reverse);
  if (!base || offset < 0 || size < 0) return false;
  tree extent = NULL_TREE;
  if (automatic (base)) extent = DECL_SIZE (base);
  else if (TREE_CODE (base) == MEM_REF)
    {
      tree displacement = TREE_OPERAND (base, 1);
      if (!tree_fits_shwi_p (displacement) || tree_to_shwi (displacement) < 0)
        return false;
      tree bytes = newtm_clone_transport_extent (live_owner, TREE_OPERAND (base, 0), write);
      if (!bytes || !tree_fits_shwi_p (bytes)) return false;
      HOST_WIDE_INT d = tree_to_shwi (displacement), n = tree_to_shwi (bytes);
      if (d > n || offset / BITS_PER_UNIT > n - d) return false;
      return size <= (n - d) * BITS_PER_UNIT - offset;
    }
  return extent && tree_fits_shwi_p (extent) && offset <= tree_to_shwi (extent)
    && size <= tree_to_shwi (extent) - offset;
}
/* A dynamic direct access must be in the exact local arm of the emitted
   diamond. No alternate edge may enter that arm. General status/failure CFG
   proofs deliberately belong to step 6.4. */
static bool private_arm (inspection &v, unsigned at, tree storage)
{
  for (unsigned j = at; j > 0; --j)
    {
      gimple *s = v.statements[j - 1];
      if (gimple_code (s) != GIMPLE_LABEL) continue;
      tree label = gimple_label_label (as_a<glabel *> (s));
      if (j < 2) return false;
      gimple *branch = v.statements[j - 2];
      if (gimple_code (branch) != GIMPLE_COND) return false;
      gcond *c = as_a<gcond *> (branch);
      if (gimple_cond_code (c) != EQ_EXPR || !same (gimple_cond_lhs (c), storage)
          || !integer_onep (gimple_cond_rhs (c)) || gimple_cond_true_label (c) != label)
        return false;
      for (unsigned k = 0; k < v.statements.length (); ++k)
        {
          gimple *other = v.statements[k];
          if (other == branch) continue;
          if (gimple_code (other) == GIMPLE_GOTO
              && gimple_goto_dest (other) == label) return false;
          if (gimple_code (other) == GIMPLE_COND
              && (gimple_cond_true_label (as_a<gcond *> (other)) == label
                  || gimple_cond_false_label (as_a<gcond *> (other)) == label)) return false;
          if (gimple_code (other) == GIMPLE_SWITCH)
            for (unsigned n = 0; n < gimple_switch_num_labels (as_a<gswitch *> (other)); ++n)
              if (CASE_LABEL (gimple_switch_label (as_a<gswitch *> (other), n)) == label) return false;
        }
      for (unsigned k = j; k < at; ++k)
        if (gimple_code (v.statements[k]) != GIMPLE_ASSIGN) return false;
      return true;
    }
  return false;
}
static bool transport_tag (inspection &v, tree tag, unsigned depth = 0)
{
  if (!tag || depth > 24) return false;
  if (TREE_CODE (tag) == COMPONENT_REF
      && TREE_OPERAND (tag, 1) == DECL_CHAIN (TYPE_FIELDS (newtm_abi_clone_binding_type ())))
    return private_interval (tag, false);
  gimple *definition = NULL;
  for (gimple *s : v.statements)
    if (is_gimple_assign (s) && gimple_assign_lhs (s) == tag)
      { if (definition) return false; definition = s; }
  return definition && (gimple_assign_single_p (definition)
      || CONVERT_EXPR_CODE_P (gimple_assign_rhs_code (definition)))
    && transport_tag (v, gimple_assign_rhs1 (definition), depth + 1);
}
static bool private_object (inspection &v, tree object, unsigned depth = 0)
{
  if (!object || depth > 24) return false;
  STRIP_NOPS (object);
  if (TREE_CODE (object) == ADDR_EXPR)
    return automatic (get_base_address (TREE_OPERAND (object, 0)));
  gimple *definition = NULL;
  for (gimple *s : v.statements)
    if (is_gimple_assign (s) && gimple_assign_lhs (s) == object)
      { if (definition) return false; definition = s; }
  return definition && (gimple_assign_single_p (definition)
      || CONVERT_EXPR_CODE_P (gimple_assign_rhs_code (definition)))
    && private_object (v, gimple_assign_rhs1 (definition), depth + 1);
}
static bool source_private (inspection &v, unsigned at, tree ref, bool write)
{
  for (unsigned i = 0; i < vec_safe_length (records); ++i)
    {
      tree r = (*records)[i];
      if (value (r, SCOPE) != live_scope || value (r, KIND) != unsigned (write ? NEWTM_OB_WRITE : NEWTM_OB_READ)
          || value (r, EVIDENCE) == NEWTM_OB_INVALIDATED) continue;
      tree d = identity (r);
      if (!d || !same (TREE_VEC_ELT (d, NEWTM_D_LOGICAL), ref)) continue;
      tree storage = TREE_VEC_ELT (d, NEWTM_D_STORAGE);
      tree object = TREE_VEC_ELT (d, NEWTM_D_OBJECT);
      if (storage && integer_onep (storage) && private_object (v, object)) return true;
      if (storage && TREE_CODE (storage) != INTEGER_CST
          && transport_tag (v, storage)
          && private_arm (v, at, storage)) return true;
    }
  return false;
}
static bool check_memory (inspection &v, unsigned at, tree ref, bool write)
{
  if (!memory (ref)) return true;
  if (!write && live_scope_kind == NEWTM_OB_CONTROLLER
      && newtm_descriptor_reference_snapshot (v.statements[at]) == ref
      && (VAR_P (ref) || TREE_CODE (ref) == PARM_DECL) && TREE_CODE (TREE_TYPE (ref)) == REFERENCE_TYPE
      && is_gimple_assign (v.statements[at]) && gimple_assign_single_p (v.statements[at])
      && gimple_assign_rhs1 (v.statements[at]) == ref
      && types_compatible_p (TREE_TYPE (gimple_assign_lhs (v.statements[at])), TREE_TYPE (ref)))
    for (tree r : *records)
      if (value (r, SCOPE) == live_scope && value (r, KIND) == NEWTM_OB_CONTROL)
        if (tree helper = TREE_VEC_ELT (r, CALLEE))
          for (tree capture = newtm_clone_captures (helper); capture; capture = TREE_CHAIN (capture))
            if (TREE_VALUE (capture) == ref) return true;
  return private_interval (ref, write) || source_private (v, at, ref, write)
    || v.refuse (v.statements[at], write ? "V04: uncovered direct transaction write"
                                       : "V04: uncovered direct transaction read");
}
static bool signature (gimple *s, tree callee)
{
  tree formal = TYPE_ARG_TYPES (TREE_TYPE (callee));
  unsigned i = 0;
  for (; formal && TREE_VALUE (formal) != void_type_node; formal = TREE_CHAIN (formal), ++i)
    {
      if (i >= gimple_call_num_args (s)) return false;
      tree expected = TREE_VALUE (formal), actual = TREE_TYPE (gimple_call_arg (s, i));
      if (!types_compatible_p (expected, actual)
          && !(POINTER_TYPE_P (expected) && POINTER_TYPE_P (actual)
               && TYPE_MAIN_VARIANT (TREE_TYPE (expected)) == void_type_node)) return false;
    }
  tree lhs = gimple_call_lhs (s), result = TREE_TYPE (TREE_TYPE (callee));
  return formal && i == gimple_call_num_args (s)
    && (result == void_type_node ? !lhs : lhs && types_compatible_p (TREE_TYPE (lhs), result));
}
static bool matches_access (gimple *s, tree r, tree d)
{
  if (!d || gimple_call_num_args (s) != 5) return false;
  bool producer = false;
  for (tree ids = anchor_ids (s); ids; ids = TREE_CHAIN (ids))
    {
      unsigned HOST_WIDE_INT id = value (record_at (tree_to_uhwi (TREE_VALUE (ids))), PRODUCER);
      if (id && value (record_at (id), INSTANCE) == value (r, INSTANCE)) producer = true;
    }
  return producer && same (gimple_call_arg (s, 1), TREE_VEC_ELT (d, NEWTM_D_OBJECT))
    && same (gimple_call_arg (s, 2), TREE_VEC_ELT (d, NEWTM_D_OFFSET))
    && same (gimple_call_arg (s, 4), TREE_VEC_ELT (d, NEWTM_D_SIZE));
}
static bool call_allowed (inspection &v, unsigned at)
{
  gimple *s = v.statements[at];
  if (gimple_call_internal_p (s))
    {
      internal_fn f = gimple_call_internal_fn (s);
      tree lhs = gimple_call_lhs (s);
      bool typed = lhs && TREE_CODE (TREE_TYPE (lhs)) == COMPLEX_TYPE
        && INTEGRAL_TYPE_P (TREE_TYPE (TREE_TYPE (lhs))) && gimple_call_num_args (s) == 2;
      if (typed)
        for (unsigned i = 0; i < 2; ++i)
          /* Overflow IFNs explicitly support operands with different integer
             precisions/signedness and a separate integer result type. */
          typed &= INTEGRAL_TYPE_P (TREE_TYPE (gimple_call_arg (s, i)));
      return ((f == IFN_ADD_OVERFLOW || f == IFN_SUB_OVERFLOW || f == IFN_MUL_OVERFLOW)
              && typed)
        || v.refuse (s, "V02: unclassified internal transaction call");
    }
  tree callee = gimple_call_fndecl (s);
  if (!callee) return v.refuse (s, "V06: indirect transaction call remains");
  if (!signature (s, callee)) return v.refuse (s, "V06: incompatible transaction call signature");
  tree ctx = newtm_clone_context (live_owner);
  if (newtm_clone_original (callee))
    {
      if (ctx && !same (gimple_call_arg (s, 0), ctx))
        return v.refuse (s, "V06: prepared call uses a different transaction context");
      for (tree ids = anchor_ids (s); ids; ids = TREE_CHAIN (ids))
        {
          unsigned HOST_WIDE_INT id = value (record_at (tree_to_uhwi (TREE_VALUE (ids))), PRODUCER);
          tree source = record_at (id);
          if (!source || TREE_VEC_ELT (source, DECLARED_CALLEE) != callee) continue;
          if (live_scope_kind == NEWTM_OB_CONTROLLER && value (source, TREATMENT) == NEWTM_OB_DELEGATED)
            return true;
          tree *args = call_inputs->get (source);
          if (newtm_clone_for (TREE_VEC_ELT (source, CALLEE)) != callee || !args
              || gimple_call_num_args (s) < unsigned (TREE_VEC_LENGTH (*args)) + 2) continue;
          bool match = true;
          for (int n = 0; n < TREE_VEC_LENGTH (*args); ++n)
            match &= same (TREE_VEC_ELT (*args, n), gimple_call_arg (s, n + 2));
          if (match) return true;
        }
      return v.refuse (s, "V06: prepared call has no matching source or delegated region");
    }
  if (!fndecl_built_in_p (callee, BUILT_IN_NORMAL))
    return v.refuse (s, "V06: unprepared transaction call remains");
  if (ctx && gimple_call_num_args (s) && !same (gimple_call_arg (s, 0), ctx))
    return v.refuse (s, "V06: runtime call uses a different borrowed transaction context");
  switch (DECL_FUNCTION_CODE (callee))
    {
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_READ:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE:
      {
        bool matched = false;
        tree access_type = NULL_TREE;
        unsigned kind = DECL_FUNCTION_CODE (callee) == BUILT_IN_GOMP_TRANSACTION_STM_V1_READ
          ? NEWTM_OB_READ : NEWTM_OB_WRITE;
        for (tree r : *records)
          if (value (r, SCOPE) == live_scope && value (r, KIND) == kind
              && matches_access (s, r, identity (r)))
            {
              matched = true;
              access_type = TREE_TYPE (TREE_VEC_ELT (identity (r), NEWTM_D_LOGICAL));
            }
        if (!matched) return v.refuse (s, "V03: transaction runtime access has no matching source identity");
        tree buffer = gimple_call_arg (s, 3), bytes = gimple_call_arg (s, 4);
        /* Resolve only copies/casts with a unique actual definition. */
        for (unsigned depth = 0; depth < 32 && TREE_CODE (buffer) != ADDR_EXPR; ++depth)
          {
            tree next = NULL_TREE; unsigned definitions = 0;
            for (gimple *p : v.statements)
              if (is_gimple_assign (p) && gimple_assign_lhs (p) == buffer)
                {
                  ++definitions;
                  if (gimple_assign_single_p (p)
                      || CONVERT_EXPR_CODE_P (gimple_assign_rhs_code (p))) next = gimple_assign_rhs1 (p);
                }
            if (definitions != 1 || !next) break;
            buffer = next;
          }
        if (TREE_CODE (buffer) != ADDR_EXPR || !tree_fits_uhwi_p (bytes))
          return v.refuse (s, "V04: transaction access lacks a typed private buffer");
        tree object = TREE_OPERAND (buffer, 0), size = TYPE_SIZE_UNIT (TREE_TYPE (object));
        if (!automatic (object) || !size || !same (size, bytes)
            || !access_type || !types_compatible_p (TYPE_MAIN_VARIANT (TREE_TYPE (object)),
                                                    TYPE_MAIN_VARIANT (access_type)))
          return v.refuse (s, "V04: transaction access buffer has incompatible extent or lifetime");
        return true;
      }
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER:
      for (tree ids = anchor_ids (s); ids; ids = TREE_CHAIN (ids))
        {
          unsigned HOST_WIDE_INT id = value (record_at (tree_to_uhwi (TREE_VALUE (ids))), PRODUCER);
          tree source = record_at (id), d = source ? identity (source) : NULL_TREE;
          if (source && value (source, KIND) == NEWTM_OB_REGISTER && d
              && same (gimple_call_arg (s, 1), TREE_VEC_ELT (d, NEWTM_D_OBJECT))
              && same (gimple_call_arg (s, 2), TREE_VEC_ELT (d, NEWTM_D_EXTENT))
              && same (gimple_call_arg (s, 3), TREE_VEC_ELT (d, NEWTM_D_KIND))) return true;
        }
      return v.refuse (s, "V03: runtime registration has no matching registration identity");
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END: return true;
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_END:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_DISCARD:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL:
      return live_scope_kind == NEWTM_OB_CONTROLLER
        || v.refuse (s, "V07: slot-owning service in borrowed transaction frame");
    default: return v.refuse (s, "V02: unclassified builtin transaction call");
    }
}
static bool actual_shape (inspection &v, gimple *s)
{
  tree *p = shapes ? shapes->get (s) : NULL;
  if (!p) return v.refuse (s, "V08: transaction instruction has no current structural evidence");
  tree shape = *p;
  unsigned subcode = is_gimple_assign (s) ? unsigned (gimple_assign_rhs_code (s))
    : is_gimple_call (s) && gimple_call_internal_p (s) ? 1000 + unsigned (gimple_call_internal_fn (s)) : 0;
  if (tree_to_uhwi (TREE_VEC_ELT (shape, 0)) != unsigned (gimple_code (s))
      || tree_to_uhwi (TREE_VEC_ELT (shape, 1)) != subcode
      || TREE_VEC_LENGTH (shape) != int (gimple_num_ops (s) + 2))
    return v.refuse (s, "V08: transaction instruction shape changed without reconstruction");
  for (unsigned i = 0; i < gimple_num_ops (s); ++i)
    if (!same (TREE_VEC_ELT (shape, i + 2), gimple_op (s, i)))
      return v.refuse (s, "V08: transaction operand changed without reconstruction");
  for (tree ids = anchor_ids (s); ids; ids = TREE_CHAIN (ids))
    if (value (record_at (tree_to_uhwi (TREE_VALUE (ids))), EVIDENCE) == NEWTM_OB_INVALIDATED)
      return v.refuse (s, "V08: transaction operation has invalidated evidence");
  return true;
}
/* Compare a real runtime operation with the source identity captured before
   replacement. An emitter declaration alone cannot discharge this check. */
static bool replacement (inspection &v, tree r, tree d, built_in_function op)
{
  unsigned matches = 0;
  for (gimple *s : v.statements)
    {
      if (!is_gimple_call (s) || !gimple_call_fndecl (s)
          || !fndecl_built_in_p (gimple_call_fndecl (s), op)) continue;
      bool producer = false;
      for (tree ids = anchor_ids (s); ids; ids = TREE_CHAIN (ids))
        {
          tree emitted = record_at (tree_to_uhwi (TREE_VALUE (ids)));
          unsigned HOST_WIDE_INT id = value (emitted, PRODUCER);
          if (id && value (record_at (id), INSTANCE) == value (r, INSTANCE)) producer = true;
        }
      if (!producer) continue;
      if (!d) return false;
      if (!same (gimple_call_arg (s, 1), TREE_VEC_ELT (d, NEWTM_D_OBJECT))) continue;
      if (op == BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER)
        {
          if (same (gimple_call_arg (s, 2), TREE_VEC_ELT (d, NEWTM_D_EXTENT))
              && same (gimple_call_arg (s, 3), TREE_VEC_ELT (d, NEWTM_D_KIND))) ++matches;
        }
      else if (same (gimple_call_arg (s, 2), TREE_VEC_ELT (d, NEWTM_D_OFFSET))
               && same (gimple_call_arg (s, 4), TREE_VEC_ELT (d, NEWTM_D_SIZE))) ++matches;
    }
  return matches == 1;
}
} // namespace

namespace {
static tree guard_binary (tree_code code, tree a, tree b)
{ return fold_build2 (code, boolean_type_node, a, b); }
static tree guard_and (tree a, tree b)
{ return guard_binary (BIT_AND_EXPR, a, b); }
static tree guard_range (tree d)
{
  const unsigned needed[] = { NEWTM_D_OBJECT, NEWTM_D_EXTENT, NEWTM_D_OFFSET,
    NEWTM_D_SIZE, NEWTM_D_KIND, NEWTM_D_STORAGE, NEWTM_D_LOCAL_VALID };
  for (unsigned field : needed) if (!TREE_VEC_ELT (d, field)) return NULL_TREE;
  tree object = TREE_VEC_ELT (d, NEWTM_D_OBJECT), extent = TREE_VEC_ELT (d, NEWTM_D_EXTENT);
  tree offset = TREE_VEC_ELT (d, NEWTM_D_OFFSET), size = TREE_VEC_ELT (d, NEWTM_D_SIZE);
  tree kind = TREE_VEC_ELT (d, NEWTM_D_KIND), storage = TREE_VEC_ELT (d, NEWTM_D_STORAGE);
  tree ptruint = unsigned_type_for (pointer_sized_int_node);
  tree wide = build_nonstandard_integer_type (MAX (TYPE_PRECISION (size_type_node), TYPE_PRECISION (ptruint)), true);
  tree address = fold_convert (wide, fold_convert (ptruint, object));
  tree left = fold_build2 (MINUS_EXPR, wide, fold_convert (wide, TYPE_MAX_VALUE (ptruint)), address);
  tree span = fold_build2 (MINUS_EXPR, size_type_node,
    fold_build2 (MAX_EXPR, size_type_node, extent, size_one_node), size_one_node);
  tree bounded = fold_build2 (MIN_EXPR, size_type_node, offset, extent);
  tree room = fold_build2 (MINUS_EXPR, size_type_node, extent, bounded);
  tree result = TREE_VEC_ELT (d, NEWTM_D_LOCAL_VALID);
  result = guard_and (result, guard_binary (NE_EXPR, object, build_zero_cst (TREE_TYPE (object))));
  result = guard_and (result, guard_binary (NE_EXPR, extent, size_zero_node));
  result = guard_and (result, guard_binary (LE_EXPR, fold_convert (wide, span), left));
  result = guard_and (result, guard_binary (LE_EXPR, fold_convert (wide, offset), left));
  result = guard_and (result, guard_binary (LE_EXPR, offset, extent));
  result = guard_and (result, guard_binary (LE_EXPR, size, room));
  result = guard_and (result, guard_binary (LE_EXPR, kind, build_int_cst (TREE_TYPE (kind), NEWTM_ABI_AGGREGATE)));
  result = guard_and (result, guard_binary (LE_EXPR, storage, build_int_cst (TREE_TYPE (storage), NEWTM_STORAGE_PRIVATE)));
  if (!integer_zerop (size))
    result = guard_and (result, guard_binary (BIT_IOR_EXPR,
      guard_binary (NE_EXPR, kind, build_zero_cst (TREE_TYPE (kind))),
      guard_and (guard_binary (EQ_EXPR, offset, size_zero_node), guard_binary (EQ_EXPR, size, extent))));
  return result;
}
static bool belongs_to (gimple *s, tree r)
{
  if (tree *p = emission_sources->get (s))
    if (unsigned HOST_WIDE_INT source = tree_to_uhwi (*p))
      if (value (record_at (source), INSTANCE) == value (r, INSTANCE)) return true;
  for (tree p = anchor_ids (s); p; p = TREE_CHAIN (p))
    if (value (record_at (tree_to_uhwi (TREE_VALUE (p))), INSTANCE) == value (r, INSTANCE)) return true;
  return false;
}
static bool require_identity (newtm_guard_analysis &guards, gimple *s, tree d,
                              const char **why, gimple **bad, bool logical)
{
  if (!guards.require (s, guard_range (d), why, bad)
      || !guards.overflow_dependencies (s, TREE_VEC_ELT (d, NEWTM_D_OFFSET), why, bad)) return false;
  if (logical)
    {
      tree ref = TREE_VEC_ELT (d, NEWTM_D_LOGICAL);
      if (!ref) return false;
      tree ptruint = unsigned_type_for (pointer_sized_int_node);
      tree wide = build_nonstandard_integer_type (MAX (TYPE_PRECISION (size_type_node), TYPE_PRECISION (ptruint)), true);
      tree actual = fold_convert (wide, fold_convert (ptruint, build_fold_addr_expr (unshare_expr (ref))));
      tree canonical = fold_build2 (PLUS_EXPR, wide,
        fold_convert (wide, fold_convert (ptruint, TREE_VEC_ELT (d, NEWTM_D_OBJECT))),
        fold_convert (wide, TREE_VEC_ELT (d, NEWTM_D_OFFSET)));
      if (!guards.require (s, guard_binary (EQ_EXPR, actual, canonical), why, bad)) return false;
    }
  return true;
}
static bool require_indices (newtm_guard_analysis &guards, gimple *s, tree ref, bool one_past,
                              const char **why, gimple **bad)
{
  if (!ref || !REFERENCE_CLASS_P (ref)) return true;
  if (TREE_CODE (ref) == ARRAY_REF)
    {
      tree index = TREE_OPERAND (ref, 1), type = TREE_TYPE (TREE_OPERAND (ref, 0));
      tree bytes = TYPE_SIZE_UNIT (type), element = TYPE_SIZE_UNIT (TREE_TYPE (ref));
      if (!bytes || !element || !INTEGRAL_TYPE_P (TREE_TYPE (index)) || integer_zerop (element)) return false;
      if (!TYPE_UNSIGNED (TREE_TYPE (index)))
        if (!guards.require (s, guard_binary (GE_EXPR, index, build_zero_cst (TREE_TYPE (index))), why, bad)) return false;
      if (TYPE_PRECISION (TREE_TYPE (index)) > TYPE_PRECISION (size_type_node))
        if (!guards.require (s, guard_binary (LE_EXPR, index, fold_convert (TREE_TYPE (index), TYPE_MAX_VALUE (size_type_node))), why, bad)) return false;
      tree count = fold_build2 (TRUNC_DIV_EXPR, size_type_node, fold_convert (size_type_node, bytes), fold_convert (size_type_node, element));
      if (!guards.require (s, guard_binary (one_past ? LE_EXPR : LT_EXPR, fold_convert (size_type_node, index), count), why, bad)) return false;
      return require_indices (guards, s, TREE_OPERAND (ref, 0), false, why, bad);
    }
  if (TREE_CODE (ref) == COMPONENT_REF) return require_indices (guards, s, TREE_OPERAND (ref, 0), false, why, bad);
  return true;
}
static bool harmless_address (tree ref)
{
  if (DECL_P (ref) || TREE_CODE (ref) == STRING_CST) return true;
  if (TREE_CODE (ref) == COMPONENT_REF) return harmless_address (TREE_OPERAND (ref, 0));
  if (TREE_CODE (ref) == ARRAY_REF)
    {
      tree index = TREE_OPERAND (ref, 1), type = TREE_TYPE (TREE_OPERAND (ref, 0));
      tree size = TYPE_SIZE_UNIT (TREE_TYPE (ref)), extent = TYPE_SIZE_UNIT (type);
      if (TREE_CODE (type) != ARRAY_TYPE || !size || !extent || !tree_fits_uhwi_p (size)
          || !tree_fits_uhwi_p (extent) || !tree_fits_uhwi_p (index) || !tree_to_uhwi (size)
          || !integer_zerop (array_ref_low_bound (ref))) return false;
      return tree_to_uhwi (index) <= tree_to_uhwi (extent) / tree_to_uhwi (size)
        && harmless_address (TREE_OPERAND (ref, 0));
    }
  return false;
}
static bool inspect_guards (inspection &v, const newtm_flow_graph &flow)
{
  newtm_guard_analysis guards (flow);
  const char *why = NULL; gimple *bad = NULL;
  for (gimple *s : v.statements)
    {
      if (math_obligations->get (s) && !guards.arithmetic (s, &why, &bad)) return v.refuse (bad, why);
      /* Borrowed ABI buffers must be nonnull before even their first field
         access, independently of any later validation of their contents. */
      if (is_gimple_assign (s))
        for (unsigned op = 0; op < gimple_num_ops (s); ++op)
          if (tree ref = gimple_op (s, op))
            if (REFERENCE_CLASS_P (ref))
              {
                tree base = get_base_address (ref);
                if (base && TREE_CODE (base) == MEM_REF)
                  {
                    tree pointer = TREE_OPERAND (base, 0);
                    if (newtm_clone_transport_extent (live_owner, pointer, op == 0))
                      if (!guards.require (s, guard_binary (NE_EXPR, pointer, build_zero_cst (TREE_TYPE (pointer))), &why, &bad))
                        return v.refuse (bad, why);
                  }
              }
      for (tree r : *records)
        {
          if (value (r, SCOPE) != live_scope || value (r, STATE) == NEWTM_OB_REJECTED) continue;
          tree d = identity (r); if (!d || !TREE_VEC_ELT (d, NEWTM_D_LOGICAL)) continue;
          unsigned kind = value (r, KIND); int role = int (value (r, ROLE)) - 1;
          bool use = false, logical = false;
          if ((kind == NEWTM_OB_READ || kind == NEWTM_OB_WRITE) && is_gimple_call (s)
              && gimple_call_fndecl (s)
              && fndecl_built_in_p (gimple_call_fndecl (s), kind == NEWTM_OB_READ
                   ? BUILT_IN_GOMP_TRANSACTION_STM_V1_READ : BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE)
              && matches_access (s, r, d)) use = logical = true;
          if (is_gimple_assign (s) && belongs_to (s, r) && (kind == NEWTM_OB_READ || kind == NEWTM_OB_WRITE))
            {
              tree ref = kind == NEWTM_OB_WRITE ? gimple_assign_lhs (s) : gimple_assign_rhs1 (s);
              if (same (ref, TREE_VEC_ELT (d, NEWTM_D_LOGICAL))) use = logical = true;
              tree address = build_fold_addr_expr (unshare_expr (TREE_VEC_ELT (d, NEWTM_D_LOGICAL)));
              if (TREE_CODE (gimple_assign_rhs1 (s)) == ADDR_EXPR && same (gimple_assign_rhs1 (s), address))
                {
                  tree ref = TREE_OPERAND (gimple_assign_rhs1 (s), 0);
                  if (!harmless_address (ref)) use = true;
                }
            }
          if (kind == NEWTM_OB_REGISTER && is_gimple_call (s) && belongs_to (s, r)
              && gimple_call_fndecl (s) && fndecl_built_in_p (gimple_call_fndecl (s), BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER)) use = true;
          if (kind == NEWTM_OB_IDENTITY && role == NEWTM_DESC_ADDRESS && is_gimple_assign (s) && belongs_to (s, r)
              && (gimple_assign_rhs_code (s) == POINTER_PLUS_EXPR
                  || TREE_CODE (gimple_assign_rhs1 (s)) == ADDR_EXPR)) use = true;
          if (kind == NEWTM_OB_IDENTITY && role == NEWTM_DESC_ARGUMENT && is_gimple_assign (s)
              && belongs_to (s, r) && TREE_CODE (gimple_assign_lhs (s)) == COMPONENT_REF)
            {
              tree field = TREE_OPERAND (gimple_assign_lhs (s), 1);
              tree type = DECL_CONTEXT (field);
              if (type == newtm_abi_binding_type () || type == newtm_abi_clone_binding_type ())
                {
                  unsigned slot = 0;
                  for (tree other : *records)
                    if (value (other, SCOPE) == live_scope && value (other, INSTANCE) == value (r, INSTANCE)
                        && value (other, KIND) == NEWTM_OB_IDENTITY
                        && int (value (other, ROLE)) - 1 == NEWTM_DESC_ARGUMENT
                        && value (other, OPERAND) < value (r, OPERAND)) ++slot;
                  tree base = gimple_assign_lhs (s);
                  while (TREE_CODE (base) == COMPONENT_REF) base = TREE_OPERAND (base, 0);
                  if (TREE_CODE (base) == ARRAY_REF)
                    { tree index = TREE_OPERAND (base, 1);
                      if (!tree_fits_uhwi_p (index) || tree_to_uhwi (index) != slot) continue; }
                  else if (slot) continue;
                  const unsigned fields[] = { NEWTM_D_OBJECT, NEWTM_D_EXTENT, NEWTM_D_OFFSET, NEWTM_D_KIND, NEWTM_D_STORAGE };
                  for (unsigned f : fields)
                    if (same (gimple_assign_rhs1 (s), TREE_VEC_ELT (d, f))) use = true;
                }
            }
          if (use && !require_identity (guards, s, d, &why, &bad, logical))
            return v.refuse (bad ? bad : s, why ? why : "V03: unresolved logical address guard");
          if (use)
            {
              tree ref = TREE_VEC_ELT (d, NEWTM_D_LOGICAL);
              if (role == NEWTM_DESC_ADDRESS && is_gimple_assign (s) && TREE_CODE (gimple_assign_rhs1 (s)) == ADDR_EXPR)
                ref = TREE_OPERAND (gimple_assign_rhs1 (s), 0);
              if (!require_indices (guards, s, ref, role == NEWTM_DESC_ADDRESS, &why, &bad))
                return v.refuse (bad ? bad : s, why ? why : "V03: unresolved evaluated array index");
            }
        }
    }
  return true;
}
/* Output pointers are read from actual call operands. Resolve only one
   unescaped address definition which dominates every instance of the call. */
static bool status_dominates (const newtm_flow_graph &flow, gimple *def, gimple *call)
{
  auto_vec<unsigned char> seen; seen.safe_grow_cleared (flow.node_count ());
  auto_vec<unsigned> work; work.safe_push (0); seen[0] = 1;
  for (unsigned at = 0; at < work.length (); ++at)
    {
      unsigned id = work[at]; gimple *s = flow.node (id).statement;
      if (s == def) continue;
      if (s == call) return false;
      for (const newtm_flow_edge &edge : flow.node (id).successors)
        if (!seen[edge.target]) { seen[edge.target] = 1; work.safe_push (edge.target); }
    }
  return true;
}
static tree status_output (inspection &v, const newtm_flow_graph &flow,
                           gimple *call, unsigned arg, tree expected)
{
  tree pointer = gimple_call_arg (call, arg);
  for (unsigned depth = 0; depth < 32 && TREE_CODE (pointer) != ADDR_EXPR; ++depth)
    {
      if (CONVERT_EXPR_CODE_P (TREE_CODE (pointer)))
        { pointer = TREE_OPERAND (pointer, 0); continue; }
      if (DECL_P (pointer) && TREE_ADDRESSABLE (pointer)) return NULL_TREE;
      gimple *definition = NULL;
      for (gimple *s : v.statements)
        if (is_gimple_assign (s) && gimple_assign_lhs (s) == pointer)
          { if (definition) return NULL_TREE; definition = s; }
      if (!definition || !(gimple_assign_single_p (definition)
          || CONVERT_EXPR_CODE_P (gimple_assign_rhs_code (definition)))
          || !status_dominates (flow, definition, call)) return NULL_TREE;
      pointer = gimple_assign_rhs1 (definition);
    }
  if (TREE_CODE (pointer) != ADDR_EXPR) return NULL_TREE;
  tree object = TREE_OPERAND (pointer, 0);
  if (!automatic (object) || (expected && !types_compatible_p (
      TYPE_MAIN_VARIANT (TREE_TYPE (object)), TYPE_MAIN_VARIANT (expected)))) return NULL_TREE;
  return object;
}
static bool inspect_status (inspection &v, const newtm_flow_graph &flow)
{
  newtm_status_analysis status (flow);
  for (gimple *s : v.statements)
    if (is_gimple_call (s) && !gimple_call_internal_p (s))
      {
        tree callee = gimple_call_fndecl (s), result = NULL_TREE, binding = NULL_TREE;
        bool checked = false;
        if (tree original = newtm_clone_original (callee))
          {
            checked = true;
            tree type = TREE_TYPE (TREE_TYPE (original));
            unsigned args = gimple_call_num_args (s);
            if (TREE_CODE (type) != VOID_TYPE)
              {
                bool pointer = TREE_CODE (type) == POINTER_TYPE;
                result = status_output (v, flow, s, args - (pointer ? 2 : 1), type);
                if (!result) return v.refuse (s, "V05: prepared result lacks an unambiguous typed output object");
                if (pointer)
                  {
                    binding = status_output (v, flow, s, args - 1, newtm_abi_clone_binding_type ());
                    if (!binding || result == binding)
                      return v.refuse (s, "V05: prepared binding lacks a distinct typed output object");
                  }
              }
          }
        else if (callee && fndecl_built_in_p (callee, BUILT_IN_NORMAL))
          switch (DECL_FUNCTION_CODE (callee))
            {
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_READ:
              result = status_output (v, flow, s, 3, NULL_TREE);
              if (!result) return v.refuse (s, "V05: read lacks an unambiguous output object");
              checked = true; break;
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN:
              result = status_output (v, flow, s, 1, uint32_type_node);
              if (!result) return v.refuse (s, "V05: begin lacks an unambiguous outermost output object");
              checked = true; break;
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE:
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER:
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS:
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN:
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END:
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_END: checked = true; break;
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE:
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_DISCARD: status.reset (s); break;
            default: break;
            }
        if (checked) status.operation (s, result, binding);
        if (result && callee && fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_READ))
          for (tree r : *records)
            if (value (r, SCOPE) == live_scope && value (r, KIND) == NEWTM_OB_READ
                && matches_access (s, r, identity (r)))
              for (unsigned at = 0; at < v.statements.length (); ++at)
                {
                  gimple *store = v.statements[at];
                  if (is_gimple_assign (store) && gimple_assign_lhs (store) == result
                      && belongs_to (store, r))
                    {
                      gimple *load = store; unsigned position = at;
                      for (unsigned depth = 0; depth < 32; ++depth)
                        {
                          tree rhs = gimple_assign_rhs1 (load);
                          if (same (rhs, TREE_VEC_ELT (identity (r), NEWTM_D_LOGICAL)))
                            {
                              if (source_private (v, position, rhs, false)) status.alternative (s, store);
                              break;
                            }
                          if (!gimple_assign_single_p (load) || !VAR_P (rhs) || TREE_ADDRESSABLE (rhs)) break;
                          gimple *definition = NULL; unsigned found = 0;
                          for (unsigned j = 0; j < v.statements.length (); ++j)
                            if (is_gimple_assign (v.statements[j]) && gimple_assign_lhs (v.statements[j]) == rhs)
                              { definition = v.statements[j]; position = j; ++found; }
                          if (found != 1 || !belongs_to (definition, r)
                              || !status_dominates (flow, definition, load)) break;
                          load = definition;
                        }
                    }
                }
      }
  const char *why = NULL; gimple *bad = NULL;
  for (gimple *store : v.statements)
    if (is_gimple_assign (store))
      {
        tree base = get_base_address (gimple_assign_lhs (store));
        if (!base || TREE_CODE (base) != MEM_REF) continue;
        tree pointer = TREE_OPERAND (base, 0);
        if (!pointer || (pointer != newtm_clone_channel (live_owner, 3)
                         && pointer != newtm_clone_channel (live_owner, 4))) continue;
        tree *source = emission_sources->get (store);
        tree publication = source ? record_at (tree_to_uhwi (*source)) : NULL_TREE;
        if (!publication || value (publication, KIND) != NEWTM_OB_PUBLISH
            || value (publication, SCOPE) != live_scope)
          return v.refuse (store, "V05: output store has no current publication instance");
        gimple *last = NULL;
        for (gimple *call : v.statements)
          if (is_gimple_call (call) && gimple_call_fndecl (call)
              && fndecl_built_in_p (gimple_call_fndecl (call), BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS)
              && belongs_to (call, publication)
              && status_dominates (flow, call, store)
              && (!last || status_dominates (flow, last, call))) last = call;
        if (!last) return v.refuse (store, "V05: output publication has no dominating final status operation");
        status.require_success (last, store);
      }
  return status.verify (&why, &bad) || v.refuse (bad, why);
}
static tree lifetime_reference (inspection &v, const newtm_flow_graph &flow, gimple *call, unsigned arg)
{
  tree p = gimple_call_arg (call, arg);
  for (unsigned depth = 0; depth < 32; ++depth)
    {
      if (TREE_CODE (p) == ADDR_EXPR) return TREE_OPERAND (p, 0);
      if (CONVERT_EXPR_CODE_P (TREE_CODE (p))) { p = TREE_OPERAND (p, 0); continue; }
      gimple *d = NULL;
      for (gimple *s : v.statements)
        if (is_gimple_assign (s) && gimple_assign_lhs (s) == p)
          { if (d) return NULL_TREE; d = s; }
      if (!d || !(gimple_assign_single_p (d) || gimple_assign_cast_p (d))
          || (DECL_P (p) && TREE_ADDRESSABLE (p)) || !status_dominates (flow, d, call)) return NULL_TREE;
      p = gimple_assign_rhs1 (d);
    }
  return NULL_TREE;
}
static tree publication_field (inspection &v, const newtm_flow_graph &flow,
                               gimple *use, tree ref)
{
  gimple *definition = NULL;
  for (gimple *s : v.statements)
    if (is_gimple_assign (s) && same (gimple_assign_lhs (s), ref))
      { if (definition) return NULL_TREE; definition = s; }
  return definition && status_dominates (flow, definition, use)
    && gimple_assign_single_p (definition) ? gimple_assign_rhs1 (definition) : NULL_TREE;
}
static bool inspect_lifetime (inspection &v, const newtm_flow_graph &flow)
{
  newtm_lifetime_analysis life (flow, live_owner);
  tree out = newtm_clone_channel (live_owner, 3), bound = newtm_clone_channel (live_owner, 4);
  life.publication (out, bound);
  for (unsigned channel = 2; channel <= 4; ++channel)
    if (tree p = newtm_clone_channel (live_owner, channel)) life.borrowed (p);
  for (unsigned scope = 1; scope < flow.scope_count (); ++scope)
    if (flow.scope (scope).kind == NEWTM_FLOW_BIND)
      for (tree d = gimple_bind_vars (as_a<gbind *> (flow.scope (scope).owner)); d; d = DECL_CHAIN (d))
        {
          tree type = TREE_TYPE (d);
          while (TREE_CODE (type) == ARRAY_TYPE) type = TREE_TYPE (type);
          if (type == newtm_abi_clone_binding_type () || type == newtm_abi_binding_type ()) life.watch (d);
        }
  for (gimple *s : v.statements)
    if (is_gimple_assign (s) && (newtm_descriptor_math_p (s) || math_obligations->get (s)))
      {
        if (VAR_P (gimple_assign_lhs (s))) life.watch (gimple_assign_lhs (s));
        if (gimple_assign_rhs_code (s) == REALPART_EXPR || gimple_assign_rhs_code (s) == IMAGPART_EXPR)
          if (tree p = get_base_address (gimple_assign_rhs1 (s))) if (VAR_P (p)) life.watch (p);
      }
  for (gimple *s : v.statements)
    if (is_gimple_call (s) && !gimple_call_internal_p (s))
      {
        tree callee = gimple_call_fndecl (s);
        if (tree original = newtm_clone_original (callee))
          {
            tree formal = DECL_CHAIN (DECL_ARGUMENTS (callee));
            tree bytes = newtm_clone_transport_extent (callee, formal, false);
            if (!bytes || !tree_fits_uhwi_p (bytes)) return v.refuse (s, "V04: prepared incoming transport has no finite signature extent");
            if (!integer_zerop (bytes))
              {
                tree ref = lifetime_reference (v, flow, s, 1);
                if (!ref) return v.refuse (s, "V04: incoming binding buffer has no dominating local address");
                life.watch (get_base_address (ref)); life.input (s, ref, tree_to_uhwi (bytes)); life.borrow (s, 1);
              }
            tree type = TREE_TYPE (TREE_TYPE (original));
            unsigned count = gimple_call_num_args (s);
            unsigned end = count - (TREE_CODE (type) == VOID_TYPE ? 0 : TREE_CODE (type) == POINTER_TYPE ? 2 : 1);
            life.effects (s, 2, end);
            unsigned capture_arg = 2;
            for (tree capture = newtm_clone_captures (callee); capture; capture = TREE_CHAIN (capture), ++capture_arg)
              if (newtm_clone_region_owner (callee) && TREE_CODE (TREE_TYPE (TREE_VALUE (capture))) == REFERENCE_TYPE)
                {
                  tree ref = lifetime_reference (v, flow, s, capture_arg);
                  if (!ref) return v.refuse (s, "V04: reference capture has no initialized representation buffer");
                  life.watch (get_base_address (ref)); life.input (s, ref); life.borrow (s, capture_arg);
                }
            if (TREE_CODE (type) != VOID_TYPE)
              {
                bool pointer = TREE_CODE (type) == POINTER_TYPE;
                tree result = status_output (v, flow, s, end, type);
                life.output (s, result, pointer, false, end); life.borrow (s, end);
                if (pointer)
                  {
                    tree binding = status_output (v, flow, s, end + 1, newtm_abi_clone_binding_type ());
                    life.output (s, binding, false, true, end); life.borrow (s, end + 1);
                  }
              }
          }
        else if (callee && fndecl_built_in_p (callee, BUILT_IN_NORMAL))
          switch (DECL_FUNCTION_CODE (callee))
            {
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE:
              {
                tree ref = status_output (v, flow, s, 3, NULL_TREE);
                life.watch (ref); life.input (s, ref); life.borrow (s, 3); break;
              }
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_READ:
              life.output (s, status_output (v, flow, s, 3, NULL_TREE)); life.borrow (s, 3); break;
            case BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN:
              life.output (s, status_output (v, flow, s, 1, uint32_type_node)); life.borrow (s, 1); break;
            default: break;
            }
      }
  /* Confront the value with the actual fields stored into the published
     binding, not with a descriptor approval or a buffer's spelling. */
  if (bound)
    {
      newtm_guard_analysis guards (flow);
      for (gimple *s : v.statements)
        if (is_gimple_assign (s) && TREE_CODE (gimple_assign_lhs (s)) == MEM_REF
            && TREE_OPERAND (gimple_assign_lhs (s), 0) == bound)
          {
            tree buffer = gimple_assign_rhs1 (s);
            if (TREE_TYPE (buffer) != newtm_abi_clone_binding_type ())
              return v.refuse (s, "V05: output binding publication has an incompatible value type");
            tree object = publication_field (v, flow, s, newtm_clone_field (newtm_clone_identity (buffer), 0));
            tree offset = publication_field (v, flow, s, newtm_clone_field (newtm_clone_identity (buffer), 2));
            gimple *store = NULL;
            for (gimple *other : v.statements)
              if (is_gimple_assign (other) && TREE_CODE (gimple_assign_lhs (other)) == MEM_REF
                  && TREE_OPERAND (gimple_assign_lhs (other), 0) == out && status_dominates (flow, s, other))
                { if (store) { store = NULL; break; } store = other; }
            if (!object || !offset || !store) return v.refuse (s, "V05: pointer publication lacks a unique paired value and binding definition");
            tree integer = unsigned_type_for (pointer_sized_int_node);
            tree canonical = fold_build2 (PLUS_EXPR, integer, fold_convert (integer, object), fold_convert (integer, offset));
            tree needed = guard_binary (EQ_EXPR, fold_convert (integer, gimple_assign_rhs1 (store)), canonical);
            const char *why = NULL; gimple *bad = NULL;
            if (!guards.require (s, needed, &why, &bad)) return v.refuse (bad ? bad : s, "V05: published pointer disagrees with the actual binding fields");
          }
    }
  const char *why = NULL; gimple *bad = NULL;
  return life.verify (&why, &bad) || v.refuse (bad, why);
}
static bool inspect_failure (inspection &v, const newtm_flow_graph &flow)
{
  for (auto it = abort_edges->begin (); it != abort_edges->end (); ++it)
    {
      gimple *g = (*it).first; tree label = (*it).second;
      if (!flow.contains (g)) return v.refuse (g, "V01: required abort edge disappeared from the live body");
      if (gimple_code (g) != GIMPLE_COND || gimple_cond_false_label (as_a<gcond *> (g)) != label)
        return v.refuse (g, "V08: required abort edge was redirected without reconstruction");
    }
  const char *why = NULL; gimple *bad = NULL;
  if (live_scope_kind == NEWTM_OB_CONTROLLER)
    {
      tree helper = NULL_TREE;
      for (tree r : *records)
        if (value (r, SCOPE) == live_scope && value (r, TREATMENT) == NEWTM_OB_DELEGATED
            && TREE_VEC_ELT (r, DECLARED_CALLEE))
          {
            tree h = TREE_VEC_ELT (r, DECLARED_CALLEE);
            if (helper && helper != h) return v.refuse (NULL, "V08: controller has inconsistent delegated helper obligations");
            helper = h;
          }
      newtm_owner_analysis owner (flow, helper);
      for (auto it = abort_edges->begin (); it != abort_edges->end (); ++it) owner.invalid ((*it).first);
      return owner.verify (&why, &bad) || v.refuse (bad, why);
    }
  tree context = newtm_clone_context (live_owner);
  if (!context) return true;
  newtm_failure_analysis failure (flow, context);
  for (auto it = abort_edges->begin (); it != abort_edges->end (); ++it) failure.invalid ((*it).first);
  return failure.verify (&why, &bad) || v.refuse (bad, why);
}
/* Reconcile every inventoried instance, including unbound and retired ones.
   A producer annotation identifies a candidate; actual category/operands and
   the independent proofs above must still demonstrate its replacement. */
static bool complete_inventory (inspection &v)
{
  if (live_reason) return true; /* Structurally inspected refused stub. */
  hash_set<tree> actual;
  for (gimple *g : v.statements)
    for (tree p = anchor_ids (g); p; p = TREE_CHAIN (p)) actual.add (record_at (tree_to_uhwi (TREE_VALUE (p))));
  for (unsigned i = 0; i < vec_safe_length (records); ++i)
    {
      tree r = (*records)[i]; if (value (r, SCOPE) != live_scope) continue;
      if (value (r, GENERATION) != live_generation || value (r, EVIDENCE) == NEWTM_OB_INVALIDATED
          || value (r, STATE) == NEWTM_OB_REJECTED)
        return v.refuse (NULL, "V08: obligation has no evidence in the current scope and generation", i + 1);
      for (tree p = TREE_VEC_ELT (r, CHILDREN); p; p = TREE_CHAIN (p))
        {
          tree c = record_at (tree_to_uhwi (TREE_VALUE (p)));
          if (!c || value (c, PARENT) != i + 1 || value (c, SCOPE) != live_scope
              || value (c, GENERATION) != live_generation)
            return v.refuse (NULL, "V08: derived obligation belongs to a different scope or generation", i + 1);
        }
      if (actual.contains (r) || TREE_VEC_ELT (r, CHILDREN)) continue;
      tree descriptor = identity (r);
      if (value (r, KIND) == NEWTM_OB_REGISTER && descriptor
          && TREE_VEC_ELT (descriptor, NEWTM_D_STORAGE)
          && integer_onep (TREE_VEC_ELT (descriptor, NEWTM_D_STORAGE))
          && private_object (v, TREE_VEC_ELT (descriptor, NEWTM_D_OBJECT))) continue;
      if (value (r, STATE) == NEWTM_OB_DERIVED)
        return v.refuse (NULL, "V01: derived obligation has no children", i + 1);
      if (live_scope_kind == NEWTM_OB_CONTROLLER && value (r, TREATMENT) == NEWTM_OB_DELEGATED) continue;
      bool found = false;
      for (gimple *g : v.statements)
        for (tree ids = anchor_ids (g); ids && !found; ids = TREE_CHAIN (ids))
          {
            tree a = record_at (tree_to_uhwi (TREE_VALUE (ids)));
            tree source = record_at (value (a, PRODUCER));
            bool related = false;
            auto_vec<tree> work; hash_set<tree> seen;
            if (source) work.safe_push (source);
            while (!work.is_empty () && !related)
              {
                tree p = work.pop (); if (seen.add (p)) continue;
                related = value (p, INSTANCE) == value (r, INSTANCE);
                if (tree next = record_at (value (p, PARENT))) work.safe_push (next);
                if (tree next = record_at (value (p, PRODUCER))) work.safe_push (next);
              }
            if (!related) continue;
            unsigned kind = value (r, KIND);
            if (kind == NEWTM_OB_READ || kind == NEWTM_OB_WRITE || kind == NEWTM_OB_REGISTER)
              {
                tree *from = references->get (r), *to = references->get (a);
                if (kind == value (a, KIND) && from && to && same (*from, *to)) found = true;
                built_in_function op = kind == NEWTM_OB_READ ? BUILT_IN_GOMP_TRANSACTION_STM_V1_READ
                  : kind == NEWTM_OB_WRITE ? BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE : BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER;
                tree callee = is_gimple_call (g) && !gimple_call_internal_p (g) ? gimple_call_fndecl (g) : NULL_TREE;
                if (!found && identity (r) && callee
                    && fndecl_built_in_p (callee, op)) found = replacement (v, r, identity (r), op);
              }
            else
              {
                found = kind == value (a, KIND);
                /* A publication marker lowers to a checked final STATUS and
                   typed stores, not another publication pseudo-call. The
                   lifetime/status proofs check its own instance and outputs. */
                tree callee = is_gimple_call (g) && !gimple_call_internal_p (g) ? gimple_call_fndecl (g) : NULL_TREE;
                if (kind == NEWTM_OB_PUBLISH && value (r, TREATMENT) == NEWTM_OB_PUBLICATION && callee
                    && fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS)) found = true;
                if (value (r, EXPECTED_SERVICE))
                  found = callee && fndecl_built_in_p (callee, (built_in_function) value (r, EXPECTED_SERVICE));
              }
          }
      if (!found)
        {
          if (flag_transaction_debug)
            {
              fprintf (stderr, "NEWTM missing local id=%u instance=%llu kind=%llu treatment=%llu producer=%llu\n", i + 1,
                (unsigned long long) value (r, INSTANCE), (unsigned long long) value (r, KIND),
                (unsigned long long) value (r, TREATMENT), (unsigned long long) value (r, PRODUCER));
              for (gimple *g : v.statements)
                for (tree p = anchor_ids (g); p; p = TREE_CHAIN (p))
                  {
                    tree a = record_at (tree_to_uhwi (TREE_VALUE (p)));
                    if (value (a, KIND) == value (r, KIND))
                      { fprintf (stderr, "candidate=%llu producer=%llu\n", (unsigned long long) tree_to_uhwi (TREE_VALUE (p)),
                                  (unsigned long long) value (a, PRODUCER)); print_gimple_stmt (stderr, g, 0, TDF_NONE); }
                  }
            }
          return v.refuse (NULL, "V01: inventoried obligation disappeared without a demonstrated replacement", i + 1);
        }
    }
  return true;
}
static bool dependency_add (tree &list, tree fn)
{
  if (!fn || !newtm_clone_original (fn)) return false;
  for (tree p = list; p; p = TREE_CHAIN (p)) if (TREE_VALUE (p) == fn) return false;
  list = tree_cons (NULL_TREE, fn, list); return true;
}
static bool dependency_union (tree &list, tree other)
{
  bool changed = false;
  for (tree p = other; p; p = TREE_CHAIN (p)) changed |= dependency_add (list, TREE_VALUE (p));
  return changed;
}
static void consolidate (inspection &v, unsigned completed)
{
  bool approved = !v.reason && !live_reason;
  tree result = make_tree_vec (RESULT_FIELDS);
  TREE_VEC_ELT (result, R_OWNER) = live_owner;
  result_set (result, R_SCOPE, live_scope); result_set (result, R_GENERATION, live_generation);
  result_set (result, R_SCOPE_KIND, live_scope_kind);
  result_set (result, R_STATE, approved ? NEWTM_LOCAL_APPROVED : NEWTM_LOCAL_REFUSED);
  result_set (result, R_CLOSED, !live_owner);
  unsigned required = NEWTM_V01 | NEWTM_V02 | NEWTM_V03 | NEWTM_V04 | NEWTM_V05 | NEWTM_V06 | NEWTM_V08;
  if (live_owner && (live_scope_kind == NEWTM_OB_CONTROLLER || newtm_clone_context (live_owner))) required |= NEWTM_V07;
  result_set (result, R_REQUIRED, required);
  result_set (result, R_PROVED, approved ? completed & required : 0);
  const char *why = v.reason ? v.reason : live_reason ? TREE_STRING_POINTER (live_reason) : NULL;
  if (why)
    {
      TREE_VEC_ELT (result, R_REASON) = build_string (strlen (why) + 1, why);
      result_set (result, R_RULE, why[0] == 'V' && why[1] == '0' && why[2] >= '1' && why[2] <= '8' ? why[2] - '0' : 0);
      tree r = record_at (v.obligation); unsigned HOST_WIDE_INT ancestor = v.obligation;
      for (unsigned n = 0; r && n < vec_safe_length (records); ++n)
        {
          unsigned HOST_WIDE_INT next = value (r, PARENT) ? value (r, PARENT) : value (r, PRODUCER);
          if (!next || next == ancestor) break;
          ancestor = next; r = record_at (next);
        }
      result_set (result, R_OBLIGATION, v.obligation); result_set (result, R_ANCESTOR, ancestor);
      tree operation = record_at (v.obligation);
      if (operation) result_set (result, R_KIND, value (operation, KIND) + 1);
      else if (v.bad)
        {
          int kind = -1;
          if (is_gimple_call (v.bad) || gimple_code (v.bad) == GIMPLE_ASM) kind = NEWTM_OB_CALL;
          else if (gimple_code (v.bad) == GIMPLE_RETURN) kind = NEWTM_OB_PUBLISH;
          else if (is_gimple_assign (v.bad))
            {
              if (memory (gimple_assign_lhs (v.bad))) kind = NEWTM_OB_WRITE;
              else for (unsigned i = 1; i < gimple_num_ops (v.bad); ++i)
                if (memory (gimple_op (v.bad, i))) { kind = NEWTM_OB_READ; break; }
              if (kind < 0 && POINTER_TYPE_P (TREE_TYPE (gimple_assign_lhs (v.bad)))) kind = NEWTM_OB_IDENTITY;
            }
          else kind = NEWTM_OB_CONTROL;
          result_set (result, R_KIND, kind + 1);
        }
      location_t loc = r ? value (r, LOCATION) : UNKNOWN_LOCATION;
      if (loc == UNKNOWN_LOCATION && v.bad) loc = gimple_location (v.bad);
      if (loc == UNKNOWN_LOCATION && live_owner) loc = DECL_SOURCE_LOCATION (live_owner);
      result_set (result, R_LOCATION, loc);
      if (v.reason && live_owner && !local_refusal) local_refusal = TREE_VEC_ELT (result, R_REASON);
      if (v.reason && !v.bad && local_failure_result)
        {
          tree first = result_at (local_failure_result);
          for (result_field f : { R_OBLIGATION, R_ANCESTOR, R_KIND, R_RULE, R_LOCATION })
            TREE_VEC_ELT (result, f) = TREE_VEC_ELT (first, f);
        }
    }
  /* Stage dependency closure before publishing any member's approval. */
  auto_vec<tree> dependencies; dependencies.safe_grow_cleared (vec_safe_length (records));
  if (approved)
    {
      for (unsigned i = 0; i < dependencies.length (); ++i)
        if (value ((*records)[i], SCOPE) == live_scope)
          {
            tree r = (*records)[i]; dependency_add (dependencies[i], TREE_VEC_ELT (r, CALLEE));
            dependency_add (dependencies[i], TREE_VEC_ELT (r, DECLARED_CALLEE));
          }
      bool change;
      do
        {
          change = false;
          for (unsigned i = 0; i < dependencies.length (); ++i)
            if (value ((*records)[i], SCOPE) == live_scope)
              {
                tree r = (*records)[i]; unsigned HOST_WIDE_INT producer = value (r, PRODUCER);
                if (producer && producer <= dependencies.length ()) change |= dependency_union (dependencies[i], dependencies[producer - 1]);
                for (tree p = TREE_VEC_ELT (r, CHILDREN); p; p = TREE_CHAIN (p))
                  change |= dependency_union (dependencies[i], dependencies[tree_to_uhwi (TREE_VALUE (p)) - 1]);
              }
        }
      while (change);
    }
  vec_safe_push (local_results, result);
  unsigned HOST_WIDE_INT id = local_results->length ();
  if (v.reason && live_owner && !local_failure_result) local_failure_result = id;
  for (unsigned i = 0; i < vec_safe_length (records); ++i)
    if (value ((*records)[i], SCOPE) == live_scope && live_scope)
      {
        tree r = (*records)[i]; unsigned needed = requirements (r);
        set (r, REQUIRED, needed); set (r, PROVED, approved ? completed & needed : 0);
        set (r, LOCAL_STATE, approved ? NEWTM_LOCAL_APPROVED : NEWTM_LOCAL_REFUSED);
        set (r, LOCAL_RESULT, id); TREE_VEC_ELT (r, DEPENDENCIES) = dependencies[i];
        if (!approved) TREE_VEC_ELT (r, REASON) = TREE_VEC_ELT (result, R_REASON);
        dependency_union (TREE_VEC_ELT (result, R_DEPENDENCIES), dependencies[i]);
      }
  if (approved && live_scope_kind != NEWTM_OB_COPY)
    for (tree p = TREE_VEC_ELT (result, R_DEPENDENCIES); p; p = TREE_CHAIN (p))
      for (unsigned j = vec_safe_length (local_results); j; --j)
        {
          tree callee = (*local_results)[j - 1];
          unsigned role = result_value (callee, R_SCOPE_KIND);
          if (TREE_VEC_ELT (callee, R_OWNER) != TREE_VALUE (p)
              || (role != NEWTM_OB_CLONE && role != NEWTM_OB_REGION)) continue;
          unsigned boundaries = result_value (callee, R_REMAP_BOUNDARIES);
          if (boundaries) newtm_obligation_remap_boundary (live_owner, boundaries);
          if (result_value (callee, R_STATE) == NEWTM_LOCAL_INVALID
              || result_value (callee, R_STATE) == NEWTM_LOCAL_REFUSED
              || result_value (callee, R_GRAPH_INVALID))
            { result_set (result, R_GRAPH_INVALID, 1); invalidate_dependents (live_owner); }
          break;
        }
  invalidate_graph_results ();
}

}

bool newtm_obligation_verify (gimple_seq seq, const char **why, gimple **bad, tree locals)
{
  inspection v;
  newtm_flow_graph flow;
  unsigned completed = 0;
  revoke_scope (live_scope);
  if (!live_owner || !shapes) v.refuse (NULL, "V08: transaction inspection outside descriptor window");
  else if (local_refusal) v.refuse (NULL, TREE_STRING_POINTER (local_refusal));
  if (!v.reason)
    {
      private_roots = hash_map<tree, tree>::create_ggc ();
      for (tree d = locals; d; d = DECL_CHAIN (d)) private_roots->put (d, d);
      if (live_scope_kind != NEWTM_OB_CONTROLLER)
        for (tree d = DECL_ARGUMENTS (live_owner); d; d = DECL_CHAIN (d)) private_roots->put (d, d);
      walk_stmt_info wi = {}; wi.info = &v; walk_gimple_seq (seq, collect_actual, NULL, &wi);
    }
  for (unsigned i = 0; !v.reason && i < v.statements.length (); ++i)
    {
      gimple *s = v.statements[i];
      /* NOPs introduced solely by removal carry no operands or effects. */
      if (gimple_code (s) == GIMPLE_NOP) continue;
      if (!actual_shape (v, s)) break;
      if (gimple_has_volatile_ops (s))
        { v.refuse (s, "V02: volatile transaction operation remains"); break; }
      if (is_gimple_assign (s))
        {
          check_memory (v, i, gimple_assign_lhs (s), true);
          if (!gimple_clobber_p (s))
            for (unsigned j = 1; !v.reason && j < gimple_num_ops (s); ++j)
              check_memory (v, i, gimple_op (s, j), false);
        }
      else if (is_gimple_call (s))
        {
          call_allowed (v, i);
          check_memory (v, i, gimple_call_lhs (s), true);
          for (unsigned j = 0; !v.reason && j < gimple_call_num_args (s); ++j)
            check_memory (v, i, gimple_call_arg (s, j), false);
        }
      else switch (gimple_code (s))
        {
        case GIMPLE_RETURN: check_memory (v, i, gimple_return_retval (as_a<greturn *> (s)), false); break;
        case GIMPLE_BIND: case GIMPLE_LABEL: case GIMPLE_DEBUG: case GIMPLE_PREDICT:
          break;
        case GIMPLE_TRY:
          /* Compiler lifetime cleanups are inspected recursively as ordinary
             operations. This does not authorize exception handlers. */
          if (gimple_try_kind (as_a<gtry *> (s)) != GIMPLE_TRY_FINALLY)
            v.refuse (s, "V02: exception handler in prepared transaction");
          break;
        case GIMPLE_GOTO:
          if (TREE_CODE (gimple_goto_dest (s)) != LABEL_DECL)
            { v.refuse (s, "V02: computed transaction goto remains"); break; }
          break;
        case GIMPLE_COND: case GIMPLE_SWITCH:
          for (unsigned j = 0; !v.reason && j < gimple_num_ops (s); ++j)
            check_memory (v, i, gimple_op (s, j), false);
          break;
        default: v.refuse (s, "V02: unclassified transaction effect or structured operation"); break;
        }
    }
  if (!v.reason)
    {
      const char *flow_reason = NULL; gimple *flow_bad = NULL;
      if (!flow.build (seq, &flow_reason, &flow_bad)) v.refuse (flow_bad, flow_reason);
    }
  if (!v.reason) completed |= NEWTM_V02;
  if (!v.reason && inspect_guards (v, flow)) completed |= NEWTM_V03;
  if (!v.reason) inspect_status (v, flow);
  if (!v.reason && inspect_lifetime (v, flow)) completed |= NEWTM_V04 | NEWTM_V05 | NEWTM_V06;
  if (!v.reason && inspect_failure (v, flow)
      && (live_scope_kind == NEWTM_OB_CONTROLLER || newtm_clone_context (live_owner))) completed |= NEWTM_V07;
  for (unsigned i = 0; !v.reason && i < vec_safe_length (records); ++i)
    {
      tree r = (*records)[i];
      if (value (r, SCOPE) == live_scope && value (r, TREATMENT) == NEWTM_OB_DELEGATED
          && live_scope_kind == NEWTM_OB_CONTROLLER)
        {
          tree helper = TREE_VEC_ELT (r, DECLARED_CALLEE);
          unsigned found = 0;
          bool original = false;
          for (tree root : *records)
            if (value (root, SCOPE) == live_scope && value (root, KIND) == NEWTM_OB_CONTROL
                && TREE_VEC_ELT (root, CALLEE) == helper && helper) original = true;
          if (original && newtm_clone_original (helper))
            for (gimple *s : v.statements)
              if (is_gimple_call (s) && gimple_call_fndecl (s) == helper) ++found;
          if (found != 1) v.refuse (NULL, "V01: delegated transaction lacks a unique prepared body invocation", i + 1);
          continue;
        }
      if (value (r, SCOPE) != live_scope
          || (!value (r, SOURCE) && !(value (r, KIND) == NEWTM_OB_CALL
                                     && value (r, TREATMENT) == NEWTM_OB_PREPARED_CALL))
          || value (r, STATE) == NEWTM_OB_REJECTED || value (r, STATE) == NEWTM_OB_DERIVED) continue;
      if (!value (r, COPY_FROM) && value (r, EVIDENCE) == NEWTM_OB_INVALIDATED)
        { v.refuse (NULL, "V08: source transaction operation has invalidated evidence", i + 1); break; }
      newtm_obligation_kind kind = (newtm_obligation_kind) value (r, KIND);
      newtm_obligation_treatment t = (newtm_obligation_treatment) value (r, TREATMENT);
      if (kind == NEWTM_OB_CALL && t == NEWTM_OB_PREPARED_CALL)
        {
          tree expected = newtm_clone_for (TREE_VEC_ELT (r, CALLEE));
          tree *args = call_inputs->get (r);
          if (!expected || expected != TREE_VEC_ELT (r, DECLARED_CALLEE) || !args)
            { v.refuse (NULL, "V06: prepared callee does not match the original source call", i + 1); break; }
          unsigned found = 0;
          for (gimple *s : v.statements)
            if (is_gimple_call (s) && gimple_call_fndecl (s) == expected
                && gimple_call_num_args (s) >= unsigned (TREE_VEC_LENGTH (*args)) + 2)
              {
                bool arguments_match = true;
                for (int n = 0; n < TREE_VEC_LENGTH (*args); ++n)
                  arguments_match &= same (TREE_VEC_ELT (*args, n), gimple_call_arg (s, n + 2));
                if (!arguments_match) continue;
                for (tree ids = anchor_ids (s); ids; ids = TREE_CHAIN (ids))
                  {
                    unsigned HOST_WIDE_INT id = value (record_at (tree_to_uhwi (TREE_VALUE (ids))), PRODUCER);
                    if (id && value (record_at (id), INSTANCE) == value (r, INSTANCE)
                        && value (record_at (tree_to_uhwi (TREE_VALUE (ids))), KIND) == NEWTM_OB_CALL) ++found;
                  }
              }
          if (found != 1) v.refuse (NULL, "V01: source transaction call lacks a unique prepared replacement", i + 1);
          continue;
        }
      if (kind == NEWTM_OB_CALL && t != NEWTM_OB_DELEGATED)
        {
          bool found = false;
          if (t == NEWTM_OB_PUBLICATION || t == NEWTM_OB_LIFECYCLE)
            for (gimple *s : v.statements)
              if (is_gimple_call (s))
                for (tree ids = anchor_ids (s); ids; ids = TREE_CHAIN (ids))
                  {
                    unsigned HOST_WIDE_INT id = value (record_at (tree_to_uhwi (TREE_VALUE (ids))), PRODUCER);
                    if (id && value (record_at (id), INSTANCE) == value (r, INSTANCE)) found = true;
                  }
          if (!found) v.refuse (NULL, "V01: source transaction call has no executable treatment", i + 1);
          continue;
        }
      if (kind != NEWTM_OB_READ && kind != NEWTM_OB_WRITE && kind != NEWTM_OB_REGISTER) continue;
      tree d = identity (r);
      /* A derived aggregate destination also reads the checked private buffer.
         Require that actual read in this instance or its emitted replacement. */
      if (!d && kind == NEWTM_OB_READ)
        {
          tree *ref = references->get (r);
          bool found = false;
          if (ref && private_interval (*ref, false))
            for (gimple *s : v.statements)
              if (is_gimple_assign (s) && same (gimple_assign_rhs1 (s), *ref))
                for (tree ids = anchor_ids (s); ids; ids = TREE_CHAIN (ids))
                  {
                    tree candidate = record_at (tree_to_uhwi (TREE_VALUE (ids)));
                    unsigned HOST_WIDE_INT producer = value (candidate, PRODUCER);
                    if (value (candidate, INSTANCE) == value (r, INSTANCE)
                        || (producer && value (record_at (producer), INSTANCE) == value (r, INSTANCE))) found = true;
                  }
          if (found) continue;
        }
      if (t == NEWTM_OB_PRIVATE)
        {
          bool found = false;
          for (gimple *s : v.statements)
            for (tree ids = anchor_ids (s); ids; ids = TREE_CHAIN (ids))
              if (tree_to_uhwi (TREE_VALUE (ids)) == i + 1) found = true;
          if (!found) v.refuse (NULL, "V01: private source transaction access disappeared without derivation", i + 1);
          continue;
        }
      if (kind == NEWTM_OB_REGISTER && d
          && TREE_VEC_ELT (d, NEWTM_D_STORAGE)
          && integer_onep (TREE_VEC_ELT (d, NEWTM_D_STORAGE))
          && private_object (v, TREE_VEC_ELT (d, NEWTM_D_OBJECT))) continue;
      built_in_function op = kind == NEWTM_OB_READ ? BUILT_IN_GOMP_TRANSACTION_STM_V1_READ
        : kind == NEWTM_OB_WRITE ? BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE : BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER;
      if (!d || !replacement (v, r, d, op))
        {
          if (flag_transaction_debug)
            { fprintf (stderr, "NEWTM unmatched obligation %u kind %u treatment %u\n", i + 1, kind, t);
              newtm_obligation_dump (stderr, live_owner);
              print_gimple_seq (stderr, seq, 0, TDF_NONE); }
          v.refuse (NULL, kind == NEWTM_OB_READ
                    ? "V01: source transaction read has no matching executable replacement"
                    : kind == NEWTM_OB_WRITE
                    ? "V01: source transaction write has no matching executable replacement"
                    : "V01: source transaction registration has no matching executable replacement", i + 1);
        }
    }
  if (!v.reason) complete_inventory (v);
  if (!v.reason) completed |= NEWTM_V01 | NEWTM_V08;
  unsigned needed = 255;
  if (live_scope_kind != NEWTM_OB_CONTROLLER && !newtm_clone_context (live_owner)) needed &= ~NEWTM_V07;
  if (!v.reason && (completed & needed) != needed) v.refuse (NULL, "V08: incomplete set of local proof stages");
  consolidate (v, completed);
  if (why) *why = v.reason;
  if (bad) *bad = v.bad;
  if (v.reason && v.bad && flag_transaction_debug)
    print_gimple_stmt (stderr, v.bad, 0, TDF_VOPS | TDF_MEMSYMS);
  if (!v.reason)
    {
      if (!inspection_counts) inspection_counts = hash_map<tree, tree>::create_ggc ();
      tree *old = inspection_counts->get (live_owner);
      inspection_counts->put (live_owner, number (old ? tree_to_uhwi (*old) + 1 : 1));
      if (!flow_node_counts) flow_node_counts = hash_map<tree, tree>::create_ggc ();
      old = flow_node_counts->get (live_owner);
      flow_node_counts->put (live_owner, number ((old ? tree_to_uhwi (*old) : 0) + flow.node_count ()));
    }
  return !v.reason;
}
unsigned newtm_obligation_inspections (tree fn)
{ tree *p = inspection_counts ? inspection_counts->get (fn) : NULL; return p ? tree_to_uhwi (*p) : 0; }
unsigned newtm_obligation_flow_nodes (tree fn)
{ tree *p = flow_node_counts ? flow_node_counts->get (fn) : NULL; return p ? tree_to_uhwi (*p) : 0; }

bool newtm_obligation_consistent (tree fn)
{
  for (unsigned i = 0; i < vec_safe_length (records); ++i)
    {
      tree r = (*records)[i];
      if (TREE_VEC_ELT (r, OWNER) != fn) continue;
      if (value (r, STATE) == NEWTM_OB_COVERED) return false;
      if (value (r, LOCAL_STATE) == NEWTM_LOCAL_APPROVED)
        {
          tree proof = result_at (value (r, LOCAL_RESULT));
          if (!proof || result_value (proof, R_STATE) != NEWTM_LOCAL_APPROVED
              || result_value (proof, R_SCOPE) != value (r, SCOPE)
              || result_value (proof, R_GENERATION) != value (r, GENERATION)
              || value (r, REQUIRED) != value (r, PROVED)
              || (result_value (proof, R_PROVED) & value (r, REQUIRED)) != value (r, REQUIRED)) return false;
        }
      else if (value (r, PROVED)) return false;
      if (value (r, STATE) == NEWTM_OB_DERIVED && !TREE_VEC_ELT (r, CHILDREN)) return false;
      for (tree p = TREE_VEC_ELT (r, CHILDREN); p; p = TREE_CHAIN (p))
        {
          unsigned HOST_WIDE_INT id = tree_to_uhwi (TREE_VALUE (p));
          tree child = record_at (id);
          if (!child || id <= i + 1 || value (child, PARENT) != i + 1
              || value (child, SCOPE) != value (r, SCOPE)) return false;
        }
    }
  return true;
}
void newtm_obligation_dump (FILE *file, tree fn)
{
  for (unsigned HOST_WIDE_INT id = 1; id <= newtm_obligation_count (); ++id)
    {
      newtm_obligation_view v;
      newtm_obligation_get (id, &v);
      if (v.owner != fn) continue;
      fprintf (file, "OBL id=%llu scope=%llu kind=%u state=%u treatment=%u evidence=%u parent=%llu copy=%llu source=%u witness=%u local=%u result=%llu required=%u proved=%u\n",
               (unsigned long long) id, (unsigned long long) v.scope, v.kind, v.state,
               v.treatment, v.evidence, (unsigned long long) v.parent,
               (unsigned long long) v.copied_from, v.source, v.witness, v.local_state,
               (unsigned long long) v.local_result, v.required, v.proved);
    }
}
#include "gt-newtm-obligation.h"
