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

<<<<<<< HEAD
/* Internal access and lifecycle interfaces for the experimental runtime.
   Trace-only skeletons: print parameters and return INVALID.  Only begin's
   nonnull OUTERMOST output is initialized to false; context slots, contexts
   and buffers are preserved.  Functional contracts describe future work.  */
=======
/* Internal access interface for the experimental transaction runtime.
   Trace-only skeleton: both operations print parameters and return INVALID.
   No argument is dereferenced and no buffer or context is modified.  The
   functional contracts below describe future work, not current behavior.  */
>>>>>>> 775307665b55aaf0c7f28e25bcb02f20d9019458

#ifndef LIBGOMP_NEWTM_TRANSACTION_H
#define LIBGOMP_NEWTM_TRANSACTION_H

#include <stddef.h>
<<<<<<< HEAD
#include <stdbool.h>

struct newtm_context;
struct newtm_hash_table;

/* Attempt states, not operation results.  This declares the future state
   machine; the context is still opaque and stubs perform no transitions.
   See OPENMP_TRANSACTION_ABORT.md for the transition/ownership contract.
   INVALID is absorbing until discard; CONFLICTED may become INVALID.
   PUBLISHING is irrevocable, entered only after all fallible preparation.
   CLOSED is terminal: detach the owning slot before releasing resources.
   Only the consuming end/discard retains a private pointer during teardown;
   CLOSED does not itself mean committed, freed or safe to retry.  */
enum newtm_attempt_state
{
  NEWTM_ATTEMPT_ACTIVE,
  NEWTM_ATTEMPT_CONFLICTED,
  NEWTM_ATTEMPT_INVALID,
  NEWTM_ATTEMPT_PUBLISHING,
  NEWTM_ATTEMPT_CLOSED
};
=======

struct newtm_context;
>>>>>>> 775307665b55aaf0c7f28e25bcb02f20d9019458

enum newtm_access_status
{
  NEWTM_ACCESS_OK,
  NEWTM_ACCESS_CONFLICT,
  NEWTM_ACCESS_INVALID
};

<<<<<<< HEAD
/* Operation results, not context states.  Keep separate from access/hash
   results; translate explicitly.  INVALID in current stubs means unavailable,
   not validated arguments.  No stub ever reports success or retry.  */
enum newtm_lifecycle_status
{
  NEWTM_LIFECYCLE_OK,
  NEWTM_LIFECYCLE_CONFLICT,
  NEWTM_LIFECYCLE_INVALID
};

/* All future lifecycle operations require the owning execution, without task
   scheduling throughout the attempt.  No mutex escapes a normal return.
   Each attempt has one canonical owning slot, never a copied owning handle.
   Borrowed context pointers cannot consume it or outlive teardown.  Calls are
   serialized by the owner; no lifecycle reentry during publication/teardown.
   Context slots are caller-owned, exclusive and valid for the call; OUTERMOST
   must not overlap slots, contexts, table storage or program objects.  Table
   lifetime extends past all its contexts.  Allocation failure is fatal.
   Current stubs do not inspect slots or pointees, allocate, free, lock, publish,
   signal failure or wait.  A nonnull OUTERMOST must still be writable.  */

/* Future entry: TABLE is borrowed, SLOT is an in/out owning context handle.
   Require nonnull TABLE/SLOT/OUTERMOST.  A null *SLOT creates an outer attempt
   at depth one; an active *SLOT for the same table enters a nested level.
   On OK, set OUTERMOST accordingly.  Do not register or read objects here.
   On failure, preserve *SLOT and depth, set OUTERMOST false; CONFLICT means
   an attempt condemned by conflict, INVALID a permanent detected violation
   or an already permanently condemned attempt.  INVALID takes precedence.
   No partially initialized level is published.  Caller checks status before
   using OUTERMOST.  Stub: print, set nonnull OUTERMOST false, return INVALID.  */
extern enum newtm_lifecycle_status newtm_begin
  (struct newtm_hash_table *table, struct newtm_context **slot, bool *outermost)
  __attribute__ ((visibility ("hidden")));

/* Future registration: active owned CTX, nonnull canonical OBJECT, positive
   SIZE in bytes and supported object lifetime through the outer exit.
   Context retains one hash reference per distinct object; identical duplicates
   are idempotent, incompatible size is INVALID.  No reservation or value copy.
   Failure adds no partial record and preserves depth; a detected permanent
   error condemns valid CTX, an already conflicted attempt returns CONFLICT.
   Stub: print all arguments, return INVALID, modify nothing.  */
extern enum newtm_lifecycle_status newtm_register
  (struct newtm_context *ctx, const void *object, size_t size)
  __attribute__ ((visibility ("hidden")));

/* Future normal exit: require matching active entry in owned *SLOT.
   Inner OK only decrements depth.  Outer OK prepares resources, publishes all
   writes while retaining every reservation.  Enter PUBLISHING only after all
   fallible preparation; no return to ACTIVE/CONFLICTED/INVALID from there.
   After publishing everything, mark CLOSED and clear *SLOT before withdrawing
   reservations and releasing references/buffers/context.  Return OK only after
   complete teardown.  A null slot is not proof of successful commit.  Publication is
   a phase of this operation, not a separate commit API.  No recoverable
   failure may occur after the first publication.
   On CONFLICT/INVALID before publication, preserve *SLOT, depth and resources
   for caller-directed cleanups and discard; never discard implicitly on error.
   Condemn a valid owned context with the effective cause, retaining INVALID
   over CONFLICT.  A subsequent normal exit cannot confirm that attempt.
   Invalid context storage must not be touched.  No commit from a finally.
   Stub: print SLOT address only, return INVALID, preserve the slot.  */
extern enum newtm_lifecycle_status newtm_end (struct newtm_context **slot)
  __attribute__ ((visibility ("hidden")));

/* Future shared failure helper for access/control, not control transfer.
   Require valid owned CTX before publication; CAUSE must be ACCESS_CONFLICT
   or ACCESS_INVALID.  Mark the whole attempt condemned, return its effective
   cause in the lifecycle domain; INVALID takes precedence and is never lost.
   ACCESS_OK/unknown causes are permanent misuse, not success.  Keep depth,
   references, reservations and buffers for cleanups/discard.  Do not touch an
   invalid/foreign context or jump/retry.  Stub: print, return INVALID only.  */
extern enum newtm_lifecycle_status newtm_fail
  (struct newtm_context *ctx, enum newtm_access_status cause)
  __attribute__ ((visibility ("hidden")));

/* Future explicit discard: outer control owns this call after supported
   cleanups, for an owned unpublished attempt, condemned or explicitly being
   unwound.  Save the failure cause outside the context before calling.
   After validation, mark CLOSED and clear *SLOT before the first release;
   this invocation alone owns the detached context until teardown completes.
   Drop private data, withdraw reservations, release references/buffers and
   all nested levels, then free context.  OK reports completed cleanup, not
   commit or eligibility for retry.  A second call with the same empty slot
   returns INVALID without cleanup; never inspect a freed context to detect it.
   Double discard is a violation, not idempotent success.
   Detectable invalid input returns INVALID without consuming the slot; an
   unrecoverable cleanup failure is fatal, never CONFLICT or partial success.
   No wait or retry here.  Stub: print SLOT only, preserve it, return INVALID.  */
extern enum newtm_lifecycle_status newtm_discard (struct newtm_context **slot)
  __attribute__ ((visibility ("hidden")));

/* Future retry-control helper, after complete discard for CONFLICT only.
   ATTEMPT is the positive consecutive-conflict count (first is 1), saturated
   by the caller at SIZE_MAX.  Require no reservations/mutexes or live use of
   discarded buffers.  OK follows a bounded progressive wait; INVALID for
   invalid input.  Never create a context, retry code or promise progress.
   Delay units/formula remain an implementation choice, not this parameter's
   units.  Stub: print ATTEMPT, return INVALID immediately without waiting.  */
extern enum newtm_lifecycle_status newtm_backoff (size_t attempt)
  __attribute__ ((visibility ("hidden")));

=======
>>>>>>> 775307665b55aaf0c7f28e25bcb02f20d9019458
/* Future contract: read SIZE bytes at OFFSET in registered canonical OBJECT
   into private DESTINATION using active, owned CTX.  Require positive SIZE,
   valid nonoverlapping storage and an in-bounds range.  Obtain a compatible
   reservation before reading; observe this attempt's pending writes.
   On OK fill DESTINATION completely; on failure leave it unchanged.  Never
   publish data or release the attempt here.  No bucket lock escapes a return.
   Current stub: print all parameters, return INVALID, modify nothing.  */
extern enum newtm_access_status newtm_read (struct newtm_context *ctx,
					  const void *object, size_t offset,
					  void *destination, size_t size)
  __attribute__ ((visibility ("hidden")));

/* Future contract: write SIZE bytes from private SOURCE at OFFSET in registered
   canonical OBJECT using active, owned CTX.  Require positive SIZE, valid
   nonoverlapping storage, a writable supported type and an in-bounds range.
   Obtain an exclusive reservation before changing private data.  On OK update
   only this attempt's private view; on failure do not apply this operation,
   even partially.  Preserve SOURCE and original memory in all normal returns.
   Do not perform commit or full cleanup.  No bucket lock escapes a return.
   Current stub: print all parameters, return INVALID, modify nothing.  */
extern enum newtm_access_status newtm_write (struct newtm_context *ctx,
					   const void *object, size_t offset,
					   const void *source, size_t size)
  __attribute__ ((visibility ("hidden")));

#endif /* LIBGOMP_NEWTM_TRANSACTION_H */
