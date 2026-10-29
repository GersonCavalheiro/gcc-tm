/* Copyright (C) 2026 Free Software Foundation, Inc.

   This file is part of the GNU Offloading and Multi Processing Library
   (libgomp).

   Libgomp is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

/* Private storage for the experimental transaction runtime.
   Ownership and algorithms: OPENMP_TRANSACTION_CONTEXT_PROPOSAL.md.
   Not a public ABI.  All context calls belong to the creating execution.  */
#ifndef LIBGOMP_NEWTM_CONTEXT_H
#define LIBGOMP_NEWTM_CONTEXT_H

#include "newtm-transaction.h"
#include "newtm-hash.h"

enum newtm_reservation
{
  NEWTM_RESERVATION_NONE,
  NEWTM_RESERVATION_READ,
  NEWTM_RESERVATION_WRITE
};

enum newtm_failure_reason
{
  NEWTM_REASON_NONE,
  NEWTM_REASON_CONFLICT,
  NEWTM_REASON_UNREGISTERED,
  NEWTM_REASON_BOUNDS,
  NEWTM_REASON_INCOMPATIBLE_REGISTRATION,
  NEWTM_REASON_INVALID_OPERATION,
  NEWTM_REASON_OVERLAPPING_REGISTRATION
};

struct newtm_interval
{
  /* Half-open [begin,end), nonempty, sorted, disjoint, adjacent nodes merged.
     Each list exclusively owns its nodes; modified is a subset of valid. */
  size_t begin, end;
  struct newtm_interval *next;
};

struct newtm_scalar_value
{
  unsigned char *bytes;
  bool valid, modified;
};

struct newtm_aggregate_value
{
  /* Full extent allocated lazily; only valid ranges are initialized. */
  unsigned char *bytes;
  struct newtm_interval *valid, *modified;
};

struct newtm_record
{
  struct newtm_record *next;
  /* Global admission index, distinct from hash references and reservations.
     Protected by newtm_registry_lock; retained through final hash release. */
  struct newtm_record *registry_next;
  bool indexed;
  struct newtm_context *context;
  struct newtm_hash_entry *entry;
  const void *object;
  size_t size;
  enum newtm_object_kind kind;
  enum newtm_reservation reservation;
  struct newtm_record *reader_next;
  union
  {
    struct newtm_scalar_value scalar;
    struct newtm_aggregate_value aggregate;
  } value;
};

struct newtm_object_policy
{
  struct newtm_record *readers, *writer;
};

struct newtm_failure_info
{
  enum newtm_failure_reason reason;
  const void *object;
  size_t offset, size;
};

struct gomp_thread;
struct newtm_context
{
  enum newtm_attempt_state state;
  size_t depth;
  struct newtm_hash_owner_lease lease;
  struct newtm_record *records;
  struct newtm_failure_info failure;
  /* Canonical slot and execution identity reject copied/foreign consumers.
     They do not validate arbitrary dangling context addresses.  */
  struct newtm_context **owning_slot;
  struct gomp_thread *owner;
};

/* Reserve a registered canonical OBJECT in the active owned CTX.  MODE must
   be READ or WRITE.  Return access OK/CONFLICT/INVALID, explicitly translating
   lifecycle state.  An unregistered object/invalid mode condemns a valid active
   attempt as INVALID; already condemned attempts preserve their cause.
   Metadata are allocated lazily outside the bucket, then installed once under
   entry_visit.  Free any losing candidate outside the bucket.  READ shares with
   readers, WRITE excludes other participants; a sole reader may promote.
   Repeated requests reuse the reservation; READ never downgrades own WRITE.
   On conflict condemn only the requester, preserving all its old reservations
   for explicit discard.  No waiting, value access, buffers or publication.
   Retain the record's hash reference and owner lease through every visit and
   candidate cleanup; no bucket lock escapes a return.  OOM is fatal.  Caller
   serializes its own context, without task scheduling or lifecycle reentry.
   This private policy helper is not a compiler-facing GOMP ABI. */
extern enum newtm_access_status newtm_reserve
  (struct newtm_context *, const void *object, enum newtm_reservation mode)
  __attribute__ ((visibility ("hidden")));

/* External failure epilogue, after propagation through prepared frames and
   supported cleanups.  Require the canonical owned live slot, no lifecycle
   reentry, and an unpublished failed attempt (possibly nested).  Save cause
   and diagnostic by value, discard every level exactly once, then diagnose
   INVALID fatally or wait for a conflict.  ATTEMPT is the caller-owned positive
   saturated consecutive-conflict count, not stored in the discarded context.
   Zero on a conflict is permanent misuse, cleaned before fatal diagnosis.
   Return CONFLICT only after complete discard and backoff; this authorizes the
   external controller to restart the WHOLE body, never an inner frame.  Never
   return OK or retry INVALID.  Invalid/foreign slots and publication/teardown
   reentry are fatal misuse; do not speculatively consume someone else's slot.
   No public GOMP ABI or nonlocal jump is introduced by this helper. */
extern enum newtm_lifecycle_status newtm_after_failure
  (struct newtm_context **slot, size_t attempt)
  __attribute__ ((visibility ("hidden")));

/* Internal outer controller for explicitly prepared bodies, not a GOMP ABI.
   BODY borrows the live context for one whole attempt.  It registers objects,
   checks every operation and propagates failures through prepared calls before
   returning.  It must balance successful nested begin/end, never consume the
   outer slot, retain context/buffer pointers, throw, longjmp or perform external
   effects.  Automatic private temporaries are recreated on each invocation.
   ARGUMENT is stable caller-owned input/control storage across attempts; this
   controller cannot roll back arbitrary writes to it.  Results are published
   through registered objects by successful outer end, not speculative outputs.
   OK confirms, CONFLICT repeats the WHOLE body after discard/backoff, INVALID
   (including unknown result tags or unbalanced nesting) cleans then diagnoses
   fatally.  Effective context failure dominates a fabricated OK or CONFLICT.
   Only call at an external boundary, not recursively/from a live attempt;
   nested regions/functions share this context and return explicit status.
   No scheduling/reentry; program object lifetime must span each attempt. */
typedef enum newtm_lifecycle_status (*newtm_body_fn)
  (struct newtm_context *, void *argument);
extern enum newtm_lifecycle_status newtm_execute (newtm_body_fn, void *argument)
  __attribute__ ((visibility ("hidden")));

/* Called only by final hash release, outside the bucket, after unlinking.
   Require no readers/writer.  The owner's lease covers the entire callback. */
extern void newtm_policy_destroy (void *)
  __attribute__ ((visibility ("hidden")));

#endif
