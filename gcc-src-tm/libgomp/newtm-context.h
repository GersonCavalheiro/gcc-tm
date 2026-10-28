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
  NEWTM_REASON_INVALID_OPERATION
};

struct newtm_interval
{
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
  unsigned char *bytes;
  struct newtm_interval *valid, *modified;
};

struct newtm_record
{
  struct newtm_record *next;
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

/* Called only by final hash release, outside the bucket, after unlinking.
   Require no readers/writer.  The owner's lease covers the entire callback. */
extern void newtm_policy_destroy (void *)
  __attribute__ ((visibility ("hidden")));

#endif
