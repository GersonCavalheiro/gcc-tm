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

/* Context ownership and teardown are operational.  Registration, values and
   publication are not implemented; never interpret tracing as STM isolation. */

#include "libgomp.h"
#include "newtm-context.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

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

/* TODO: Retain an object reference and its compiler-supplied KIND without
   reserving or copying its value.  Validate size/kind and duplicate consistency
   under the functional contract in newtm-transaction.h.  This trace stub only
   logs KIND numerically, including unknown tags; it does not inspect CTX or
   infer type from SIZE, reserve memory, or implement the future context.  */
enum newtm_lifecycle_status
newtm_register (struct newtm_context *ctx, const void *object, size_t size,
                enum newtm_object_kind kind)
{
  printf ("[newtm] newtm_register: ctx=%p, object=%p, size=%zu, kind=%d\n",
	  (void *) ctx, (void *) object, size, (int) kind);
  fflush (stdout);
  return NEWTM_LIFECYCLE_INVALID;
}

/* Algorithm: validate canonical ownership and propagate condemned state.
   An ACTIVE inner exit reduces depth only.  Outer publication is unavailable:
   condemn it as INVALID, preserve all resources and require explicit discard.
   Never return success for a commit that has not been implemented. */
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
  if (ctx->depth > 1)
    {
      --ctx->depth;
      return NEWTM_LIFECYCLE_OK;
    }
  return newtm_fail (ctx, NEWTM_ACCESS_INVALID);
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

/* Algorithm: validate canonical owner/slot and an unpublished attempt.  Mark
   CLOSED and detach slot before cleanup.  Withdraw reservations under buckets;
   release private buffers, interval lists, hash references and records outside
   them.  Transfer the lease to a local token, free context, then release the
   owner participation LAST.  No publication, retry, user callback or implicit
   fatal for the saved INVALID cause: outer control retains that diagnosis. */
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
      free (record);
    }
  struct newtm_hash_owner_lease lease = ctx->lease;
  ctx->lease.active = false;
  ctx->lease.table = NULL;
  free (ctx);
  if (newtm_hash_owner_release (&lease) != NEWTM_HASH_OK)
    gomp_fatal ("transaction owner participation release failed");
  return NEWTM_LIFECYCLE_OK;
}

/* TODO: Bounded progressive waiting after discard; retry remains generated
   control flow.  No sleep belongs in this trace-only stage.  */
enum newtm_lifecycle_status
newtm_backoff (size_t attempt)
{
  printf ("[newtm] newtm_backoff: attempt=%zu\n", attempt);
  fflush (stdout);
  return NEWTM_LIFECYCLE_INVALID;
}

/* TODO: Implement the read contract in newtm-transaction.h and
   OPENMP_TRANSACTION_ACCESS_AGENT.md once its dependencies are available.  */
enum newtm_access_status
newtm_read (struct newtm_context *ctx, const void *object, size_t offset,
	    void *destination, size_t size)
{
  printf ("[newtm] newtm_read: ctx=%p, object=%p, offset=%zu, "
	  "destination=%p, size=%zu\n",
	  (void *) ctx, (void *) object, offset, destination, size);
  fflush (stdout);
  return NEWTM_ACCESS_INVALID;
}

/* TODO: Implement the write contract without publishing to original memory.
   A conflict must be signaled before the requested access takes place.  */
enum newtm_access_status
newtm_write (struct newtm_context *ctx, const void *object, size_t offset,
	     const void *source, size_t size)
{
  printf ("[newtm] newtm_write: ctx=%p, object=%p, offset=%zu, "
	  "source=%p, size=%zu\n",
	  (void *) ctx, (void *) object, offset, (void *) source, size);
  fflush (stdout);
  return NEWTM_ACCESS_INVALID;
}
