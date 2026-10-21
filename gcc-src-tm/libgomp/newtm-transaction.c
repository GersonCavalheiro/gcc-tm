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
/* Trace-only access and lifecycle skeletons.  Only begin initializes its
   OUTERMOST output; no context, slot or program data is inspected or changed.  */
=======
/* Trace-only access skeleton.  Do not validate or dereference arguments,
   acquire reservations, copy data, or change transaction state here.  */
>>>>>>> 775307665b55aaf0c7f28e25bcb02f20d9019458

#include "libgomp.h"
#include "newtm-transaction.h"
#include <stdio.h>

<<<<<<< HEAD
/* TODO: Create/enter a context under the lifecycle contract.  Keep the in/out
   slot intact on failure: clearing it could lose an existing outer attempt.  */
enum newtm_lifecycle_status
newtm_begin (struct newtm_hash_table *table, struct newtm_context **slot,
	     bool *outermost)
{
  printf ("[newtm] newtm_begin: table=%p, slot=%p, outermost=%p\n",
	  (void *) table, (void *) slot, (void *) outermost);
  fflush (stdout);
  if (outermost)
    *outermost = false;
  return NEWTM_LIFECYCLE_INVALID;
}

/* TODO: Retain an object reference without reserving or copying its value.  */
enum newtm_lifecycle_status
newtm_register (struct newtm_context *ctx, const void *object, size_t size)
{
  printf ("[newtm] newtm_register: ctx=%p, object=%p, size=%zu\n",
	  (void *) ctx, (void *) object, size);
  fflush (stdout);
  return NEWTM_LIFECYCLE_INVALID;
}

/* TODO: Inner exit and outer publication are phases selected by context depth.
   Follow newtm_attempt_state: a failed exit retains resources for explicit
   caller-directed discard.  Never report OK without completing its phase.  */
enum newtm_lifecycle_status
newtm_end (struct newtm_context **slot)
{
  printf ("[newtm] newtm_end: slot=%p\n", (void *) slot);
  fflush (stdout);
  return NEWTM_LIFECYCLE_INVALID;
}

/* TODO: Record the effective cause without cleanup or control transfer.  */
enum newtm_lifecycle_status
newtm_fail (struct newtm_context *ctx, enum newtm_access_status cause)
{
  printf ("[newtm] newtm_fail: ctx=%p, cause=%d\n",
	  (void *) ctx, (int) cause);
  fflush (stdout);
  return NEWTM_LIFECYCLE_INVALID;
}

/* TODO: Discard all levels exactly once after supported cleanups.  Validate,
   mark CLOSED and detach the canonical slot before the first resource release;
   only this consumer may tear down the saved context.  The stub does none of
   these steps and must not claim successful cleanup.  */
enum newtm_lifecycle_status
newtm_discard (struct newtm_context **slot)
{
  printf ("[newtm] newtm_discard: slot=%p\n", (void *) slot);
  fflush (stdout);
  return NEWTM_LIFECYCLE_INVALID;
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

=======
>>>>>>> 775307665b55aaf0c7f28e25bcb02f20d9019458
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
