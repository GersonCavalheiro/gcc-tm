/* Access preparation within the descriptor lifetime; no persistent recipes.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_ACCESS_H
#define GCC_NEWTM_ACCESS_H
#include "newtm-descriptor.h"

enum newtm_access_storage
{
  NEWTM_ACCESS_REJECTED, NEWTM_ACCESS_REGISTERED, NEWTM_ACCESS_PRIVATE,
  NEWTM_ACCESS_DYNAMIC
};
/* All trees are borrowed from this activation before tree-nested/omplower.
   REFERENCE is the evaluated GIMPLE memory operand, never a source witness.
   Form its address only after VALID, before the eventual read/write call.
   REGISTERED still requires runtime registration; PRIVATE is compiler proof.
   DYNAMIC requires dispatch on the validated per-activation storage tag. */
struct newtm_access
{
  tree reference, object, extent, offset, size, kind, valid, storage;
  newtm_access_storage disposition;
  const char *reason;
};
/* Cannot be reopened after preparation or after gimplification. */
extern bool newtm_access_begin (tree);
extern void newtm_access_end ();
/* Scalar facts only; safe after remapping, not executable access plans. */
extern unsigned newtm_access_count (tree, newtm_access_storage);
extern bool newtm_access_prepared_p (tree);
extern bool newtm_access_describe (const gimple *, newtm_descriptor_role,
                                   unsigned, newtm_access *);
/* Refuses incomplete functions atomically. No partial executable body. */
extern bool newtm_access_preflight (tree, const char **);
#endif
