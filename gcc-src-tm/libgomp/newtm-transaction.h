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

/* Internal access and lifecycle interfaces for the experimental runtime.
   Begin, fail and discard manage real contexts.  End handles nested exits only;
   registration retains object identities; read/write operate on private values
   under reservations.  Outer end publishes modified values and completes
   cleanup.  Bounded progressive backoff follows a discarded conflict.
   Diagnostics are retained outside all runtime mutexes.  */

#ifndef LIBGOMP_NEWTM_TRANSACTION_H
#define LIBGOMP_NEWTM_TRANSACTION_H

#include <stddef.h>
#include <stdbool.h>

struct newtm_context;
struct newtm_hash_table;
struct newtm_hash_owner_lease;

/* Representation class of a supported canonical object, supplied by compiler
   type analysis at registration, never inferred from SIZE or pointer alignment.
   Arrays and admitted aggregates use AGGREGATE, even with one scalar element.
   A listed pointer object uses SCALAR; this does not register its pointee.
   These tags do not validate language types or admit otherwise excluded types.
   Values outside this enum are invalid under the registration contract.
   This is an internal interface, not a new language clause or public GOMP ABI.  */
enum newtm_object_kind
{
  NEWTM_OBJECT_SCALAR,
  NEWTM_OBJECT_AGGREGATE
};

/* Attempt states, not operation results.  Context storage is private in
   newtm-context.h.  Begin/fail/discard handle unpublished attempts; outer end
   implements ACTIVE -> PUBLISHING -> CLOSED, followed by complete teardown.
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

enum newtm_access_status
{
  NEWTM_ACCESS_OK,
  NEWTM_ACCESS_CONFLICT,
  NEWTM_ACCESS_INVALID
};

/* Operation results, not context states.  Keep separate from access/hash
   results; translate explicitly.  INVALID denotes a detected permanent failure
   or invalid invocation, never permission to retry. */
enum newtm_lifecycle_status
{
  NEWTM_LIFECYCLE_OK,
  NEWTM_LIFECYCLE_CONFLICT,
  NEWTM_LIFECYCLE_INVALID
};

/* All lifecycle operations require the owning execution, without task
   scheduling throughout the attempt.  No mutex escapes a normal return.
   Each attempt has one canonical owning slot, never a copied owning handle.
   Borrowed context pointers cannot consume it or outlive teardown.  Calls are
   serialized by the owner; no lifecycle reentry during publication/teardown.
   Context slots are caller-owned, exclusive and valid for the call; OUTERMOST
   must not overlap slots, leases, contexts or program objects.  Keep the slot
   alive through all nesting and teardown.  The transferred lease protects table
   lifetime.  Allocation failure is fatal.  Prepared external failure control
   is private to newtm-context.h; no automatic compiler retry is implied. */

/* Entry algorithm:
   1. Initialize writable OUTERMOST to false, validate SLOT/OUTERMOST.
   2. For an empty SLOT require active LEASE with nonnull table; allocate and
      initialize a context completely, transfer LEASE (clear its source), then
      publish *SLOT at depth one and set OUTERMOST true.
   3. For an existing owned context require LEASE==NULL, check state and depth
      overflow, then increment depth.  Nesting neither admits nor releases users.
   The canonical SLOT must remain at its original address through teardown.
   LEASE is used only on outer entry and is preserved on every returning error.
   Allocation failure is fatal.  Invalid/foreign storage cannot be inspected
   safely; valid live storage is a caller precondition.
   On OK, set OUTERMOST accordingly.  Do not register or read objects here.
   On failure, preserve *SLOT and depth, set OUTERMOST false; CONFLICT means
   an attempt condemned by conflict, INVALID a permanent detected violation
   or an already permanently condemned attempt.  INVALID takes precedence.
   No partially initialized level is published.  Caller checks status before
   using OUTERMOST.  All diagnostics occur outside runtime mutexes.  */
extern enum newtm_lifecycle_status newtm_begin
  (struct newtm_hash_owner_lease *lease, struct newtm_context **slot,
   bool *outermost)
  __attribute__ ((visibility ("hidden")));

/* Registration: active owned CTX, nonnull canonical OBJECT, positive
   SIZE in bytes, compiler-validated KIND and supported lifetime through outer
   exit.  KIND describes the whole canonical registered object, not an accessed
   field/element.  Keep it immutable in that attempt's record; read/write obtain
   it from the record and need no extra tag.  SCALAR selects a simple full-value
   private representation; AGGREGATE selects a full-extent buffer filled lazily
   with valid/modified interval lists.  Neither tag copies or initializes data.
   Context retains one hash reference per distinct object; identical duplicates
   are idempotent, incompatible size or kind is INVALID.  Unknown KIND is also
   INVALID.  A detected INVALID in a valid context propagates for safe cleanup
   and then fatal diagnosis by outer control, never retry.  No reservation,
   policy metadata, private value buffer or value copy is created here.
   Failure adds no partial record and preserves depth; a detected permanent
   error condemns valid CTX, an already conflicted attempt returns CONFLICT.
   Check ownership/state before argument validation; an already INVALID attempt
   preserves its first permanent cause.  Hash SIZE_MISMATCH maps to incompatible
   registration; other returned hash failures map to permanent invalid operation.
   Allocate the candidate before hash acquisition; link only after success, with
   one retained reference per record.  No fallible step follows acquisition.
   The context's list spans all nested levels and is released by discard.
   Keep depth unchanged on every return.  Logs remain outside runtime locks.
   Exact keys do not detect overlaps between distinct canonical objects; kind
   is compiler supplied and checked for duplicates within this attempt only. */
extern enum newtm_lifecycle_status newtm_register
  (struct newtm_context *ctx, const void *object, size_t size,
   enum newtm_object_kind kind)
  __attribute__ ((visibility ("hidden")));

/* Normal exit: require matching active entry in owned *SLOT.
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
   Outer preparation validates every record's representation and normalized
   interval lists (modified subset of valid, independent node ownership), then
   verifies identity and reservation under each bucket.  No original values are
   read.  The retained reservations remain stable after these short visits.
   No allocation is needed for publication.  Copy modified scalars in full and
   only modified intervals of aggregates; do not touch read-only/unused objects.
   After ALL copies, consume the slot using the shared teardown with owner
   release last.  Structural teardown failures are fatal, never recoverable.
   Program objects must remain live, disjoint and writable in modified ranges;
   pointer validity and language type constraints remain caller preconditions. */
extern enum newtm_lifecycle_status newtm_end (struct newtm_context **slot)
  __attribute__ ((visibility ("hidden")));

/* Shared failure helper for access/control, not control transfer.
   Require valid owned CTX before publication; CAUSE must be ACCESS_CONFLICT
   or ACCESS_INVALID.  Mark the whole attempt condemned, return its effective
   cause in the lifecycle domain; INVALID takes precedence and is never lost.
   ACCESS_OK/unknown causes are permanent misuse, not success.  Keep depth,
   references, reservations and buffers for cleanups/discard.  Do not touch an
   invalid/foreign context or jump/retry.
   Algorithm: validate owner and state; preserve an existing INVALID, otherwise
   record INVALID for permanent/unknown causes or CONFLICT for conflict.  Return
   the effective lifecycle cause without cleanup, publication or control transfer.
   PUBLISHING/CLOSED cannot be condemned by a normal operation.  */
extern enum newtm_lifecycle_status newtm_fail
  (struct newtm_context *ctx, enum newtm_access_status cause)
  __attribute__ ((visibility ("hidden")));

/* Explicit discard: outer control owns this call after supported
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
   Algorithm: validate the canonical owned slot and unpublished state; mark
   CLOSED, detach slot and clear depth.  For each record withdraw its reservation
   under its bucket, then free private buffers/intervals and release its hash
   reference outside that bucket.  Free record/context; transfer the lease to a
   local token so owner_release is the very last cleanup operation.  No wait,
   retry, publication or user callbacks.  Structural cleanup failure is fatal.  */
extern enum newtm_lifecycle_status newtm_discard (struct newtm_context **slot)
  __attribute__ ((visibility ("hidden")));

/* Retry-control helper, after complete discard for CONFLICT only.
   ATTEMPT is the positive consecutive-conflict count (first is 1), saturated
   by the caller at SIZE_MAX.  Require no reservations/mutexes or live use of
   discarded buffers.  OK follows a bounded progressive wait; INVALID for
   invalid input.  Never create a context, retry code or promise progress.
   Wait uses cpu_relax, 64 * 2^min(ATTEMPT-1,10) iterations (cap 65536).
   These are relaxation iterations, not time units.  Clamp before shifting so
   even SIZE_MAX is bounded.  Zero returns INVALID without waiting.  No mutable
   global state, allocation, task scheduling or synchronization is introduced. */
extern enum newtm_lifecycle_status newtm_backoff (size_t attempt)
  __attribute__ ((visibility ("hidden")));

/* Read SIZE bytes at OFFSET in registered canonical OBJECT
   into private DESTINATION using active, owned CTX.  Require positive SIZE,
   valid nonoverlapping storage and an in-bounds range.  Obtain a compatible
   reservation before reading; observe this attempt's pending writes.
   On OK fill DESTINATION completely; on failure leave it unchanged.  Never
   publish data or release the attempt here.  No bucket lock escapes a return.
   Scalar accesses require OFFSET=0 and the full registered size; cache the
   value once, including pending writes.  Aggregates allocate their full extent
   lazily, without initializing unrelated bytes.  Read only requested gaps in
   the valid interval list; never reload valid bytes over previous writes.
   Merge adjacent/overlapping valid ranges.  No change to modified ranges.
   Validate bounds by subtraction and reject known aliases of the transport
   buffer with this context's original objects/private runtime storage.  Actual
   buffer extent/lifetime and aliases outside the context remain preconditions.
   Allocate before filling the destination; OOM is fatal.  All diagnostics,
   allocations and copies happen outside bucket locks.  The shared entry path
   completes validation and reservation before returning a record to the copy
   path.  Newly detected failures describe the requested object/offset/size;
   subsequent calls preserve an already condemned attempt's first diagnostic. */
extern enum newtm_access_status newtm_read (struct newtm_context *ctx,
					  const void *object, size_t offset,
					  void *destination, size_t size)
  __attribute__ ((visibility ("hidden")));

/* Write SIZE bytes from private SOURCE at OFFSET in registered
   canonical OBJECT using active, owned CTX.  Require positive SIZE, valid
   nonoverlapping storage, a writable supported type and an in-bounds range.
   Obtain an exclusive reservation before changing private data.  On OK update
   only this attempt's private view; on failure do not apply this operation,
   even partially.  Preserve SOURCE and original memory in all normal returns.
   Do not perform commit or full cleanup.  No bucket lock escapes a return.
   Scalar accesses replace the full value and set valid/modified.  Aggregate
   writes copy only the requested bytes and union the range into independent
   valid/modified lists, sorted, disjoint and with adjacent intervals merged.
   Never read original bytes, including when preparing a partial write.  Prepare
   buffer and interval nodes before applying changes; OOM is fatal.  Validate
   transport/bounds as for read.  Failure preserves earlier private writes;
   no normal return publishes values or releases the attempt's reservations.
   Language type/const/volatile/atomic checks remain compiler preconditions. */
extern enum newtm_access_status newtm_write (struct newtm_context *ctx,
					   const void *object, size_t offset,
					   const void *source, size_t size)
  __attribute__ ((visibility ("hidden")));

#endif /* LIBGOMP_NEWTM_TRANSACTION_H */
