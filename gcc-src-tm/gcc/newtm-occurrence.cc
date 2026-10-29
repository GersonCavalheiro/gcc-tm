/* Bind transaction source occurrences to emitted, already evaluated GIMPLE.
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
#include "newtm-occurrence.h"
#include "newtm-descriptor.h"
#include "ggc.h"

/* Operational bindings retain source facts and a key to the actual
   instruction, never an executable copy of a source recipe.  Source-only
   witnesses are explicitly unevaluated and cannot supply an address/result.
   Binding vector: operation ID, source ID, parent source ID, evaluation-group
   ID, source tree code, location, function, mode, copied-from operation ID,
   source-only witness (otherwise NULL), source site.  Mode flags are load=1,
   store=2, value=4, call=8, trace-register=16, source-only=32, return=64,
   branch=128.  A site retains (ID, parent site, evaluation-group site, code,
   location, function, stable origin token, captured registration type). Source nodes can be freed
   during gimplification; only stable facts may survive that phase.  All executable operands are
   read from the instruction key. */
static GTY(()) hash_map<gimple *, tree> *newtm_bindings;
static GTY(()) hash_map<tree, tree> *newtm_selected;
static unsigned HOST_WIDE_INT newtm_next_source;
static unsigned HOST_WIDE_INT newtm_next_operation;

struct newtm_source_frame
{
  unsigned HOST_WIDE_INT id, parent, group;
  enum tree_code code;
  location_t location;
  unsigned function_uid;
};
/* Scalar facts only; no untraced GC pointers on this stack. */
static auto_vec<newtm_source_frame> newtm_source_stack;
static GTY(()) vec<tree, va_gc> *newtm_site_stack;

void
newtm_occurrence_select (tree fn)
{
  if (!newtm_selected) newtm_selected = hash_map<tree, tree>::create_ggc ();
  newtm_selected->put (fn, fn);
  newtm_descriptor_note_function (fn);
}

bool
newtm_occurrence_enter (tree expr, tree fn)
{
  if (!flag_openmp || !expr || !fn) return false;
  bool same_function = !newtm_source_stack.is_empty ()
    && newtm_source_stack.last ().function_uid == DECL_UID (fn);
  if (!same_function && TREE_CODE (expr) != OMP_TRANSACTION
      && (!newtm_selected || !newtm_selected->get (fn))) return false;
  newtm_source_frame f;
  f.id = ++newtm_next_source;
  f.function_uid = DECL_UID (fn);
  f.parent = same_function ? newtm_source_stack.last ().id : 0;
  f.group = same_function ? newtm_source_stack.last ().group : 0;
  f.code = TREE_CODE (expr);
  /* Keep the outer evaluation group: helper-generated INIT_EXPR temporaries
     must not split an RMW's load/address computation/store into unrelated IDs. */
  if (!f.group && (f.code == EXPR_STMT || f.code == RETURN_EXPR
                  || f.code == CALL_EXPR || f.code == MODIFY_EXPR || f.code == INIT_EXPR
                  || f.code == PREINCREMENT_EXPR || f.code == POSTINCREMENT_EXPR
                  || f.code == PREDECREMENT_EXPR || f.code == POSTDECREMENT_EXPR))
    f.group = f.id;
  f.location = EXPR_P (expr) && EXPR_LOCATION (expr) != UNKNOWN_LOCATION
    ? EXPR_LOCATION (expr) : input_location;
  tree site = make_tree_vec (8);
  TREE_VEC_ELT (site, 0) = build_int_cst (long_long_unsigned_type_node, f.id);
  TREE_VEC_ELT (site, 1) = same_function ? newtm_site_stack->last () : NULL_TREE;
  TREE_VEC_ELT (site, 2) = NULL_TREE;
  if (f.group && f.group != f.id)
    for (unsigned i = newtm_source_stack.length (); i; --i)
      if (newtm_source_stack[i - 1].id == f.group)
        { TREE_VEC_ELT (site, 2) = (*newtm_site_stack)[i - 1]; break; }
  TREE_VEC_ELT (site, 3) = build_int_cst (unsigned_type_node, f.code);
  TREE_VEC_ELT (site, 4) = build_int_cst (unsigned_type_node, f.location);
  TREE_VEC_ELT (site, 5) = fn;
  /* Gimplification explicitly frees helper INIT_EXPR/CALL_EXPR trees. Keep a
     stable token, never a GC root to a source node whose storage can be freed. */
  TREE_VEC_ELT (site, 6) = TREE_VEC_ELT (site, 0);
  TREE_VEC_ELT (site, 7) = newtm_descriptor_registration_type (expr);
  vec_safe_push (newtm_site_stack, site);
  newtm_source_stack.safe_push (f);
  return true;
}

void
newtm_occurrence_leave ()
{
  gcc_assert (!newtm_source_stack.is_empty ());
  newtm_source_stack.pop ();
  newtm_site_stack->pop ();
}

tree
newtm_occurrence_binding (const gimple *stmt)
{
  tree *v = newtm_bindings ? newtm_bindings->get (const_cast<gimple *> (stmt)) : NULL;
  return v ? *v : NULL_TREE;
}

static bool
newtm_memory_operand (tree t)
{
  return t && (REFERENCE_CLASS_P (t)
               || (DECL_P (t) && (VAR_P (t) || TREE_CODE (t) == PARM_DECL)
                   && !is_gimple_reg (t)));
}

static void
newtm_record_binding (gimple *stmt, unsigned mode, tree ref)
{
  if (!newtm_bindings) newtm_bindings = hash_map<gimple *, tree>::create_ggc ();
  const newtm_source_frame &f = newtm_source_stack.last ();
  tree v = make_tree_vec (11);
  TREE_VEC_ELT (v, 0) = build_int_cst (long_long_unsigned_type_node, ++newtm_next_operation);
  TREE_VEC_ELT (v, 1) = build_int_cst (long_long_unsigned_type_node, f.id);
  TREE_VEC_ELT (v, 2) = build_int_cst (long_long_unsigned_type_node, f.parent);
  TREE_VEC_ELT (v, 3) = build_int_cst (long_long_unsigned_type_node, f.group);
  TREE_VEC_ELT (v, 4) = build_int_cst (unsigned_type_node, f.code);
  TREE_VEC_ELT (v, 5) = build_int_cst (unsigned_type_node, f.location);
  TREE_VEC_ELT (v, 6) = current_function_decl;
  TREE_VEC_ELT (v, 7) = build_int_cst (unsigned_type_node, mode);
  TREE_VEC_ELT (v, 8) = size_zero_node;
  TREE_VEC_ELT (v, 9) = ref;
  TREE_VEC_ELT (v, 10) = newtm_site_stack->last ();
  newtm_bindings->put (stmt, v);
}

/* An explicit non-executing anchor preserves semantic reads which frontend
   folding may erase.  It has NO fabricated result or evaluated address.  A
   later instrumentation pass must reconcile it or reject it, never pretend
   that this witness is an already evaluated GIMPLE load. */
void
newtm_occurrence_source_only (gimple *stmt, tree ref)
{
  gcc_assert (!newtm_source_stack.is_empty () && ref && !TREE_SIDE_EFFECTS (ref));
  newtm_record_binding (stmt, 32, ref);
}

void
newtm_occurrence_emit (gimple *stmt)
{
  if (!stmt || newtm_source_stack.is_empty () || !current_function_decl
      || newtm_source_stack.last ().function_uid != DECL_UID (current_function_decl)
      || newtm_occurrence_binding (stmt))
    return;
  unsigned mode = 0;
  if (is_gimple_assign (stmt))
    {
      mode = 4;
      if (newtm_memory_operand (gimple_assign_lhs (stmt))) mode |= 2;
      if (gimple_assign_single_p (stmt)
          && newtm_memory_operand (gimple_assign_rhs1 (stmt))) mode |= 1;
      /* Lifetime clobbers are not program reads/writes. */
      if (gimple_clobber_p (stmt)) return;
    }
  else if (is_gimple_call (stmt))
    {
      mode = 8;
      tree fn = gimple_call_fndecl (stmt);
      if (fn && fndecl_built_in_p (fn, BUILT_IN_NORMAL))
        {
          if (DECL_FUNCTION_CODE (fn) == BUILT_IN_GOMP_TRANSACTION_REGISTER)
            mode = 16;
          else if (DECL_FUNCTION_CODE (fn) == BUILT_IN_GOMP_TRANSACTION_BEGIN
                   || DECL_FUNCTION_CODE (fn) == BUILT_IN_GOMP_TRANSACTION_END)
            return;
        }
    }
  else if (gimple_code (stmt) == GIMPLE_RETURN) mode = 64;
  else if (gimple_code (stmt) == GIMPLE_COND) mode = 128;
  else return;
  newtm_record_binding (stmt, mode, NULL_TREE);
}

/* Catch helper paths which append without gimplify_seq_add_stmt.  Descendants
   already captured by their own source frame retain that more specific owner.
   The walker visits only newly appended statements, not preceding evaluations. */
static tree
newtm_capture_stmt (gimple_stmt_iterator *gsi, bool *, walk_stmt_info *)
{
  newtm_occurrence_emit (gsi_stmt (*gsi));
  return NULL_TREE;
}

void
newtm_occurrence_capture (gimple_seq seq, gimple *previous)
{
  if (newtm_source_stack.is_empty () || !current_function_decl
      || newtm_source_stack.last ().function_uid != DECL_UID (current_function_decl)) return;
  gimple_stmt_iterator p = previous ? gsi_for_stmt (previous, &seq) : gsi_start (seq);
  if (previous) gsi_next (&p);
  for (; !gsi_end_p (p); gsi_next (&p))
    {
      /* walk_gimple_stmt traverses child sequences without relinking P. */
      walk_stmt_info wi = {};
      walk_gimple_stmt (&p, newtm_capture_stmt, NULL, &wi);
    }
}

void
newtm_occurrence_copy (const gimple *original, gimple *copy)
{
  tree v = newtm_occurrence_binding (original);
  if (!v) return;
  tree next = copy_node (v);
  TREE_VEC_ELT (next, 8) = TREE_VEC_ELT (v, 0);
  TREE_VEC_ELT (next, 0)
    = build_int_cst (long_long_unsigned_type_node, ++newtm_next_operation);
  newtm_bindings->put (copy, next);
  newtm_descriptor_copy (original, copy);
}

static tree
newtm_retire_stmt (gimple_stmt_iterator *gsi, bool *, walk_stmt_info *)
{
  if (newtm_bindings) newtm_bindings->remove (gsi_stmt (*gsi));
  return NULL_TREE;
}

void
newtm_occurrence_retire (gimple_seq seq)
{
  if (!newtm_bindings) return;
  walk_stmt_info wi = {};
  walk_gimple_seq (seq, newtm_retire_stmt, NULL, &wi);
}

/* No operational bindings survive the High-GIMPLE preparation window.  In
   particular, release roots for old instructions replaced while outlining. */
void
newtm_occurrence_reset ()
{
  gcc_assert (newtm_source_stack.is_empty ());
  newtm_bindings = NULL;
  newtm_selected = NULL;
  gcc_assert (vec_safe_is_empty (newtm_site_stack));
  newtm_site_stack = NULL;
  newtm_descriptor_reset ();
}

#include "gt-newtm-occurrence.h"
