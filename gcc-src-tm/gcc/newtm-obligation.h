/* Inventory, independent local verification and explicit graph dependencies.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_OBLIGATION_H
#define GCC_NEWTM_OBLIGATION_H

enum newtm_obligation_kind
{
  NEWTM_OB_READ, NEWTM_OB_WRITE, NEWTM_OB_CALL, NEWTM_OB_REGISTER,
  NEWTM_OB_IDENTITY, NEWTM_OB_PUBLISH, NEWTM_OB_CONTROL
};
enum newtm_obligation_state
{ NEWTM_OB_PENDING, NEWTM_OB_DERIVED, NEWTM_OB_COVERED, NEWTM_OB_REJECTED };
enum newtm_obligation_evidence
{ NEWTM_OB_CURRENT, NEWTM_OB_INVALIDATED, NEWTM_OB_RETIRED };
enum newtm_obligation_treatment
{
  NEWTM_OB_UNDECLARED, NEWTM_OB_STM, NEWTM_OB_PRIVATE,
  NEWTM_OB_DYNAMIC, NEWTM_OB_PREPARED_CALL, NEWTM_OB_PUBLICATION,
  NEWTM_OB_LIFECYCLE, NEWTM_OB_SPLIT_STORE, NEWTM_OB_RECONCILED,
  NEWTM_OB_DELEGATED, NEWTM_OB_EMITTED, NEWTM_OB_FAILURE_STUB
};
enum newtm_obligation_scope
{ NEWTM_OB_CLONE, NEWTM_OB_REGION, NEWTM_OB_CONTROLLER, NEWTM_OB_COPY };
enum newtm_local_state
{ NEWTM_LOCAL_UNVERIFIED, NEWTM_LOCAL_APPROVED, NEWTM_LOCAL_INVALID, NEWTM_LOCAL_REFUSED };
/* Rule bits V01 through V08. Local approval never grants graph approval. */
enum newtm_local_rule
{ NEWTM_V01 = 1, NEWTM_V02 = 2, NEWTM_V03 = 4, NEWTM_V04 = 8,
  NEWTM_V05 = 16, NEWTM_V06 = 32, NEWTM_V07 = 64, NEWTM_V08 = 128 };

/* Durable facts only. No descriptor, source expression or GIMPLE pointer.
   CURRENT means a snapshot belongs to the live generation, not that it is
   an independently verified proof. No API in step 6.2 issues COVERED. */
struct newtm_obligation_view
{
  unsigned HOST_WIDE_INT id, instance, scope, generation, operation, origin, group;
  unsigned HOST_WIDE_INT copied_operation, copied_from, parent, producer;
  tree owner, callee, declared_callee;
  const char *reason;
  location_t location;
  newtm_obligation_kind kind;
  newtm_obligation_scope scope_kind;
  newtm_obligation_state state;
  newtm_obligation_evidence evidence;
  newtm_obligation_treatment treatment;
  int role, descriptor_state, storage; /* -1 absent, -2 dynamic, -3 invalid. */
  unsigned operand, descriptor_fields, children;
  bool source, witness;
  unsigned required, proved;
  unsigned HOST_WIDE_INT local_result;
  newtm_local_state local_state;
  tree dependencies; /* List of prepared declarations; no expired operands. */
};

/* Historical local result for one scope/revision. CLOSED means its operational
   window is gone. Neither APPROVED nor an empty dependency list grants any
   permission to activate a graph or reuse this result after remapping. */
struct newtm_local_view
{
  unsigned HOST_WIDE_INT id, scope, generation, obligation, ancestor;
  tree owner, dependencies;
  newtm_obligation_scope scope_kind;
  newtm_local_state state;
  unsigned required, proved, rule;
  location_t location;
  int kind; /* -1 for a scope/instruction without an obligation. */
  const char *reason;
  bool closed;
  /* A dependency changed since this local revision was recorded. This is
     neither a graph certificate nor a change to the local proof itself. */
  bool graph_invalid;
  /* Boundaries crossed by this revision or a dependency. These invalidate
     reuse as a current proof, not its value as historical evidence. */
  unsigned remap_boundaries;
};
enum newtm_remap_boundary { NEWTM_REMAP_NESTED = 1, NEWTM_REMAP_OMP = 2 };
extern void newtm_obligation_remap_boundary (tree, unsigned);
extern unsigned HOST_WIDE_INT newtm_obligation_local_count ();
extern bool newtm_obligation_local_get (unsigned HOST_WIDE_INT, newtm_local_view *);
/* Certificate of the pre-lowering graph only. It cannot authorize remapped IR. */
struct newtm_graph_view
{
  tree root;
  unsigned HOST_WIDE_INT controller;
  unsigned members;
  bool valid; /* Historical validity only. */
  /* No current proof is produced by the historical verifier. */
  bool current_approved;
  unsigned remap_boundaries;
};
extern unsigned HOST_WIDE_INT newtm_obligation_graph_count ();
extern bool newtm_obligation_graph_get (unsigned HOST_WIDE_INT, newtm_graph_view *);
extern bool newtm_obligation_graph_member (unsigned HOST_WIDE_INT, unsigned, unsigned HOST_WIDE_INT *);
/* Component of a function revision; the controller itself has no component. */
extern bool newtm_obligation_graph_component (unsigned HOST_WIDE_INT, unsigned HOST_WIDE_INT, unsigned *);
extern bool newtm_obligation_graph_commit (tree, unsigned HOST_WIDE_INT, tree);
extern void newtm_obligation_graph_invalidate_callers (tree);
extern bool newtm_obligation_graph_revalidate (unsigned HOST_WIDE_INT, unsigned HOST_WIDE_INT, tree);
/* Emit the first structured refusal from the latest inspection. */
extern void newtm_obligation_report (location_t);

extern bool newtm_obligation_begin (tree, gimple_seq, newtm_obligation_scope,
                                   gimple * = NULL);
extern void newtm_obligation_end ();
/* Inspect actual High GIMPLE, then atomically publish local proof results.
   A refused preflight stub can be structurally valid but never locally approved. */
extern bool newtm_obligation_verify (gimple_seq, const char **, gimple ** = NULL,
                                     tree = NULL_TREE);
extern unsigned newtm_obligation_inspections (tree);
/* Scalar history only; neither a graph nor a flow-safety certificate. */
extern unsigned newtm_obligation_flow_nodes (tree);
extern void newtm_obligation_reject (const char *);
extern void newtm_obligation_declare (const gimple *, newtm_obligation_kind,
                                     newtm_obligation_treatment);
extern void newtm_obligation_declare_all (const gimple *, newtm_obligation_treatment,
                                         tree = NULL_TREE);
extern void newtm_obligation_emitted (gimple_seq, const gimple * = NULL);
extern void newtm_obligation_math (gimple *);
/* Read only during the active preparation window, before end. */
extern bool newtm_obligation_math_p (const gimple *);
/* Required abort edge; survives forget/retire until the window closes.
   Recording this obligation never proves its propagation. */
extern void newtm_obligation_abort (gimple *, tree);
extern bool newtm_obligation_abort_p (const gimple *);
extern void newtm_obligation_reconcile (const gimple *, const gimple *);
extern void newtm_obligation_derive_store (const gimple *, gimple *);
/* Flattening a structured transaction derives its control obligation from
   actual child control operations; it never declares the wrapper covered. */
extern void newtm_obligation_derive_control (const gimple *, gimple_seq);
extern void newtm_obligation_copy (const gimple *, gimple *);
extern void newtm_obligation_forget (const gimple *);
extern void newtm_obligation_retire (gimple_seq);
extern void newtm_obligation_reset ();
/* Explicit invalidation of recorded dependencies; not a replacement for the
   independent mutation/remapping audit to be implemented in steps 6.3–6.5. */
extern void newtm_obligation_invalidate (unsigned HOST_WIDE_INT);
extern unsigned HOST_WIDE_INT newtm_obligation_count ();
extern bool newtm_obligation_get (unsigned HOST_WIDE_INT, newtm_obligation_view *);
extern bool newtm_obligation_has_anchor (unsigned HOST_WIDE_INT);
/* Ledger integrity only: pending and rejected entries are legitimate. This
   does not certify coverage or authorize activation of a prepared body. */
extern bool newtm_obligation_consistent (tree);
extern void newtm_obligation_dump (FILE *, tree);
#endif
