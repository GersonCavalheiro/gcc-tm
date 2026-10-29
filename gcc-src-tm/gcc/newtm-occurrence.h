/* Non-executable occurrence bindings for experimental OpenMP transactions.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_OCCURRENCE_H
#define GCC_NEWTM_OCCURRENCE_H

/* These bindings describe actual GIMPLE operands, never executable copies of
   source recipes.  They are valid through omplower, before optimization/SSA.
   Source IDs identify a gimplification invocation; operation IDs identify a
   particular emitted instruction (copies receive fresh operation IDs).
   Binding slot 10 links to the source site: its stable origin token,
   captured registration type and parent/group sites are provenance, never
   expressions to evaluate again. Source helper trees may be explicitly freed. */
extern void newtm_occurrence_select (tree);
extern bool newtm_occurrence_enter (tree, tree);
extern void newtm_occurrence_leave ();
extern void newtm_occurrence_emit (gimple *);
extern void newtm_occurrence_source_only (gimple *, tree);
extern void newtm_occurrence_capture (gimple_seq, gimple *);
extern tree newtm_occurrence_binding (const gimple *);
extern void newtm_occurrence_copy (const gimple *, gimple *);
extern void newtm_occurrence_retire (gimple_seq);
extern void newtm_occurrence_reset ();

/* RAII balances the source stack on every early return/error in gimplify. */
class newtm_occurrence_scope
{
  bool active;
public:
  newtm_occurrence_scope (tree expr, tree fn)
    : active (newtm_occurrence_enter (expr, fn)) {}
  ~newtm_occurrence_scope () { if (active) newtm_occurrence_leave (); }
  bool enabled () const { return active; }
};
#endif
