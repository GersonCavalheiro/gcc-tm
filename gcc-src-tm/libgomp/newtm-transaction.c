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

/* Private values, reservations and outer confirmation are operational through
   internal interfaces.  The GOMP bridge still only traces objects. */

#include "libgomp.h"
#include "newtm-context.h"
#include "doacross.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Admit ranges atomically across all attempts, including registrations whose
   hash acquisition is still pending.  Never call hash, allocation, diagnostics
   or policy callbacks under this mutex.  Each indexed node is stable and its
   owner's lease covers admission through removal after final hash cleanup. */
static gomp_mutex_t newtm_registry_lock;
static struct newtm_record *newtm_registry;

static void __attribute__ ((constructor))
newtm_registry_initialize (void)
{
#if !GOMP_MUTEX_INIT_0
  gomp_mutex_init (&newtm_registry_lock);
#endif
}

static enum newtm_failure_reason
newtm_registry_admit (struct newtm_record *record)
{
  uintptr_t address = (uintptr_t) record->object;
  enum newtm_failure_reason reason = NEWTM_REASON_NONE;
  gomp_mutex_lock (&newtm_registry_lock);
  for (struct newtm_record *p = newtm_registry; p; p = p->registry_next)
    {
      uintptr_t other = (uintptr_t) p->object;
      if (address == other)
        {
          if (record->size != p->size || record->kind != p->kind)
            { reason = NEWTM_REASON_INCOMPATIBLE_REGISTRATION; break; }
        }
      else if (address < other ? other - address < record->size
               : address - other < p->size)
        { reason = NEWTM_REASON_OVERLAPPING_REGISTRATION; break; }
    }
  if (reason == NEWTM_REASON_NONE)
    {
      record->registry_next = newtm_registry;
      newtm_registry = record;
      record->indexed = true;
    }
  gomp_mutex_unlock (&newtm_registry_lock);
  return reason;
}

static void
newtm_registry_remove (struct newtm_record *record)
{
  if (!record->indexed) return; /* Private cleanup tests may seed unindexed nodes. */
  gomp_mutex_lock (&newtm_registry_lock);
  struct newtm_record **link = &newtm_registry;
  while (*link && *link != record) link = &(*link)->registry_next;
  bool found = *link != NULL;
  if (found)
    {
      *link = record->registry_next;
      record->registry_next = NULL;
      record->indexed = false;
    }
  gomp_mutex_unlock (&newtm_registry_lock);
  if (!found) gomp_fatal ("transaction range index invariant violated");
}

/* Algorithm: compare the creating execution and canonical live slot.
   Require valid live context storage; never use this to probe arbitrary or
   dangling addresses.  No locks: only the owning execution may call lifecycle. */
static bool
newtm_context_owned (struct newtm_context *ctx)
{
  return ctx && ctx->owner == gomp_thread () && ctx->owning_slot
    && *ctx->owning_slot == ctx && ctx->lease.active && ctx->lease.table;
}

/* Algorithm: reject permanent failure first, then conflict, otherwise require
   ACTIVE.  Operation results and attempt states are distinct enum domains. */
static enum newtm_lifecycle_status
newtm_context_status (struct newtm_context *ctx)
{
  if (ctx->state == NEWTM_ATTEMPT_ACTIVE)
    return NEWTM_LIFECYCLE_OK;
  if (ctx->state == NEWTM_ATTEMPT_CONFLICTED)
    return NEWTM_LIFECYCLE_CONFLICT;
  return NEWTM_LIFECYCLE_INVALID;
}

/* Algorithm: initialize OUTERMOST=false; validate owning slot and lease.
   Outer entry allocates/initializes privately, transfers the lease by clearing
   its source, and publishes a complete context.  Nested entry requires no new
   lease, preserves the existing participation, and increments checked depth.
   Returning errors never consume a lease or publish a partial context. */
enum newtm_lifecycle_status
newtm_begin (struct newtm_hash_owner_lease *lease, struct newtm_context **slot,
             bool *outermost)
{
  printf ("[newtm] newtm_begin: lease=%p, slot=%p, outermost=%p\n",
          (void *) lease, (void *) slot, (void *) outermost);
  fflush (stdout);
  if (outermost)
    *outermost = false;
  if (!slot || !outermost)
    {
      if (slot && newtm_context_owned (*slot)
          && (*slot)->owning_slot == slot)
        return newtm_fail (*slot, NEWTM_ACCESS_INVALID);
      return NEWTM_LIFECYCLE_INVALID;
    }
  if (*slot)
    {
      struct newtm_context *ctx = *slot;
      if (!newtm_context_owned (ctx) || ctx->owning_slot != slot)
        return NEWTM_LIFECYCLE_INVALID;
      if (lease || ctx->depth == SIZE_MAX || ctx->depth == 0)
        return newtm_fail (ctx, NEWTM_ACCESS_INVALID);
      enum newtm_lifecycle_status status = newtm_context_status (ctx);
      if (status != NEWTM_LIFECYCLE_OK)
        return status;
      ++ctx->depth;
      return NEWTM_LIFECYCLE_OK;
    }
  if (!lease || !lease->active || !lease->table)
    return NEWTM_LIFECYCLE_INVALID;
  struct newtm_context *ctx = gomp_malloc_cleared (sizeof (*ctx));
  ctx->state = NEWTM_ATTEMPT_ACTIVE;
  ctx->depth = 1;
  ctx->owner = gomp_thread ();
  ctx->owning_slot = slot;
  ctx->lease = *lease;
  lease->active = false;
  lease->table = NULL;
  *slot = ctx;
  *outermost = true;
  return NEWTM_LIFECYCLE_OK;
}

/* Record a permanent context error without overwriting an earlier one.
   Only call with an owned unpublished context; retain resources for discard. */
static enum newtm_lifecycle_status
newtm_context_invalid (struct newtm_context *ctx, const void *object,
                       size_t size, enum newtm_failure_reason reason)
{
  bool already_invalid = ctx->state == NEWTM_ATTEMPT_INVALID;
  enum newtm_lifecycle_status status = newtm_fail (ctx, NEWTM_ACCESS_INVALID);
  if (!already_invalid)
    {
      ctx->failure.reason = reason;
      ctx->failure.object = object;
      ctx->failure.offset = 0;
      ctx->failure.size = size;
    }
  return status;
}

/* Algorithm: validate ownership/state and object description, then search the
   attempt's stable list by exact canonical address.  Compatible duplicates
   return OK without another hash acquisition, including across nested levels.
   Prepare a zeroed private record, acquire one physical hash reference, and
   link only after success.  On returned hash failure free the candidate and
   condemn the attempt without changing its list or depth.  No fallible step
   follows acquisition: the reference belongs to the linked record until
   discard.  Never read OBJECT, allocate value/policy storage or obtain a
   reservation.  Allocation failure is fatal; logs occur outside locks. */
enum newtm_lifecycle_status
newtm_register (struct newtm_context *ctx, const void *object, size_t size,
                enum newtm_object_kind kind)
{
  printf ("[newtm] newtm_register: ctx=%p, object=%p, size=%zu, kind=%d\n",
	  (void *) ctx, (void *) object, size, (int) kind);
  fflush (stdout);
  if (!newtm_context_owned (ctx))
    return NEWTM_LIFECYCLE_INVALID;
  enum newtm_lifecycle_status status = newtm_context_status (ctx);
  if (status != NEWTM_LIFECYCLE_OK)
    return status;
  if (!ctx->depth || !object || !size
      || size - 1 > UINTPTR_MAX - (uintptr_t) object
      || (kind != NEWTM_OBJECT_SCALAR && kind != NEWTM_OBJECT_AGGREGATE))
    return newtm_context_invalid (ctx, object, size,
                                  NEWTM_REASON_INVALID_OPERATION);
  for (struct newtm_record *record = ctx->records; record;
       record = record->next)
    if (record->object == object)
      {
        if (record->size != size || record->kind != kind)
          return newtm_context_invalid
            (ctx, object, size, NEWTM_REASON_INCOMPATIBLE_REGISTRATION);
        return NEWTM_LIFECYCLE_OK;
      }

  struct newtm_record *record = gomp_malloc_cleared (sizeof (*record));
  record->context = ctx;
  record->object = object;
  record->size = size;
  record->kind = kind;
  enum newtm_failure_reason reason = newtm_registry_admit (record);
  if (reason != NEWTM_REASON_NONE)
    {
      free (record);
      return newtm_context_invalid (ctx, object, size, reason);
    }
  enum newtm_hash_status acquired
    = newtm_hash_acquire (ctx->lease.table, object, size, &record->entry);
  if (acquired != NEWTM_HASH_OK)
    {
      newtm_registry_remove (record);
      free (record);
      return newtm_context_invalid
        (ctx, object, size, acquired == NEWTM_HASH_SIZE_MISMATCH
         ? NEWTM_REASON_INCOMPATIBLE_REGISTRATION
         : NEWTM_REASON_INVALID_OPERATION);
    }
  record->reservation = NEWTM_RESERVATION_NONE;
  record->next = ctx->records;
  ctx->records = record;
  return NEWTM_LIFECYCLE_OK;
}

struct newtm_reserve_argument
{
  struct newtm_record *record;
  enum newtm_reservation mode;
  struct newtm_object_policy *candidate;
  bool needs_policy;
  enum newtm_access_status status;
};

/* Under the bucket only: install a prepared policy if still absent, then
   check and grant the reservation atomically.  A first visit without metadata
   asks the caller to allocate outside the lock.  A second visit rechecks all
   state; concurrent installation leaves our candidate unused.  Never allocate,
   log, wait, free or access program values here.  Failed promotion keeps READ.
   Existing reservations are changed only on successful promotion/grant. */
static void
newtm_reserve_visit (const void *object, size_t size, void **data, void *argument)
{
  struct newtm_reserve_argument *arg = argument;
  struct newtm_record *record = arg->record;
  struct newtm_object_policy *policy = *data;
  arg->needs_policy = false;
  arg->status = NEWTM_ACCESS_INVALID;
  if (object != record->object || size != record->size)
    return;
  if (!policy)
    {
      if (record->reservation != NEWTM_RESERVATION_NONE)
        return;
      if (!arg->candidate)
        { arg->needs_policy = true; return; }
      policy = arg->candidate;
      *data = policy;
      arg->candidate = NULL;
    }
  if (policy->writer && policy->readers)
    return;
  if (record->reservation == NEWTM_RESERVATION_WRITE)
    {
      if (policy->writer == record && !policy->readers)
        arg->status = NEWTM_ACCESS_OK;
      return;
    }
  if (record->reservation == NEWTM_RESERVATION_READ)
    {
      if (policy->writer)
        return;
      struct newtm_record *reader = policy->readers;
      while (reader && reader != record)
        reader = reader->reader_next;
      if (!reader)
        return;
      if (arg->mode == NEWTM_RESERVATION_READ)
        { arg->status = NEWTM_ACCESS_OK; return; }
      if (policy->readers != record || record->reader_next)
        { arg->status = NEWTM_ACCESS_CONFLICT; return; }
      policy->readers = NULL;
      policy->writer = record;
      record->reader_next = NULL;
      record->reservation = NEWTM_RESERVATION_WRITE;
    }
  else if (record->reservation == NEWTM_RESERVATION_NONE)
    {
      if (policy->writer
          || (arg->mode == NEWTM_RESERVATION_WRITE && policy->readers))
        { arg->status = NEWTM_ACCESS_CONFLICT; return; }
      if (arg->mode == NEWTM_RESERVATION_READ)
        {
          record->reader_next = policy->readers;
          policy->readers = record;
        }
      else
        policy->writer = record;
      record->reservation = arg->mode;
    }
  else
    return;
  arg->status = NEWTM_ACCESS_OK;
}

/* Algorithm: validate owned ACTIVE context/mode and find the registered object.
   Visit under its bucket; if metadata are absent, prepare a zeroed candidate
   outside locks and visit again.  Free an unused candidate after unlock.
   Translate failure explicitly and condemn only the requesting context after
   unlock, retaining existing reservations/references/depth for discard. */
enum newtm_access_status
newtm_reserve (struct newtm_context *ctx, const void *object,
               enum newtm_reservation mode)
{
  printf ("[newtm] newtm_reserve: ctx=%p, object=%p, mode=%d\n",
          (void *) ctx, (void *) object, (int) mode);
  fflush (stdout);
  if (!newtm_context_owned (ctx))
    return NEWTM_ACCESS_INVALID;
  enum newtm_lifecycle_status state = newtm_context_status (ctx);
  if (state != NEWTM_LIFECYCLE_OK)
    return state == NEWTM_LIFECYCLE_CONFLICT
      ? NEWTM_ACCESS_CONFLICT : NEWTM_ACCESS_INVALID;
  if (!ctx->depth || !object
      || (mode != NEWTM_RESERVATION_READ && mode != NEWTM_RESERVATION_WRITE))
    {
      newtm_context_invalid (ctx, object, 0, NEWTM_REASON_INVALID_OPERATION);
      return NEWTM_ACCESS_INVALID;
    }
  struct newtm_record *record = ctx->records;
  while (record && record->object != object)
    record = record->next;
  if (!record)
    {
      newtm_context_invalid (ctx, object, 0, NEWTM_REASON_UNREGISTERED);
      return NEWTM_ACCESS_INVALID;
    }
  struct newtm_reserve_argument arg
    = { record, mode, NULL, false, NEWTM_ACCESS_INVALID };
  newtm_hash_entry_visit (ctx->lease.table, record->entry,
                         newtm_reserve_visit, &arg);
  if (arg.needs_policy)
    {
      arg.candidate = gomp_malloc_cleared (sizeof (*arg.candidate));
      newtm_hash_entry_visit (ctx->lease.table, record->entry,
                             newtm_reserve_visit, &arg);
    }
  free (arg.candidate);
  if (arg.status == NEWTM_ACCESS_INVALID)
    newtm_context_invalid (ctx, object, record->size,
                          NEWTM_REASON_INVALID_OPERATION);
  else if (arg.status == NEWTM_ACCESS_CONFLICT)
    {
      newtm_fail (ctx, NEWTM_ACCESS_CONFLICT);
      ctx->failure.object = object;
      ctx->failure.offset = 0;
      ctx->failure.size = record->size;
    }
  return arg.status;
}

static void newtm_teardown (struct newtm_context **, struct newtm_context *);

/* Validate normalized ranges without touching any program values.  Monotonic
   nonadjacent endpoints also reject a cycle in an otherwise live node list. */
static bool
newtm_intervals_valid (struct newtm_interval *list, size_t size)
{
  size_t previous = 0;
  bool first = true;
  for (; list; list = list->next)
    {
      if (list->begin >= list->end || list->end > size
          || (!first && previous >= list->begin))
        return false;
      first = false;
      previous = list->end;
    }
  return true;
}

/* Private preparation: check ownership, representation and modified subset.
   Both interval lists must own distinct nodes.  No allocation, value reads or
   publication.  Live context/record/list storage is a caller precondition. */
static bool
newtm_record_ready (struct newtm_context *ctx, struct newtm_record *r)
{
  if (r->context != ctx || !r->indexed || !r->entry || !r->object || !r->size
      || (r->reservation != NEWTM_RESERVATION_NONE
          && r->reservation != NEWTM_RESERVATION_READ
          && r->reservation != NEWTM_RESERVATION_WRITE)
      || (r->reservation != NEWTM_RESERVATION_READ && r->reader_next))
    return false;
  bool valid, modified;
  if (r->kind == NEWTM_OBJECT_SCALAR)
    {
      valid = r->value.scalar.valid;
      modified = r->value.scalar.modified;
      if ((valid || modified) && !r->value.scalar.bytes)
        return false;
      if (modified && !valid)
        return false;
    }
  else if (r->kind == NEWTM_OBJECT_AGGREGATE)
    {
      struct newtm_aggregate_value *v = &r->value.aggregate;
      if (!newtm_intervals_valid (v->valid, r->size)
          || !newtm_intervals_valid (v->modified, r->size))
        return false;
      valid = v->valid != NULL;
      modified = v->modified != NULL;
      if ((valid || modified) && !v->bytes)
        return false;
      struct newtm_interval *cover = v->valid;
      for (struct newtm_interval *m = v->modified; m; m = m->next)
        {
          while (cover && cover->end < m->end) cover = cover->next;
          if (!cover || cover->begin > m->begin)
            return false;
          for (struct newtm_interval *p = v->valid; p; p = p->next)
            if (p == m) return false;
        }
    }
  else
    return false;
  return (!valid || r->reservation != NEWTM_RESERVATION_NONE)
    && (!modified || r->reservation == NEWTM_RESERVATION_WRITE);
}

struct newtm_commit_check
{
  struct newtm_record *record;
  bool ready;
};

/* Under the bucket: verify the stable identity and our exact reservation.
   Other readers may leave/join after this visit, but cannot revoke ours.
   This callback does not modify policy or private state, allocate or log. */
static void
newtm_commit_visit (const void *object, size_t size, void **data, void *argument)
{
  struct newtm_commit_check *arg = argument;
  struct newtm_record *r = arg->record;
  struct newtm_object_policy *p = *data;
  arg->ready = false;
  if (object != r->object || size != r->size || (p && p->writer && p->readers))
    return;
  size_t matches = 0;
  if (p)
    for (struct newtm_record *reader = p->readers; reader;
         reader = reader->reader_next)
      if (reader == r) ++matches;
  if (r->reservation == NEWTM_RESERVATION_NONE)
    arg->ready = !matches && (!p || p->writer != r);
  else if (r->reservation == NEWTM_RESERVATION_READ)
    arg->ready = p && !p->writer && matches == 1;
  else if (r->reservation == NEWTM_RESERVATION_WRITE)
    arg->ready = p && p->writer == r && !p->readers;
}

/* Validate canonical ownership and propagate condemned state.  Inner exit
   reduces depth only.  Outer exit prepares/validates EVERY record and its
   reservation before PUBLISHING.  Then copy only modified values/ranges,
   retaining ALL reservations until ALL objects are published.  Publication
   allocates nothing and has no recoverable failure path.  Detach and tear down
   only afterwards; OK is returned after the final owner lease release.
   Object lifetime/writability and nonoverlap remain caller preconditions. */
enum newtm_lifecycle_status
newtm_end (struct newtm_context **slot)
{
  printf ("[newtm] newtm_end: slot=%p\n", (void *) slot);
  fflush (stdout);
  if (!slot || !newtm_context_owned (*slot) || (*slot)->owning_slot != slot)
    return NEWTM_LIFECYCLE_INVALID;
  struct newtm_context *ctx = *slot;
  enum newtm_lifecycle_status status = newtm_context_status (ctx);
  if (status != NEWTM_LIFECYCLE_OK)
    return status;
  if (!ctx->depth)
    return newtm_fail (ctx, NEWTM_ACCESS_INVALID);
  if (ctx->depth > 1)
    {
      --ctx->depth;
      return NEWTM_LIFECYCLE_OK;
    }
  for (struct newtm_record *r = ctx->records; r; r = r->next)
    {
      if (!newtm_record_ready (ctx, r))
        return newtm_context_invalid (ctx, r->object, r->size,
                                      NEWTM_REASON_INVALID_OPERATION);
      struct newtm_commit_check check = { r, false };
      newtm_hash_entry_visit (ctx->lease.table, r->entry,
                             newtm_commit_visit, &check);
      if (!check.ready)
        return newtm_context_invalid (ctx, r->object, r->size,
                                      NEWTM_REASON_INVALID_OPERATION);
    }
  ctx->state = NEWTM_ATTEMPT_PUBLISHING;
  for (struct newtm_record *r = ctx->records; r; r = r->next)
    {
      if (r->kind == NEWTM_OBJECT_SCALAR)
        {
          if (r->value.scalar.modified)
            memcpy ((void *) r->object, r->value.scalar.bytes, r->size);
        }
      else
        for (struct newtm_interval *p = r->value.aggregate.modified; p;
             p = p->next)
          memcpy ((unsigned char *) r->object + p->begin,
                  r->value.aggregate.bytes + p->begin, p->end - p->begin);
    }
  newtm_teardown (slot, ctx);
  return NEWTM_LIFECYCLE_OK;
}

/* Algorithm: validate live ownership and unpublished state.  INVALID dominates
   all conflicts; preserve the first permanent cause.  Otherwise record the
   requested failure and return its explicit lifecycle translation.  Do not
   change depth, release resources, jump or emit the final fatal diagnostic. */
enum newtm_lifecycle_status
newtm_fail (struct newtm_context *ctx, enum newtm_access_status cause)
{
  printf ("[newtm] newtm_fail: ctx=%p, cause=%d\n",
          (void *) ctx, (int) cause);
  fflush (stdout);
  if (!newtm_context_owned (ctx)
      || ctx->state == NEWTM_ATTEMPT_PUBLISHING
      || ctx->state == NEWTM_ATTEMPT_CLOSED)
    return NEWTM_LIFECYCLE_INVALID;
  if (ctx->state == NEWTM_ATTEMPT_INVALID)
    return NEWTM_LIFECYCLE_INVALID;
  if (cause == NEWTM_ACCESS_CONFLICT)
    {
      ctx->state = NEWTM_ATTEMPT_CONFLICTED;
      ctx->failure.reason = NEWTM_REASON_CONFLICT;
      return NEWTM_LIFECYCLE_CONFLICT;
    }
  ctx->state = NEWTM_ATTEMPT_INVALID;
  ctx->failure.reason = NEWTM_REASON_INVALID_OPERATION;
  return NEWTM_LIFECYCLE_INVALID;
}

/* Algorithm: final hash release has already unlinked the entry and dropped its
   bucket mutex.  Check the no-participants invariant, then free only shared
   policy storage.  Residual participants are fatal, never forcibly removed.
   The caller retains its owner lease throughout this callback. */
void
newtm_policy_destroy (void *data)
{
  struct newtm_object_policy *policy = data;
  if (!policy)
    return;
  if (policy->readers || policy->writer)
    gomp_fatal ("transaction policy destroyed with active reservations");
  free (policy);
}

struct newtm_withdraw_argument
{
  struct newtm_record *record;
  bool invalid;
};

/* Algorithm, under the bucket: locate/remove exactly this reader, or clear
   this writer.  Set a flag for structural mismatch, diagnosing only after
   entry_visit unlocks.  Do not allocate, log, wait, recurse or free a record. */
static void
newtm_withdraw (const void *object, size_t size, void **data, void *argument)
{
  struct newtm_withdraw_argument *arg = argument;
  struct newtm_record *record = arg->record;
  struct newtm_object_policy *policy = *data;
  if (object != record->object || size != record->size || !policy)
    { arg->invalid = true; return; }
  if (record->reservation == NEWTM_RESERVATION_READ)
    {
      struct newtm_record **link = &policy->readers;
      while (*link && *link != record)
        link = &(*link)->reader_next;
      if (!*link || policy->writer)
        { arg->invalid = true; return; }
      *link = record->reader_next;
    }
  else if (record->reservation == NEWTM_RESERVATION_WRITE)
    {
      if (policy->writer != record || policy->readers)
        { arg->invalid = true; return; }
      policy->writer = NULL;
    }
  else
    { arg->invalid = true; return; }
  record->reader_next = NULL;
  record->reservation = NEWTM_RESERVATION_NONE;
}

/* Algorithm: walk an exclusively owned interval list, save next before free.
   Lists must be acyclic and independent; no bucket or program memory involved. */
static void
newtm_free_intervals (struct newtm_interval *interval)
{
  while (interval)
    {
      struct newtm_interval *next = interval->next;
      free (interval);
      interval = next;
    }
}

/* Shared consuming cleanup, called only after validation (and, for commit,
   completed publication).  Mark CLOSED and detach slot before any release.
   Withdraw reservations under buckets;
   release private buffers, interval lists, hash references and records outside
   them.  Transfer the lease to a local token, free context, then release the
   owner participation LAST.  No publication, retry, user callback or implicit
   fatal for the saved INVALID cause: outer control retains that diagnosis. */
static void
newtm_teardown (struct newtm_context **slot, struct newtm_context *ctx)
{
  ctx->state = NEWTM_ATTEMPT_CLOSED;
  ctx->depth = 0;
  *slot = NULL;
  while (ctx->records)
    {
      struct newtm_record *record = ctx->records;
      ctx->records = record->next;
      if (record->reservation != NEWTM_RESERVATION_NONE)
        {
          struct newtm_withdraw_argument arg = { record, false };
          newtm_hash_entry_visit (ctx->lease.table, record->entry,
                                  newtm_withdraw, &arg);
          if (arg.invalid)
            gomp_fatal ("transaction reservation cleanup invariant violated");
        }
      if (record->kind == NEWTM_OBJECT_SCALAR)
        free (record->value.scalar.bytes);
      else if (record->kind == NEWTM_OBJECT_AGGREGATE)
        {
          free (record->value.aggregate.bytes);
          newtm_free_intervals (record->value.aggregate.valid);
          newtm_free_intervals (record->value.aggregate.modified);
        }
      else
        gomp_fatal ("transaction record has invalid representation kind");
      newtm_hash_release (ctx->lease.table, record->entry);
      newtm_registry_remove (record);
      free (record);
    }
  struct newtm_hash_owner_lease lease = ctx->lease;
  ctx->lease.active = false;
  ctx->lease.table = NULL;
  free (ctx);
  if (newtm_hash_owner_release (&lease) != NEWTM_HASH_OK)
    gomp_fatal ("transaction owner participation release failed");
}

/* Explicit discard consumes an owned unpublished attempt without publication.
   Commit uses the same teardown only AFTER publishing every modified range.
   Detected precondition failures preserve the slot; teardown errors are fatal. */
enum newtm_lifecycle_status
newtm_discard (struct newtm_context **slot)
{
  printf ("[newtm] newtm_discard: slot=%p\n", (void *) slot);
  fflush (stdout);
  if (!slot || !newtm_context_owned (*slot) || (*slot)->owning_slot != slot)
    return NEWTM_LIFECYCLE_INVALID;
  struct newtm_context *ctx = *slot;
  if (ctx->state != NEWTM_ATTEMPT_ACTIVE
      && ctx->state != NEWTM_ATTEMPT_CONFLICTED
      && ctx->state != NEWTM_ATTEMPT_INVALID)
    return NEWTM_LIFECYCLE_INVALID;
  newtm_teardown (slot, ctx);
  return NEWTM_LIFECYCLE_OK;
}

/* External failure epilogue: validate exclusive ownership before consuming,
   make ACTIVE/zero-count conflict misuse permanently INVALID, save cause and
   diagnostic on this stack, then discard once.  Only afterwards emit fatal
   diagnosis or perform bounded backoff.  Never inspect the consumed context,
   publish values, restart an inner level or convert INVALID to a conflict. */
enum newtm_lifecycle_status
newtm_after_failure (struct newtm_context **slot, size_t attempt)
{
  printf ("[newtm] newtm_after_failure: slot=%p, attempt=%zu\n",
          (void *) slot, attempt);
  fflush (stdout);
  if (!slot || !newtm_context_owned (*slot) || (*slot)->owning_slot != slot)
    gomp_fatal ("transaction failure control requires an owned live slot");
  struct newtm_context *ctx = *slot;
  if (ctx->state == NEWTM_ATTEMPT_PUBLISHING
      || ctx->state == NEWTM_ATTEMPT_CLOSED)
    gomp_fatal ("transaction failure control during publication or teardown");
  if (ctx->state != NEWTM_ATTEMPT_CONFLICTED
      && ctx->state != NEWTM_ATTEMPT_INVALID)
    newtm_fail (ctx, NEWTM_ACCESS_INVALID);
  if (ctx->state == NEWTM_ATTEMPT_CONFLICTED && !attempt)
    newtm_fail (ctx, NEWTM_ACCESS_INVALID);
  enum newtm_lifecycle_status cause = newtm_context_status (ctx);
  struct newtm_failure_info failure = ctx->failure;
  if (newtm_discard (slot) != NEWTM_LIFECYCLE_OK)
    gomp_fatal ("transaction failure cleanup did not complete");
  if (cause != NEWTM_LIFECYCLE_CONFLICT)
    gomp_fatal ("transaction invalid attempt (reason %d, object %p, "
                "offset %zu, size %zu)", (int) failure.reason,
                (void *) failure.object, failure.offset, failure.size);
  if (newtm_backoff (attempt) != NEWTM_LIFECYCLE_OK)
    gomp_fatal ("transaction backoff failed after cleanup");
  return NEWTM_LIFECYCLE_CONFLICT;
}

/* Outer prepared-body control: admit a fresh context, execute the entire body
   once, and translate its explicit status without consuming invalid results.
   End commits only at depth one.  Every failed attempt goes through exactly
   one consuming failure epilogue, with a saturated counter owned by this stack.
   Only a completely discarded conflict returns from that epilogue; restart at
   admission/body entry, never at an inner region or access.  No nonlocal jumps.
   A callback must obey the prepared-body contract in newtm-context.h. */
enum newtm_lifecycle_status
newtm_execute (newtm_body_fn body, void *argument)
{
  if (!body)
    gomp_fatal ("transaction execution requires a prepared body");
  size_t conflicts = 0;
  for (;;)
    {
      struct newtm_hash_owner_lease lease = { 0 };
      enum newtm_hash_status admitted = newtm_hash_owner_acquire (&lease);
      if (admitted != NEWTM_HASH_OK)
        gomp_fatal ("transaction execution admission failed (status %d)", admitted);
      struct newtm_context *ctx = NULL;
      bool outer;
      enum newtm_lifecycle_status entered = newtm_begin (&lease, &ctx, &outer);
      if (entered != NEWTM_LIFECYCLE_OK)
        {
          if (newtm_hash_owner_release (&lease) != NEWTM_HASH_OK)
            gomp_fatal ("transaction execution admission cleanup failed");
          gomp_fatal ("transaction execution entry failed (status %d)", entered);
        }
      enum newtm_lifecycle_status status = body (ctx, argument);
      if (!newtm_context_owned (ctx))
        gomp_fatal ("prepared transaction body consumed its owning context");
      if (status == NEWTM_LIFECYCLE_CONFLICT)
        newtm_fail (ctx, NEWTM_ACCESS_CONFLICT);
      else if (status != NEWTM_LIFECYCLE_OK)
        newtm_fail (ctx, NEWTM_ACCESS_INVALID);
      if (ctx->state == NEWTM_ATTEMPT_ACTIVE && ctx->depth != 1)
        newtm_fail (ctx, NEWTM_ACCESS_INVALID);
      if (ctx->state == NEWTM_ATTEMPT_ACTIVE)
        {
          status = newtm_end (&ctx);
          if (status == NEWTM_LIFECYCLE_OK)
            {
              if (ctx)
                gomp_fatal ("transaction confirmation did not consume its slot");
              return NEWTM_LIFECYCLE_OK;
            }
        }
      if (ctx->state == NEWTM_ATTEMPT_CONFLICTED && conflicts != SIZE_MAX)
        ++conflicts;
      status = newtm_after_failure (&ctx, conflicts);
      if (status != NEWTM_LIFECYCLE_CONFLICT || ctx)
        gomp_fatal ("transaction failure control did not complete discard");
    }
}

/* After complete discard only: exponentially increase bounded CPU relaxation
   from 64 to 65536 iterations.  Clamp before shifting, including SIZE_MAX,
   without shared mutable state, mutexes, allocation or task scheduling.
   These are relaxation iterations, not time units or a progress guarantee.
   The caller owns the conflict counter and the complete outer-block restart. */
enum newtm_lifecycle_status
newtm_backoff (size_t attempt)
{
  printf ("[newtm] newtm_backoff: attempt=%zu\n", attempt);
  fflush (stdout);
  if (!attempt)
    return NEWTM_LIFECYCLE_INVALID;
  size_t shift = attempt > 11 ? 10 : attempt - 1;
  size_t iterations = (size_t) 64 << shift;
  for (size_t i = 0; i < iterations; ++i)
    cpu_relax ();
  return NEWTM_LIFECYCLE_OK;
}

/* Byte ranges must be representable; subtraction avoids end-address overflow. */
static bool
newtm_ranges_overlap (const void *a, size_t asize, const void *b, size_t bsize)
{
  uintptr_t x = (uintptr_t) a, y = (uintptr_t) b;
  return x <= y ? y - x < asize : x - y < bsize;
}

/* Validate the known nonaliasing portion of the private transport contract.
   Buffer validity/extent and aliases outside this context remain caller
   preconditions.  Never dereference BUFFER to validate it. */
static bool
newtm_private_buffer (struct newtm_context *ctx, const void *buffer, size_t size)
{
  if (!buffer || size - 1 > UINTPTR_MAX - (uintptr_t) buffer
      || newtm_ranges_overlap (buffer, size, ctx, sizeof (*ctx))
      || newtm_ranges_overlap (buffer, size, ctx->owning_slot,
                               sizeof (*ctx->owning_slot)))
    return false;
  for (struct newtm_record *r = ctx->records; r; r = r->next)
    {
      if (newtm_ranges_overlap (buffer, size, r->object, r->size)
          || newtm_ranges_overlap (buffer, size, r, sizeof (*r)))
        return false;
      unsigned char *bytes = r->kind == NEWTM_OBJECT_SCALAR
        ? r->value.scalar.bytes : r->value.aggregate.bytes;
      if (bytes && newtm_ranges_overlap (buffer, size, bytes, r->size))
        return false;
      if (r->kind == NEWTM_OBJECT_AGGREGATE)
        {
          struct newtm_interval *lists[]
            = { r->value.aggregate.valid, r->value.aggregate.modified };
          for (unsigned int i = 0; i < 2; ++i)
            for (struct newtm_interval *p = lists[i]; p; p = p->next)
              if (newtm_ranges_overlap (buffer, size, p, sizeof (*p)))
                return false;
        }
    }
  return true;
}

/* Validate owner/state, registration, range and transport, then reserve the
   object before exposing its record to the data path.  Scalar accesses cover
   the entire registered value.  On failure preserve buffers/private values
   and old reservations, recording the requested subrange for diagnostics.
   Already condemned attempts preserve their original cause and subrange. */
static enum newtm_access_status
newtm_prepare_access (struct newtm_context *ctx, const void *object,
                      size_t offset, const void *buffer, size_t size,
                      enum newtm_reservation mode, struct newtm_record **out)
{
  *out = NULL;
  if (!newtm_context_owned (ctx))
    return NEWTM_ACCESS_INVALID;
  enum newtm_lifecycle_status state = newtm_context_status (ctx);
  if (state != NEWTM_LIFECYCLE_OK)
    return state == NEWTM_LIFECYCLE_CONFLICT
      ? NEWTM_ACCESS_CONFLICT : NEWTM_ACCESS_INVALID;
  enum newtm_failure_reason reason = NEWTM_REASON_INVALID_OPERATION;
  if (!ctx->depth || !object)
    goto invalid;
  struct newtm_record *record;
  for (record = ctx->records; record; record = record->next)
    if (record->object == object)
      break;
  if (!record)
    { reason = NEWTM_REASON_UNREGISTERED; goto invalid; }
  if (!size || offset > record->size || size > record->size - offset)
    { reason = NEWTM_REASON_BOUNDS; goto invalid; }
  if ((record->kind != NEWTM_OBJECT_SCALAR
       && record->kind != NEWTM_OBJECT_AGGREGATE)
      || (record->kind == NEWTM_OBJECT_SCALAR
          && (offset || size != record->size))
      || !newtm_private_buffer (ctx, buffer, size))
    goto invalid;
  enum newtm_access_status status = newtm_reserve (ctx, object, mode);
  if (status != NEWTM_ACCESS_OK)
    {
      /* Reserve reports the whole object; access diagnostics describe this
         operation.  Validation above established an ACTIVE owned context, so
         this is a newly detected failure, not an older cause to overwrite. */
      ctx->failure.offset = offset;
      ctx->failure.size = size;
      return status;
    }
  *out = record;
  return NEWTM_ACCESS_OK;
 invalid:
  newtm_context_invalid (ctx, object, size, reason);
  ctx->failure.offset = offset;
  return NEWTM_ACCESS_INVALID;
}

/* Sorted half-open lists contain no overlaps or adjacency. */
static bool
newtm_interval_contains (struct newtm_interval *list, size_t begin, size_t end)
{
  while (list && list->end < end)
    list = list->next;
  return list && list->begin <= begin;
}

/* Union [BEGIN,END) using one preallocated node; merge adjacent/overlapping
   intervals, freeing replaced nodes.  No allocation or recoverable failure.
   Valid and modified lists own separate nodes, never shared. */
static void
newtm_interval_insert (struct newtm_interval **list, struct newtm_interval *node,
                       size_t begin, size_t end)
{
  while (*list && (*list)->end < begin)
    list = &(*list)->next;
  while (*list && (*list)->begin <= end)
    {
      struct newtm_interval *old = *list;
      if (old->begin < begin) begin = old->begin;
      if (old->end > end) end = old->end;
      *list = old->next;
      free (old);
    }
  node->begin = begin;
  node->end = end;
  node->next = *list;
  *list = node;
}

/* Under a retained reservation, copy only missing parts of the requested
   range.  Existing valid bytes include previous writes and must be skipped.
   Never read outside [BEGIN,END), including other fields or object padding. */
static void
newtm_fill_missing (struct newtm_record *record, size_t begin, size_t end)
{
  struct newtm_aggregate_value *value = &record->value.aggregate;
  const unsigned char *original = record->object;
  for (struct newtm_interval *p = value->valid; p && begin < end; p = p->next)
    {
      if (p->end <= begin) continue;
      if (p->begin >= end) break;
      if (p->begin > begin)
        memcpy (value->bytes + begin, original + begin, p->begin - begin);
      if (p->end > begin) begin = p->end;
    }
  if (begin < end)
    memcpy (value->bytes + begin, original + begin, end - begin);
}

/* Validate and reserve before touching data.  Lazily allocate a scalar value
   or full-extent aggregate buffer without initializing unrelated bytes.  For
   aggregates prepare an interval node, fill only requested gaps, then mark the
   range valid.  Fill destination only after all fallible preparation. */
enum newtm_access_status
newtm_read (struct newtm_context *ctx, const void *object, size_t offset,
	    void *destination, size_t size)
{
  printf ("[newtm] newtm_read: ctx=%p, object=%p, offset=%zu, "
	  "destination=%p, size=%zu\n",
	  (void *) ctx, (void *) object, offset, destination, size);
  fflush (stdout);
  struct newtm_record *record;
  enum newtm_access_status status
    = newtm_prepare_access (ctx, object, offset, destination, size,
                            NEWTM_RESERVATION_READ, &record);
  if (status != NEWTM_ACCESS_OK)
    return status;
  if (record->kind == NEWTM_OBJECT_SCALAR)
    {
      struct newtm_scalar_value *value = &record->value.scalar;
      if (!value->bytes) value->bytes = gomp_malloc (record->size);
      if (!value->valid)
        {
          memcpy (value->bytes, object, size);
          value->valid = true;
        }
      memcpy (destination, value->bytes, size);
    }
  else
    {
      struct newtm_aggregate_value *value = &record->value.aggregate;
      size_t end = offset + size;
      if (!newtm_interval_contains (value->valid, offset, end))
        {
          struct newtm_interval *node = gomp_malloc (sizeof (*node));
          if (!value->bytes) value->bytes = gomp_malloc (record->size);
          newtm_fill_missing (record, offset, end);
          newtm_interval_insert (&value->valid, node, offset, end);
        }
      memcpy (destination, value->bytes + offset, size);
    }
  return NEWTM_ACCESS_OK;
}

/* Validate and reserve exclusively; allocate buffer and both interval nodes
   before changing any private bytes.  Write only SOURCE's requested range,
   never read original data.  Mark modified as well as valid, coalescing each
   list independently.  Prior private writes outside the range are preserved;
   publication belongs exclusively to the outer normal exit. */
enum newtm_access_status
newtm_write (struct newtm_context *ctx, const void *object, size_t offset,
	     const void *source, size_t size)
{
  printf ("[newtm] newtm_write: ctx=%p, object=%p, offset=%zu, "
	  "source=%p, size=%zu\n",
	  (void *) ctx, (void *) object, offset, (void *) source, size);
  fflush (stdout);
  struct newtm_record *record;
  enum newtm_access_status status
    = newtm_prepare_access (ctx, object, offset, source, size,
                            NEWTM_RESERVATION_WRITE, &record);
  if (status != NEWTM_ACCESS_OK)
    return status;
  if (record->kind == NEWTM_OBJECT_SCALAR)
    {
      struct newtm_scalar_value *value = &record->value.scalar;
      if (!value->bytes) value->bytes = gomp_malloc (record->size);
      memcpy (value->bytes, source, size);
      value->valid = value->modified = true;
    }
  else
    {
      struct newtm_aggregate_value *value = &record->value.aggregate;
      size_t end = offset + size;
      struct newtm_interval *valid = NULL, *modified = NULL;
      if (!newtm_interval_contains (value->valid, offset, end))
        valid = gomp_malloc (sizeof (*valid));
      if (!newtm_interval_contains (value->modified, offset, end))
        modified = gomp_malloc (sizeof (*modified));
      if (!value->bytes) value->bytes = gomp_malloc (record->size);
      memcpy (value->bytes + offset, source, size);
      if (valid) newtm_interval_insert (&value->valid, valid, offset, end);
      if (modified)
        newtm_interval_insert (&value->modified, modified, offset, end);
    }
  return NEWTM_ACCESS_OK;
}
