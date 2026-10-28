/* Context lifecycle/cleanup tests.  Historical filename; begin/fail/discard
   now operate on real storage.  Private inspection adds no exported ABI. */
#include "newtm-context.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#undef NDEBUG
#include <assert.h>

/* Install prepared metadata under the bucket; allocation is outside it. */
static void
install (const void *object, size_t size, void **data, void *arg)
{
  (void) object; (void) size;
  assert (*data == NULL);
  *data = arg;
}

/* Add a second context's reader to exercise non-head removal and lifetime. */
static void
add_reader (const void *object, size_t size, void **data, void *arg)
{
  struct newtm_object_policy *p = *data;
  struct newtm_record *r = arg;
  assert (p && !p->writer && object == r->object && size == r->size);
  r->reader_next = p->readers;
  p->readers = r;
}

static void *
foreign_discard (void *arg)
{
  assert (newtm_discard (arg) == NEWTM_LIFECYCLE_INVALID);
  return NULL;
}

static struct newtm_record *
record (struct newtm_context *ctx, void *object, size_t size,
        enum newtm_object_kind kind, enum newtm_reservation reservation)
{
  struct newtm_record *r = calloc (1, sizeof (*r));
  struct newtm_object_policy *p = calloc (1, sizeof (*p));
  assert (r && p);
  assert (newtm_hash_acquire (ctx->lease.table, object, size, &r->entry)
          == NEWTM_HASH_OK);
  r->object = object; r->size = size; r->kind = kind;
  r->context = ctx; r->reservation = reservation;
  if (reservation == NEWTM_RESERVATION_READ) p->readers = r;
  if (reservation == NEWTM_RESERVATION_WRITE) p->writer = r;
  newtm_hash_entry_visit (ctx->lease.table, r->entry, install, p);
  r->next = ctx->records; ctx->records = r;
  return r;
}

extern void GOMP_transaction_begin (void);
extern void GOMP_transaction_register (void *, size_t);

int
main (int argc, char **argv)
{
  if (argc == 2 && strcmp (argv[1], "policy-live") == 0)
    {
      struct newtm_record live = { 0 };
      struct newtm_object_policy policy = { &live, NULL };
      newtm_policy_destroy (&policy);
      abort ();
    }
  if (argc == 2 && strcmp (argv[1], "bridge-invalid") == 0)
    {
      int value = 0;
      GOMP_transaction_begin ();
      GOMP_transaction_register (&value, sizeof value);
      GOMP_transaction_register (NULL, 0);
      abort ();
    }

  struct newtm_hash_owner_lease lease = { 0 };
  struct newtm_context *ctx = NULL;
  bool outer = true;
  assert (newtm_begin (NULL, &ctx, &outer) == NEWTM_LIFECYCLE_INVALID);
  assert (!ctx && !outer);
  assert (newtm_discard (NULL) == NEWTM_LIFECYCLE_INVALID);
  assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_INVALID);
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  struct newtm_hash_table *table = lease.table;
  assert (newtm_begin (&lease, NULL, &outer) == NEWTM_LIFECYCLE_INVALID);
  assert (lease.active && lease.table == table);
  assert (newtm_begin (&lease, &ctx, NULL) == NEWTM_LIFECYCLE_INVALID);
  assert (lease.active && !ctx);
  assert (newtm_begin (&lease, &ctx, &outer) == NEWTM_LIFECYCLE_OK);
  assert (outer && ctx && !lease.active && !lease.table);
  assert (ctx->depth == 1 && ctx->state == NEWTM_ATTEMPT_ACTIVE);
  assert (ctx->records == NULL && ctx->lease.active);
  assert (newtm_begin (NULL, &ctx, &outer) == NEWTM_LIFECYCLE_OK);
  assert (!outer && ctx->depth == 2);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_OK && ctx->depth == 1);

  struct newtm_context *copy = ctx;
  assert (newtm_discard (&copy) == NEWTM_LIFECYCLE_INVALID);
  assert (ctx == copy && ctx->depth == 1);
  assert (newtm_begin (NULL, &copy, &outer) == NEWTM_LIFECYCLE_INVALID);
  pthread_t thread;
  assert (!pthread_create (&thread, NULL, foreign_discard, &ctx));
  assert (!pthread_join (thread, NULL));
  assert (ctx == copy && ctx->state == NEWTM_ATTEMPT_ACTIVE);

  /* Unimplemented operations preserve their buffers/arguments and return no
     fabricated success.  Explicit kind logging remains part of this test. */
  int scalar = 42;
  unsigned char object[32]; memset (object, 0x33, sizeof object);
  assert (newtm_register (ctx, &scalar, sizeof scalar, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_INVALID);
  assert (newtm_register (ctx, object, sizeof object, NEWTM_OBJECT_AGGREGATE)
          == NEWTM_LIFECYCLE_INVALID);
  assert (newtm_register (NULL, NULL, 0, (enum newtm_object_kind) 99)
          == NEWTM_LIFECYCLE_INVALID);
  assert (newtm_backoff (1) == NEWTM_LIFECYCLE_INVALID);
  assert (scalar == 42 && object[0] == 0x33);

  ctx->depth = SIZE_MAX;
  assert (newtm_begin (NULL, &ctx, &outer) == NEWTM_LIFECYCLE_INVALID);
  assert (ctx->depth == SIZE_MAX && ctx->state == NEWTM_ATTEMPT_INVALID);
  assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_OK && !ctx);
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, &ctx, &outer) == NEWTM_LIFECYCLE_OK);
  /* Outer commit remains unavailable, even with no records. */
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_INVALID);
  assert (ctx && ctx->state == NEWTM_ATTEMPT_INVALID);
  assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_OK && !ctx);
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, &ctx, &outer) == NEWTM_LIFECYCLE_OK);

  /* Seed valid future policy records to exercise the real cleanup of buffers,
     independent interval lists, reader and writer links, and final callbacks. */
  struct newtm_record *s = record (ctx, &scalar, sizeof scalar,
                                  NEWTM_OBJECT_SCALAR, NEWTM_RESERVATION_READ);
  s->value.scalar.bytes = malloc (sizeof scalar);
  assert (s->value.scalar.bytes);
  struct newtm_record *a = record (ctx, object, sizeof object,
                                  NEWTM_OBJECT_AGGREGATE, NEWTM_RESERVATION_WRITE);
  a->value.aggregate.bytes = malloc (sizeof object);
  a->value.aggregate.valid = calloc (1, sizeof (struct newtm_interval));
  a->value.aggregate.modified = calloc (1, sizeof (struct newtm_interval));
  assert (a->value.aggregate.bytes && a->value.aggregate.valid
          && a->value.aggregate.modified);
  memset (a->value.aggregate.bytes, 0x77, sizeof object);
  a->value.aggregate.valid->end = sizeof object;
  a->value.aggregate.modified->end = sizeof object;
  struct newtm_context *other = NULL;
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, &other, &outer) == NEWTM_LIFECYCLE_OK);
  struct newtm_record *reader = calloc (1, sizeof (*reader));
  assert (reader);
  reader->context = other; reader->object = &scalar;
  reader->size = sizeof scalar; reader->kind = NEWTM_OBJECT_SCALAR;
  reader->reservation = NEWTM_RESERVATION_READ;
  assert (newtm_hash_acquire (table, &scalar, sizeof scalar, &reader->entry)
          == NEWTM_HASH_OK);
  other->records = reader;
  newtm_hash_entry_visit (table, reader->entry, add_reader, reader);
  assert (newtm_hash_count (table) == 2);
  assert (newtm_hash_owner_close () == NEWTM_HASH_BUSY);
  /* Already admitted nesting remains legal after the close of admission. */
  assert (newtm_begin (NULL, &ctx, &outer) == NEWTM_LIFECYCLE_OK);
  assert (newtm_fail (ctx, NEWTM_ACCESS_CONFLICT) == NEWTM_LIFECYCLE_CONFLICT);
  assert (newtm_begin (NULL, &ctx, &outer) == NEWTM_LIFECYCLE_CONFLICT);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_CONFLICT);
  assert (ctx->depth == 2 && ctx->records == a);
  assert (newtm_fail (ctx, NEWTM_ACCESS_INVALID) == NEWTM_LIFECYCLE_INVALID);
  assert (newtm_fail (ctx, NEWTM_ACCESS_CONFLICT) == NEWTM_LIFECYCLE_INVALID);
  assert (ctx->failure.reason == NEWTM_REASON_INVALID_OPERATION);
  assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_OK && !ctx);
  assert (newtm_hash_count (table) == 1); /* Other reader keeps entry alive. */
  assert (newtm_hash_owner_close () == NEWTM_HASH_BUSY);
  assert (newtm_discard (&other) == NEWTM_LIFECYCLE_OK && !other);
  assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_INVALID);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_INVALID);
  assert (scalar == 42);
  for (size_t i = 0; i < sizeof object; ++i) assert (object[i] == 0x33);
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  puts ("[teste] context lifecycle OK");
  return 0;
}
