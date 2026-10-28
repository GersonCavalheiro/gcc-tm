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

/* Internal hash interface for the experimental transaction runtime.
   Table lifetime, acquire/lookup/release, immutable queries and metadata
   visits and consistent saturated counting are operational.
   All calls retain debug printf/fflush outside bucket locks.
   The compiler-facing bridge retains real entries; it does not implement STM.  */

#ifndef LIBGOMP_NEWTM_HASH_H
#define LIBGOMP_NEWTM_HASH_H

#include <stddef.h>
#include <stdbool.h>

#define NEWTM_HASH_DEFAULT_BUCKETS 1024

struct newtm_hash_table;
struct newtm_hash_entry;

enum newtm_hash_status
{
  NEWTM_HASH_OK,
  NEWTM_HASH_INVALID,
  NEWTM_HASH_NOT_FOUND,
  NEWTM_HASH_SIZE_MISMATCH,
  NEWTM_HASH_BUSY,
  NEWTM_HASH_OVERFLOW
};

/* Called outside the bucket lock when the last reference is released.
   Owns only the retired generation's POLICY_DATA; must return normally,
   without exceptions/longjmp, table reentry or access to the retired entry,
   program object or another generation. TABLE must outlive this callback.  */
typedef void (*newtm_hash_policy_destroy_fn) (void *policy_data);

/* Called under the bucket lock.  May update *POLICY_DATA; ARGUMENT carries
   caller input/output. Subject to the full entry_visit contract below:
   no reentry, transaction waits, inverted locks, exceptions or escaping slot.
   Must not execute user code, consume the reference or release the mutex.  */
typedef void (*newtm_hash_visit_fn) (const void *address, size_t size,
                                   void **policy_data, void *argument);

/* Proposed address-to-bucket algorithm, shared by acquire and lookup:
   Convert ADDRESS to uintptr_t without discarding low alignment bits. Select
   the MurmurHash3 finalizer by UINTPTR_MAX, not sizeof (unsigned long):
   - UINTPTR_MAX == UINT32_MAX: unsigned 32-bit x; x ^= x >> 16;
     x *= 0x85ebca6b; x ^= x >> 13; x *= 0xc2b2ae35; x ^= x >> 16.
   - UINTPTR_MAX == UINT64_MAX: unsigned 64-bit x; x ^= x >> 33;
     x *= 0xff51afd7ed558ccd; x ^= x >> 33;
     x *= 0xc4ceb9fe1a85ec53; x ^= x >> 33.
   Products wrap at the selected unsigned width; use appropriately typed
   constants and shifts strictly smaller than that width. Finalizers from
   https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp .
   Compute x % bucket_count without narrowing x before the reduction. Any
   positive capacity whose allocation is representable is admissible; do not
   substitute a bit mask or require a power of two. Equality still compares
   exact addresses, not mixed values. Store the resulting immutable index in
   new entries for release/visit. No seed, mutable state or object reads.
   Widths other than 32/64 have no selected implementation: the operational
   implementation must diagnose unsupported widths at build time, rather
   than silently truncate; wider-width support requires an explicit extension.
   The mapping is implemented for acquisition/lookup; it does not read objects.  */

/* Logging constraint for the proposed operational algorithms:
   Never call printf/fflush or other diagnostic I/O while holding any bucket
   mutex, including from visit callbacks. The same restriction applies while
   count holds multiple buckets. Keep the printf/fflush debug messages.
   Lazy bridge initialization must also release its global mutex before any
   creation log or fatal diagnostic. Raw allocation under a runtime lock must
   report failure to its caller without I/O; release the lock before gomp_fatal.
   Capture only the required data while protected and emit before locking
   or after unlocking; never dereference a released entry for logging, expose mutable
   metadata without protection or infer concurrent operation order from print
   order. If tracing at the bridge, likewise hold no bucket mutex during I/O.
   Update output-dependent tests and documentation in the same change as any
   message change. Verify actual operations, reference lifetime and callbacks;
   message counts alone are not proof of correct hash behavior.  */

/* Common functional pre/postconditions:
   TABLE/ENTRY arguments must denote live matching objects; destruction must
   not overlap any other operation, including immutable-field queries.
   Retained references keep entries alive, not the registered program objects.
   OUT, when nonnull, must designate writable exclusive storage, not overlapping
   table, entries or metadata. Clearing OUT does not release its old contents;
   the caller must preserve any previously owned reference separately.
   Returnable failures cause no partial insertion or reference increment by
   this call; other threads may still change the table. No normal return leaves
   a bucket mutex held. NULL checks do not validate dangling pointers.
   These are hash lifetime/storage contracts, not transactional reservations
   or synchronization of the registered objects' values.  */

/* Create an empty table with BUCKET_COUNT buckets and one mutex per bucket.
   DESTROY_POLICY owns nonnull entry metadata; it may be null only when all
   metadata remain null.  Store the new table in *OUT.  Return OK, INVALID
   for zero capacity/null OUT, or OVERFLOW for an unrepresentable allocation.
   On failure clear *OUT when OUT is valid. Allocation failure is fatal.
   Allocation uses a shared nonlogging malloc/calloc helper; the bridge calls
   that helper while initializing and emits the existing creation log only
   after publishing the complete table and releasing its global mutex.
   The public wrapper logs without acquiring that mutex. No recoverable OOM
   status is introduced; fatal diagnostics occur outside runtime locks.  */
extern enum newtm_hash_status newtm_hash_create (size_t bucket_count,
                   newtm_hash_policy_destroy_fn destroy_policy,
                   struct newtm_hash_table **out)
  __attribute__ ((visibility ("hidden")));

/* With externally established quiescence, destroy TABLE and its mutexes
   if empty. The caller must prevent new operations and wait for all active
   operations, visits and policy destructors to finish, keeping this exclusion
   through the return. Idle retained references are allowed, but their entries
   cause BUSY; they must not be used concurrently with this call. A zero count
   neither establishes quiescence nor proves that destructors have finished.
   Return INVALID for null TABLE, BUSY if any entry remains, or OK on success.
   BUSY must leave the complete table intact; success invalidates TABLE.  */
extern enum newtm_hash_status newtm_hash_destroy (struct newtm_hash_table *table)
  __attribute__ ((visibility ("hidden")));

/* Find or insert ADDRESS with SIZE bytes in TABLE under its bucket mutex.
   Return a retained entry in *OUT, creating it with null metadata if absent.
   Return OK, INVALID for null arguments/zero SIZE, SIZE_MISMATCH for an
   existing key with another size, or OVERFLOW for counters/allocation size.
   Clear *OUT on failure when valid; make no partial changes.  Allocation
   failure is fatal. Use the shared uintptr_t/fmix32/fmix64 bucket algorithm
   specified above. Do not read or reserve the program object.
   Initial implementation allocates a missing entry with malloc under the
   bucket mutex. On allocation failure, unlock before gomp_fatal, keeping OUT
   null and the table unchanged; OOM remains fatal, not a returned status.
   Once operational correctness is established, measured allocator contention
   may motivate allocation outside the lock. Such an optimization must recheck
   the key under lock, dispose of unused candidates and preserve uniqueness,
   size checks, overflow handling, reference ownership and failure atomicity.
   It is a future option, not a requirement of the initial implementation.  */
extern enum newtm_hash_status newtm_hash_acquire (struct newtm_hash_table *table, const void *address,
                    size_t size, struct newtm_hash_entry **out)
  __attribute__ ((visibility ("hidden")));

/* Look up ADDRESS without insertion and retain the entry under its bucket
   mutex.  Return OK, NOT_FOUND, INVALID for null arguments, or OVERFLOW for
   the reference counter.  Clear *OUT on failure when valid.  The caller must
   release a successful reference; it is not a transactional reservation.
   Use the same uintptr_t/fmix32/fmix64 reduction as acquire, without truncation.  */
extern enum newtm_hash_status newtm_hash_lookup_acquire (struct newtm_hash_table *table,
                           const void *address,
                           struct newtm_hash_entry **out)
  __attribute__ ((visibility ("hidden")));

/* Release one caller-owned reference to ENTRY in TABLE under its bucket lock.
   Require a live matching TABLE/ENTRY, a positive reference count and one
   caller-owned reference obtained by a successful acquire/lookup_acquire.
   Each successful acquisition requires one release; copying a pointer does
   not acquire a reference, and a failed acquisition requires no release.
   Withdraw only the participations/reservations sustained by the consumed
   reference before this call; other independently retained users may remain.
   Keep TABLE alive until this call fully returns, including the destructor
   and entry deallocation after unlock. Empty buckets do not prove quiescence.
   Decrement under the bucket mutex. At zero, unlink, unlock, destroy nonnull
   metadata exactly once and free the entry before returning. No participation
   or reservation may outlive the last reference sustaining it.
   A positive decrement only means this call does not destroy ENTRY; another
   thread may release the last reference and free it before this call returns.
   After release, access requires another independently retained reference.
   A new generation of the same address may exist during old metadata cleanup;
   its destructor must touch neither the program object nor the new generation.
   The callback must return normally, without reentry, exceptions or longjmp.
   Return no state and no held bucket lock. NULL, mismatched table, zero count,
   double release and dangling pointers are contract violations with no
   guaranteed diagnosis or recoverable result; never force removal while used.  */
extern void newtm_hash_release (struct newtm_hash_table *table,
                    struct newtm_hash_entry *entry)
  __attribute__ ((visibility ("hidden")));

/* Return the immutable canonical program-object address stored in ENTRY.
   Require a valid retained reference; no lock or object dereference is needed.
   The returned address does not extend the lifetime of the program object.  */
extern const void * newtm_hash_entry_address (const struct newtm_hash_entry *entry)
  __attribute__ ((visibility ("hidden")));

/* Return ENTRY's immutable object size in bytes.  Require a valid retained
   reference; do not read the object or acquire a transactional reservation.  */
extern size_t newtm_hash_entry_size (const struct newtm_hash_entry *entry)
  __attribute__ ((visibility ("hidden")));

/* Call nonnull VISIT on the address, size and metadata slot of retained ENTRY
   under its bucket mutex in TABLE, forwarding ARGUMENT (which may be null).
   Require valid matching TABLE/ENTRY.  Return no value; results travel through
   ARGUMENT. The callback must return normally without consuming the retained
   reference, unlocking, reentering this table, waiting for a transaction,
   executing user code or acquiring locks with an inverted order. It must not
   retain the metadata slot address. Metadata ownership belongs to the entry;
   replacements must dispose of or transfer the old metadata correctly.
   Nonnull metadata require a configured policy destructor. Pointers copied
   out of the callback are not protected after unlock; any later use needs an
   explicit lifetime and synchronization protocol. Return with the reference
   preserved and mutex released; callback changes are not rolled back.  */
extern void newtm_hash_entry_visit (struct newtm_hash_table *table,
                        struct newtm_hash_entry *entry,
                        newtm_hash_visit_fn visit, void *argument)
  __attribute__ ((visibility ("hidden")));

/* Return a consistent entry count, saturated at SIZE_MAX, for valid TABLE.
   Lock every bucket in ascending order, sum counts, unlock in reverse order.
   For diagnostics only; require no concurrent destruction and no bucket
   lock already held. Never call from a visit or destructor of this table.
   References and transactional participants are not counted. The snapshot may
   change after unlock; zero does not exclude new operations, retained users
   of the table or an unlinked entry whose policy destructor is still running.
   It is not authorization to destroy TABLE; preserve all state/references.  */
extern size_t newtm_hash_count (struct newtm_hash_table *table)
  __attribute__ ((visibility ("hidden")));

/* Internal owner admission, distinct from the nine table primitives.
   Initialize a lease to zero. A successful acquire admits one user and lends
   the owner's global table until release; retain the lease across ALL table
   operations, entry references, metadata destructors and cleanup. Copying a
   lease does not acquire another participation. Access/transfer requires the
   caller's own synchronization; do not release while another thread uses it.
   Independently created tables still use their caller's external quiescence.
   These interfaces are hidden and introduce no compiler-facing GOMP ABI. */
struct newtm_hash_owner_lease
{
  struct newtm_hash_table *table;
  bool active;
};

/* OK admits while OPEN; BUSY rejects CLOSING; INVALID rejects CLOSED, a null
   lease or an already active lease; OVERFLOW leaves admission unchanged.
   An inactive valid lease has a null table on failure. Allocation remains
   fatal outside the owner lock. Initialization logs are covered by the lease.
   The global table has immutable newtm_policy_destroy configured at creation;
   context cleanup must withdraw all reservations before releasing references. */
extern enum newtm_hash_status
newtm_hash_owner_acquire (struct newtm_hash_owner_lease *lease)
  __attribute__ ((visibility ("hidden")));

/* Finish all use/references first. OK consumes one participation and clears
   the lease; INVALID preserves it for null/inactive/mismatched leases.
   No table access is allowed after release. Does not itself destroy tables. */
extern enum newtm_hash_status
newtm_hash_owner_release (struct newtm_hash_owner_lease *lease)
  __attribute__ ((visibility ("hidden")));

/* Nonblocking OPEN -> CLOSING -> CLOSED, irreversible. Reject new users but
   allow admitted users/nested regions to finish. BUSY means active leases,
   another destruction coordinator, or residual entries; preserve table and
   remain CLOSING, never force removal or reopen. Retry after users finish/join.
   OK closes an unused owner or destroys its drained table; repeated CLOSED
   calls return OK without destroying twice. No waits, bucket locks or I/O
   under the owner mutex. Static admission control outlives the freed table.
   Does not make dlclose safe: callers must exclude future library execution. */
extern enum newtm_hash_status newtm_hash_owner_close (void)
  __attribute__ ((visibility ("hidden")));

#endif /* LIBGOMP_NEWTM_HASH_H */
