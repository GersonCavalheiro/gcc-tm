/* Prepared GENERIC clones and per-activation identity channels.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_CLONE_H
#define GCC_NEWTM_CLONE_H
/* Called at the actual outlining boundary; records declaration provenance. */
extern void newtm_clone_record_outline (tree, tree);
extern void newtm_clone_record_decl (tree, tree);
extern void newtm_clone_record_field (tree, tree, bool);
/* Declaration/field provenance for proof terms; never access descriptors. */
extern tree newtm_clone_memory_origin (tree);
extern tree newtm_clone_memory_field (tree);
/* Declaration-only provenance from the actual GNU frame construction. */
extern void newtm_clone_record_frame (tree, tree, tree, tree);
extern tree newtm_clone_native_frame (tree);
/* A current outlined receiver, only after confronting capture producers. */
extern tree newtm_clone_entry_frame (tree, gimple *);
extern void newtm_clone_unit ();
extern tree newtm_clone_for (tree);
extern tree newtm_clone_original (tree);
extern tree newtm_clone_context (tree);
/* Signature-owned transport channels, valid independently of source names. */
extern tree newtm_clone_channel (tree, unsigned);
/* Exact transport object extent and direction, derived from the signature. */
extern tree newtm_clone_transport_extent (tree, tree, bool);
extern bool newtm_clone_publication_p (tree, const gimple *);
/* Nonnull only for the prepared body of an external source region. */
extern tree newtm_clone_region_owner (tree);
extern void newtm_clone_bind_region (gimple *, tree);
extern tree newtm_clone_region_body (const gimple *);
extern void newtm_clone_forget_region (const gimple *);
extern const char *newtm_clone_region_rejection (tree);
/* Certify the complete prepared call graph, after its bodies have finished
   gimplification. Uses stable summaries, not descriptors or GIMPLE operands. */
extern bool newtm_clone_require_region (tree);
/* Compare a local dependency list with the still-live preflight snapshot. */
extern bool newtm_clone_matches_preflight (tree, tree);
/* Check a source call and snapshot its prepared dependency while descriptors
   are live. Later graph checks use declarations, never retired descriptors. */
extern const char *newtm_clone_check_call (const gimple *);
extern tree newtm_clone_parameter_binding (tree, tree);
extern tree newtm_clone_captures (tree);
extern tree newtm_clone_capture_value (tree, tree);
extern tree newtm_clone_capture_object_binding (tree, tree);
extern tree newtm_clone_pointer_root (tree, tree);
extern tree newtm_clone_pointer_key (tree, tree);
extern tree newtm_clone_result_binding (const gimple *);
extern tree newtm_clone_identity (tree);
extern tree newtm_clone_storage (tree);
extern tree newtm_clone_field (tree, unsigned);
extern void newtm_clone_scan_results (tree);
extern void newtm_clone_finish (tree);
extern void newtm_clone_control (tree);
extern void newtm_clone_certify_regions ();
/* Produces temporary current facts, never an approval: records are
   (function, component mask, dependency mask, current byte snapshot).
   Includes the actual controller and its outlining producer ancestors. */
extern bool newtm_clone_verify_current (tree, tree *, tree *);
#endif
