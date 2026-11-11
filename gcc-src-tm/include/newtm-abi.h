/* Experimental compiler/libgomp STM ABI v1.  Same-target ABI only.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_ABI_H
#define GCC_NEWTM_ABI_H

#define NEWTM_ABI_OK 0
#define NEWTM_ABI_CONFLICT 1
#define NEWTM_ABI_INVALID 2
#define NEWTM_ABI_SCALAR 0
#define NEWTM_ABI_AGGREGATE 1
#define NEWTM_STORAGE_REGISTERED 0
#define NEWTM_STORAGE_PRIVATE 1

/* One source of truth for field order; use native target alignment/padding.
   Bindings borrow logical identity, never references or private storage.
   Prepared clones wire incoming/result channels through the clone transport. */
#define NEWTM_ABI_BINDING_FIELDS(F) \
  F (PTR, object)                  \
  F (SIZE, object_size)            \
  F (SIZE, offset)                 \
  F (U32, kind)

#ifndef NEWTM_ABI_COMPILER
#include <stddef.h>
#include <stdint.h>
struct newtm_context;
typedef uint32_t newtm_status_v1;
typedef struct newtm_context *newtm_slot_v1;
struct newtm_binding_v1
{
#define NEWTM_ABI_PTR void *
#define NEWTM_ABI_SIZE size_t
#define NEWTM_ABI_U32 uint32_t
#define NEWTM_ABI_FIELD(T, N) NEWTM_ABI_##T N;
  NEWTM_ABI_BINDING_FIELDS (NEWTM_ABI_FIELD)
#undef NEWTM_ABI_FIELD
#undef NEWTM_ABI_PTR
#undef NEWTM_ABI_SIZE
#undef NEWTM_ABI_U32
};
/* Clone-only transport. The identity ABI above is unchanged. Storage is a
   compiler-proved classification, not a fallback after failed registration.
   PRIVATE denotes an object owned by this attempt, including caller locals;
   addresses of a callee's own locals must never escape that callee. */
struct newtm_clone_binding_v1
{
  struct newtm_binding_v1 identity;
  uint32_t storage;
};
#ifdef __cplusplus
extern "C" {
#endif
/* SLOT is exclusive, initially null, stable until consumed; never copy an
   owning slot. CTX is borrowed. OUTERMOST is uint32_t (0/1), initialized to 0
   even on returning failure. It must not alias SLOT or any runtime storage.
   Begin acquires/transfers a lease only on outer entry. No implicit register.
   Buffers, objects and slots must be live and satisfy the internal contracts. */
newtm_status_v1 GOMP_transaction_stm_v1_begin (newtm_slot_v1 *, uint32_t *);
newtm_status_v1 GOMP_transaction_stm_v1_register
  (struct newtm_context *, const void *, size_t, uint32_t);
newtm_status_v1 GOMP_transaction_stm_v1_read
  (struct newtm_context *, const void *, size_t, void *, size_t);
newtm_status_v1 GOMP_transaction_stm_v1_write
  (struct newtm_context *, const void *, size_t, const void *, size_t);
/* End only on normal success; outer OK publishes and consumes SLOT, inner OK
   decrements depth. Errors preserve resources for the external epilogue.
   Fail condemns without cleanup; OK/unknown cause is INVALID.
   Discard explicitly consumes an unpublished slot without publication.
   After_failure consumes all levels, returns CONFLICT after backoff only;
   INVALID is fatal AFTER cleanup. Count is positive, saturated by caller.
   Failed begin with no context requires no after_failure. */
/* Prepared clones borrow CTX only. Nested operations use its canonical slot
   internally; nest_end must never consume/commit the outer attempt. A failed
   nested entry/exit propagates to the outer controller without local cleanup. */
/* Observe effective status without changing nesting, reservations or values.
   Borrowed live CTX owned by this execution is required; unknown states are
   INVALID. Useful at clone entry and before publishing caller-private results. */
newtm_status_v1 GOMP_transaction_stm_v1_status (struct newtm_context *);
newtm_status_v1 GOMP_transaction_stm_v1_nest_begin (struct newtm_context *);
newtm_status_v1 GOMP_transaction_stm_v1_nest_end (struct newtm_context *);
newtm_status_v1 GOMP_transaction_stm_v1_end (newtm_slot_v1 *);
newtm_status_v1 GOMP_transaction_stm_v1_fail (struct newtm_context *, uint32_t);
newtm_status_v1 GOMP_transaction_stm_v1_discard (newtm_slot_v1 *);
newtm_status_v1 GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *, size_t);
/* Terminal controller error with no live owned context. Existing contexts
   must first go through after_failure; this service performs no cleanup. */
void GOMP_transaction_stm_v1_fatal (uint32_t) __attribute__ ((__noreturn__));
#ifdef __cplusplus
}
#endif
#endif
#endif
