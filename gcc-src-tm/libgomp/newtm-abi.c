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

/* Compiler-facing STM v1 adapters and terminal controller diagnostic. */
#include "libgomp.h"
#include "newtm-context.h"

/* Executable compiler boundary. Translate domains explicitly: the C enums are
   private implementation details, never ABI-sized status values. */
static uint32_t
newtm_abi_lifecycle (enum newtm_lifecycle_status status)
{
  switch (status)
    {
    case NEWTM_LIFECYCLE_OK: return NEWTM_ABI_OK;
    case NEWTM_LIFECYCLE_CONFLICT: return NEWTM_ABI_CONFLICT;
    default: return NEWTM_ABI_INVALID;
    }
}

static uint32_t
newtm_abi_access (enum newtm_access_status status)
{
  switch (status)
    {
    case NEWTM_ACCESS_OK: return NEWTM_ABI_OK;
    case NEWTM_ACCESS_CONFLICT: return NEWTM_ABI_CONFLICT;
    default: return NEWTM_ABI_INVALID;
    }
}

uint32_t
GOMP_transaction_stm_v1_begin (newtm_slot_v1 *slot, uint32_t *outermost)
{
  if (outermost)
    *outermost = 0;
  if (!slot || !outermost)
    {
      /* Preserve the internal rule: a detectable invalid invocation condemns
         an existing owned context, but never acquires an outer lease. */
      bool ignored;
      return newtm_abi_lifecycle (newtm_begin (NULL, slot,
                                              outermost ? &ignored : NULL));
    }
  bool outer = false;
  if (*slot)
    {
      uint32_t status = newtm_abi_lifecycle (newtm_begin (NULL, slot, &outer));
      if (status == NEWTM_ABI_OK)
        *outermost = outer;
      return status;
    }
  struct newtm_hash_owner_lease lease = { 0 };
  if (newtm_hash_owner_acquire (&lease) != NEWTM_HASH_OK)
    return NEWTM_ABI_INVALID;
  uint32_t status = newtm_abi_lifecycle (newtm_begin (&lease, slot, &outer));
  if (status != NEWTM_ABI_OK)
    {
      if (newtm_hash_owner_release (&lease) != NEWTM_HASH_OK)
        gomp_fatal ("transaction ABI entry cleanup failed");
    }
  else
    *outermost = outer;
  return status;
}

uint32_t
GOMP_transaction_stm_v1_register (struct newtm_context *ctx, const void *object,
                                 size_t extent, uint32_t kind)
{
  enum newtm_object_kind internal_kind;
  switch (kind)
    {
    case NEWTM_ABI_SCALAR: internal_kind = NEWTM_OBJECT_SCALAR; break;
    case NEWTM_ABI_AGGREGATE: internal_kind = NEWTM_OBJECT_AGGREGATE; break;
    default:
      return newtm_abi_lifecycle (newtm_fail (ctx, NEWTM_ACCESS_INVALID));
    }
  return newtm_abi_lifecycle (newtm_register (ctx, object, extent, internal_kind));
}

uint32_t
GOMP_transaction_stm_v1_read (struct newtm_context *ctx, const void *object,
                             size_t offset, void *destination, size_t size)
{
  return newtm_abi_access (newtm_read (ctx, object, offset, destination, size));
}

uint32_t
GOMP_transaction_stm_v1_write (struct newtm_context *ctx, const void *object,
                              size_t offset, const void *source, size_t size)
{
  return newtm_abi_access (newtm_write (ctx, object, offset, source, size));
}

/* Borrowed contexts cannot expose/copy the canonical owning slot. Validate the
   execution before following it; pointer lifetime is still a caller contract.
   Internal begin/end repeat all lifecycle/state/lease validation. */
static bool
newtm_abi_borrowed (struct newtm_context *ctx)
{
  return ctx && ctx->owner == gomp_thread () && ctx->owning_slot
    && *ctx->owning_slot == ctx;
}

uint32_t
GOMP_transaction_stm_v1_status (struct newtm_context *ctx)
{
  if (!newtm_abi_borrowed (ctx) || !ctx->lease.active || !ctx->lease.table
      || !ctx->depth)
    return NEWTM_ABI_INVALID;
  switch (ctx->state)
    {
    case NEWTM_ATTEMPT_ACTIVE: return NEWTM_ABI_OK;
    case NEWTM_ATTEMPT_CONFLICTED: return NEWTM_ABI_CONFLICT;
    default: return NEWTM_ABI_INVALID;
    }
}

uint32_t
GOMP_transaction_stm_v1_nest_begin (struct newtm_context *ctx)
{
  if (!newtm_abi_borrowed (ctx))
    return NEWTM_ABI_INVALID;
  bool outer = false;
  return newtm_abi_lifecycle (newtm_begin (NULL, ctx->owning_slot, &outer));
}

uint32_t
GOMP_transaction_stm_v1_nest_end (struct newtm_context *ctx)
{
  if (!newtm_abi_borrowed (ctx))
    return NEWTM_ABI_INVALID;
  /* A borrowed frame can exit only a nested region, never confirm/consume the
     outer level. Permanent misuse is retained for the external epilogue. */
  if (ctx->depth <= 1)
    return newtm_abi_lifecycle (newtm_fail (ctx, NEWTM_ACCESS_INVALID));
  return newtm_abi_lifecycle (newtm_end (ctx->owning_slot));
}

uint32_t
GOMP_transaction_stm_v1_end (newtm_slot_v1 *slot)
{
  return newtm_abi_lifecycle (newtm_end (slot));
}

uint32_t
GOMP_transaction_stm_v1_fail (struct newtm_context *ctx, uint32_t cause)
{
  enum newtm_access_status internal_cause = cause == NEWTM_ABI_CONFLICT
    ? NEWTM_ACCESS_CONFLICT : NEWTM_ACCESS_INVALID;
  return newtm_abi_lifecycle (newtm_fail (ctx, internal_cause));
}

uint32_t
GOMP_transaction_stm_v1_discard (newtm_slot_v1 *slot)
{
  return newtm_abi_lifecycle (newtm_discard (slot));
}

uint32_t
GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *slot, size_t conflicts)
{
  return newtm_abi_lifecycle (newtm_after_failure (slot, conflicts));
}

void
GOMP_transaction_stm_v1_fatal (uint32_t status)
{
  gomp_fatal ("transaction STM controller failed without context (status %u)",
              (unsigned) status);
}
