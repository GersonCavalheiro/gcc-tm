/* Prepare existing accesses before remapping and descriptor retirement.
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
#include "function.h"
#include "tree-pass.h"
#include "fold-const.h"
#include "internal-fn.h"
#include "newtm-access.h"
#include "newtm-clone.h"
#include "newtm-lto.h"
#include "newtm-occurrence.h"
#include "newtm-abi.h"
#include "newtm-obligation.h"

static GTY(()) tree newtm_live_access_function;
static GTY(()) hash_map<tree, tree> *newtm_access_summaries;

bool newtm_access_begin (tree fn)
{
  if (newtm_live_access_function || !cfun || cfun->decl != fn
      || (cfun->curr_properties & PROP_gimple_any) || newtm_access_prepared_p (fn))
    return false;
  newtm_live_access_function = fn;
  return true;
}
void newtm_access_end () { newtm_live_access_function = NULL_TREE; }
unsigned newtm_access_count (tree fn, newtm_access_storage kind)
{
  tree *p = newtm_access_summaries ? newtm_access_summaries->get (fn) : NULL;
  return p ? tree_to_uhwi (TREE_VEC_ELT (*p, kind)) : 0;
}
bool newtm_access_prepared_p (tree fn)
{ return newtm_access_summaries && newtm_access_summaries->get (fn); }

bool
newtm_access_describe (const gimple *stmt, newtm_descriptor_role role,
                       unsigned index, newtm_access *out)
{
  *out = {};
  out->disposition = NEWTM_ACCESS_REJECTED;
  out->reason = "access descriptor is outside its preparation window";
  tree occurrence = newtm_occurrence_binding (stmt);
  if (!newtm_live_access_function || !occurrence
      || TREE_VEC_ELT (occurrence, 6) != newtm_live_access_function) return false;
  tree d = newtm_descriptor_find (stmt, role, index);
  out->reason = "missing executable access descriptor";
  if (!d) return false;
  unsigned state = tree_to_uhwi (TREE_VEC_ELT (d, NEWTM_D_STATE));
  out->reason = "unknown, source-only or stale access identity";
  if (state > NEWTM_DESC_RESULT) return false;
  out->reason = "unresolved access identity channel";
  unsigned fields[] = { NEWTM_D_OBJECT, NEWTM_D_EXTENT, NEWTM_D_OFFSET,
    NEWTM_D_SIZE, NEWTM_D_KIND, NEWTM_D_VALID, NEWTM_D_STORAGE };
  for (unsigned field : fields)
    if (!TREE_VEC_ELT (d, field)) return false;
  /* Reacquire the operand from the current instruction. The descriptor's
     logical witness is never a substitute for the operation being changed. */
  if (role == NEWTM_DESC_READ && is_gimple_assign (stmt))
    out->reference = gimple_assign_rhs1 (stmt);
  else if (role == NEWTM_DESC_WRITE)
    {
      if (is_gimple_assign (stmt)) out->reference = gimple_assign_lhs (stmt);
      else if (is_gimple_call (stmt)) out->reference = gimple_call_lhs (stmt);
    }
  out->reason = "descriptor does not identify an evaluated memory operation";
  if (!out->reference || out->reference != TREE_VEC_ELT (d, NEWTM_D_LOGICAL))
    return false;
  out->object = TREE_VEC_ELT (d, NEWTM_D_OBJECT);
  out->extent = TREE_VEC_ELT (d, NEWTM_D_EXTENT);
  out->offset = TREE_VEC_ELT (d, NEWTM_D_OFFSET);
  out->size = TREE_VEC_ELT (d, NEWTM_D_SIZE);
  out->kind = TREE_VEC_ELT (d, NEWTM_D_KIND);
  out->valid = TREE_VEC_ELT (d, NEWTM_D_VALID);
  out->storage = TREE_VEC_ELT (d, NEWTM_D_STORAGE);
  if (TREE_CODE (out->storage) != INTEGER_CST)
    out->disposition = NEWTM_ACCESS_DYNAMIC;
  else if (tree_fits_uhwi_p (out->storage)
           && tree_to_uhwi (out->storage) == NEWTM_STORAGE_PRIVATE)
    out->disposition = NEWTM_ACCESS_PRIVATE;
  else if (tree_fits_uhwi_p (out->storage)
           && tree_to_uhwi (out->storage) == NEWTM_STORAGE_REGISTERED)
    out->disposition = NEWTM_ACCESS_REGISTERED;
  else { out->reason = "unknown access storage classification"; return false; }
  out->reason = NULL;
  return true;
}

namespace {
/* Structural proof only: never execute a witness. Expand compiler temporary
   definitions within one straight-line sequence, normalize zero-offset memory
   references, and pair each anchor with at most one subsequent real load.
   Calls, writes and control boundaries discard pending proofs. */
static tree
read_shape (tree t, hash_map<tree, gimple *> &definitions, unsigned depth = 0)
{
  if (!t || depth > 24) return NULL_TREE;
  STRIP_ANY_LOCATION_WRAPPER (t);
  STRIP_NOPS (t);
  if (gimple **p = definitions.get (t))
    {
      gimple *stmt = *p;
      tree a = read_shape (gimple_assign_rhs1 (stmt), definitions, depth + 1);
      if (!a) return NULL_TREE;
      enum tree_code code = gimple_assign_rhs_code (stmt);
      switch (gimple_assign_rhs_class (stmt))
        {
        case GIMPLE_SINGLE_RHS: return a;
        case GIMPLE_UNARY_RHS:
          return fold_build1 (code, TREE_TYPE (t), a);
        case GIMPLE_BINARY_RHS:
          {
            tree b = read_shape (gimple_assign_rhs2 (stmt), definitions, depth + 1);
            return b ? fold_build2 (code, TREE_TYPE (t), a, b) : NULL_TREE;
          }
        default: return NULL_TREE;
        }
    }
  if (TREE_CODE (t) == INDIRECT_REF || TREE_CODE (t) == MEM_REF)
    {
      tree base = read_shape (TREE_OPERAND (t, 0), definitions, depth + 1);
      if (!base) return NULL_TREE;
      if (TREE_CODE (t) == MEM_REF && !integer_zerop (TREE_OPERAND (t, 1)))
        base = fold_build2 (POINTER_PLUS_EXPR, TREE_TYPE (base), base,
                           fold_convert (sizetype, TREE_OPERAND (t, 1)));
      return build1 (INDIRECT_REF, TYPE_MAIN_VARIANT (TREE_TYPE (t)), base);
    }
  if (!EXPR_P (t)) return t;
  if (TREE_SIDE_EFFECTS (t)) return NULL_TREE;
  tree copy = copy_node (t);
  for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
    if (TREE_OPERAND (t, i))
      {
        tree operand = read_shape (TREE_OPERAND (t, i), definitions, depth + 1);
        if (!operand) return NULL_TREE;
        TREE_OPERAND (copy, i) = operand;
      }
  return copy;
}
static void
reconcile_reads (gimple_seq sequence)
{
  hash_map<tree, gimple *> definitions;
  auto_vec<gimple *> pending;
  for (gimple_stmt_iterator it = gsi_start (sequence); !gsi_end_p (it); gsi_next (&it))
    {
      gimple *stmt = gsi_stmt (it);
      tree b = newtm_occurrence_binding (stmt);
      unsigned mode = b ? tree_to_uhwi (TREE_VEC_ELT (b, 7)) : 0;
      if (mode & 32) { pending.safe_push (stmt); continue; }
      if ((mode & 1) && is_gimple_assign (stmt))
        {
          tree actual = read_shape (gimple_assign_rhs1 (stmt), definitions);
          for (unsigned i = 0; actual && i < pending.length (); ++i)
            {
              tree witness = TREE_VEC_ELT (newtm_occurrence_binding (pending[i]), 9);
              tree shape = read_shape (witness, definitions);
              if (shape && operand_equal_p (actual, shape, 0))
                {
                  newtm_obligation_reconcile (pending[i], stmt);
                  newtm_occurrence_forget (pending[i]);
                  pending.ordered_remove (i);
                  break;
                }
            }
        }
      if (is_gimple_assign (stmt))
        {
          tree lhs = gimple_assign_lhs (stmt);
          if (TREE_CODE (lhs) == SSA_NAME
              || (DECL_P (lhs) && DECL_ARTIFICIAL (lhs) && is_gimple_reg (lhs)))
            definitions.put (lhs, stmt);
        }
      if (gimple_code (stmt) == GIMPLE_BIND)
        reconcile_reads (gimple_bind_body (as_a<gbind *> (stmt)));
      else if (gimple_code (stmt) == GIMPLE_TRY)
        {
          reconcile_reads (gimple_try_eval (as_a<gtry *> (stmt)));
          reconcile_reads (gimple_try_cleanup (as_a<gtry *> (stmt)));
        }
      else if (is_gimple_omp (stmt) && gimple_has_substatements (stmt))
        reconcile_reads (gimple_omp_body (stmt));
      bool arithmetic = is_gimple_call (stmt) && gimple_call_internal_p (stmt)
        && (gimple_call_internal_fn (stmt) == IFN_ADD_OVERFLOW
            || gimple_call_internal_fn (stmt) == IFN_SUB_OVERFLOW
            || gimple_call_internal_fn (stmt) == IFN_MUL_OVERFLOW);
      if ((mode & 2) || (is_gimple_call (stmt) && !arithmetic)
          || gimple_has_substatements (stmt)
          || gimple_code (stmt) == GIMPLE_COND || gimple_code (stmt) == GIMPLE_SWITCH
          || gimple_code (stmt) == GIMPLE_LABEL || gimple_code (stmt) == GIMPLE_GOTO)
        { pending.truncate (0); definitions.empty (); }
    }
}
struct preflight { const char *reason = NULL; unsigned counts[4] = {}; };
static tree inspect (gimple_stmt_iterator *it, bool *, walk_stmt_info *wi)
{
  preflight &s = *(preflight *) wi->info;
  gimple *stmt = gsi_stmt (*it);
  tree binding = newtm_occurrence_binding (stmt);
  if (tree d = newtm_descriptor_find (stmt, NEWTM_DESC_ADDRESS))
    if (!TREE_VEC_ELT (d, NEWTM_D_VALID)
        || tree_to_uhwi (TREE_VEC_ELT (d, NEWTM_D_STATE)) > NEWTM_DESC_RESULT)
      { s.reason = "address formation has no executable identity"; ++s.counts[NEWTM_ACCESS_REJECTED]; }
  if (!binding) return NULL_TREE; /* Compiler scaffolding is not a source op. */
  unsigned mode = tree_to_uhwi (TREE_VEC_ELT (binding, 7));
  if (mode & 32)
    { s.reason = "source-only occurrence has no evaluated access";
      ++s.counts[NEWTM_ACCESS_REJECTED]; return NULL_TREE; }
  for (unsigned bit = 1; bit <= 2; bit <<= 1)
    if ((mode & bit) || (bit == 2 && newtm_descriptor_find (stmt, NEWTM_DESC_WRITE)))
      {
        newtm_access access;
        if (!newtm_access_describe (stmt, bit == 1 ? NEWTM_DESC_READ
                                                  : NEWTM_DESC_WRITE, 0, &access))
          s.reason = access.reason;
        ++s.counts[access.disposition];
      }
  if ((mode & 8) && is_gimple_call (stmt))
    if (const char *reason = newtm_clone_check_call (stmt))
      { s.reason = reason; ++s.counts[NEWTM_ACCESS_REJECTED]; }
  if ((mode & 16) && !newtm_descriptor_find (stmt, NEWTM_DESC_REGISTER))
    { s.reason = "registration has no executable descriptor";
      ++s.counts[NEWTM_ACCESS_REJECTED]; }
  if (mode & (8 | 16))
    for (tree p = newtm_descriptor_entries (stmt); p; p = TREE_CHAIN (p))
      {
        unsigned role = tree_to_uhwi (TREE_VEC_ELT (TREE_PURPOSE (p), 0));
        if (role != NEWTM_DESC_ARGUMENT && role != NEWTM_DESC_REGISTER) continue;
        tree d = TREE_VALUE (p);
        unsigned fields[] = { NEWTM_D_OBJECT, NEWTM_D_EXTENT, NEWTM_D_OFFSET,
                              NEWTM_D_KIND, NEWTM_D_VALID, NEWTM_D_STORAGE };
        for (unsigned field : fields)
          if (!TREE_VEC_ELT (d, field))
            { s.reason = "call or registration has unresolved identity";
              ++s.counts[NEWTM_ACCESS_REJECTED]; break; }
      }
  return NULL_TREE;
}
}
bool
newtm_access_preflight (tree fn, const char **reason)
{
  if (newtm_live_access_function != fn)
    { *reason = "function is outside its preparation window"; return false; }
  reconcile_reads (gimple_body (fn));
  preflight s;
  walk_stmt_info wi = {}; wi.info = &s;
  walk_gimple_seq (gimple_body (fn), inspect, NULL, &wi);
  tree summary = make_tree_vec (4);
  for (unsigned i = 0; i < 4; ++i)
    TREE_VEC_ELT (summary, i) = build_int_cst (unsigned_type_node, s.counts[i]);
  if (!newtm_access_summaries) newtm_access_summaries = hash_map<tree, tree>::create_ggc ();
  newtm_access_summaries->put (fn, summary);
  *reason = s.reason;
  return !s.reason;
}


/* Persistent contracts only: reconstructed proofs are never serialized. */
tree newtm_access_export ()
{
  tree data = make_tree_vec (1);
  TREE_VEC_ELT (data, 0) = newtm_lto_pack (newtm_access_summaries);
  return data;
}
void newtm_access_import (tree data)
{
  gcc_assert (data && TREE_CODE (data) == TREE_VEC && TREE_VEC_LENGTH (data) == 1);
  newtm_lto_unpack (newtm_access_summaries, TREE_VEC_ELT (data, 0));
}

#include "gt-newtm-access.h"
