/* Current native and reconstructed LTO graph certificates. No approval transfer.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_CURRENT_H
#define GCC_NEWTM_CURRENT_H
enum newtm_current_state
{ NEWTM_CURRENT_ABSENT, NEWTM_CURRENT_APPROVED, NEWTM_CURRENT_INVALID,
  NEWTM_CURRENT_REFUSED };
enum newtm_current_component
{ NEWTM_CURRENT_CHANNELS = 1, NEWTM_CURRENT_STATUS = 2,
  NEWTM_CURRENT_OWNER = 4, NEWTM_CURRENT_IDENTITY = 8,
  NEWTM_CURRENT_COVERAGE = 16, NEWTM_CURRENT_ALL = 31 };
struct newtm_current_view
{
  unsigned HOST_WIDE_INT id;
  tree root, controller;
  unsigned members, required, proved, dependencies;
  newtm_current_state state;
  bool current_approved;
  const char *reason;
};
extern unsigned HOST_WIDE_INT newtm_current_count ();
extern bool newtm_current_get (unsigned HOST_WIDE_INT, newtm_current_view *);
extern bool newtm_current_member (unsigned HOST_WIDE_INT, unsigned, tree *, unsigned *);
/* Always reexecutes all component proofs and creates a new generation.
   Failed attempts never expose a partly approved graph. */
extern bool newtm_current_verify (tree);
/* NULL invalidates every certificate.  Restoring IR never revives one. */
extern void newtm_current_invalidate (tree);
/* A standalone component recheck withdraws aggregate authority. During a
   complete graph check, successful unchanged shared evidence is staged. */
extern void newtm_current_component_begin (tree);
/* End the current native or LTO window before transformations/streaming. */
extern void newtm_current_end_frontend ();
/* Scalar byte snapshot of the live native or reconstructed SSA CFG. */
extern tree newtm_current_ir (tree);
/* Diagnostic context only; never reads retired descriptors or program values. */
extern location_t newtm_current_diagnostic_location (tree, gimple *);
extern void newtm_current_diagnostic_context (tree, gimple *, const char *,
                                             const char *);
extern void newtm_current_diagnose (tree, gimple *, const char *, const char *,
                                    const char *, const char * = NULL);
#endif
