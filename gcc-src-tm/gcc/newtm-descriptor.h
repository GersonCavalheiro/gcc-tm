/* Checked identity descriptors, before functional transaction lowering.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_DESCRIPTOR_H
#define GCC_NEWTM_DESCRIPTOR_H

enum newtm_descriptor_role
{
  NEWTM_DESC_READ, NEWTM_DESC_WRITE, NEWTM_DESC_REGISTER,
  NEWTM_DESC_ADDRESS, NEWTM_DESC_ARGUMENT, NEWTM_DESC_RETURN,
  NEWTM_DESC_SOURCE
};
enum newtm_descriptor_state
{
  NEWTM_DESC_REGISTERED, NEWTM_DESC_PRIVATE, NEWTM_DESC_UNREGISTERED,
  NEWTM_DESC_INCOMING, NEWTM_DESC_RESULT, NEWTM_DESC_UNKNOWN,
  NEWTM_DESC_SOURCE_ONLY, NEWTM_DESC_STALE
};
enum newtm_descriptor_field
{
  NEWTM_D_OBJECT, NEWTM_D_EXTENT, NEWTM_D_OFFSET, NEWTM_D_KIND,
  NEWTM_D_SIZE, NEWTM_D_VALID, NEWTM_D_STATE, NEWTM_D_ORIGIN,
  NEWTM_D_CHANNEL, NEWTM_D_KIND_VALUE, NEWTM_D_EXTENT_VALUE,
  NEWTM_D_LOGICAL, NEWTM_D_LOCAL_VALID, NEWTM_D_FIELDS
};
/* Only REGISTERED/PRIVATE/UNREGISTERED have materialized object/extent/kind
   fields. INCOMING/RESULT are explicit binding channels, never invented values.
   VALID is structural/range validity, not authorization to access memory.
   Context registration and reservations remain runtime responsibilities.
   Logical references are metadata, not recipes to evaluate. Descriptors expire
   before omplower; copied instructions are STALE until rebuilt after remapping.
   A consumer must resolve/reject UNKNOWN/SOURCE_ONLY and incomplete channels,
   and branch on validity before forming an address or performing an access. */
extern void newtm_descriptor_note_registration (tree, tree);
extern tree newtm_descriptor_registration_type (tree);
extern void newtm_descriptor_note_function (tree);
extern void newtm_descriptor_function (tree);
extern tree newtm_descriptor_entries (const gimple *);
extern tree newtm_descriptor_find (const gimple *, newtm_descriptor_role, unsigned = 0);
extern void newtm_descriptor_copy (const gimple *, gimple *);
extern void newtm_descriptor_retire (gimple_seq);
extern void newtm_descriptor_reset ();
#endif
