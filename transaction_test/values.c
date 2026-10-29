/* Private values: actual byte results, normalized interval unions, guarded
   fields, and unchanged outputs on failure.  No publication is implemented. */
#include "newtm-context.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#undef NDEBUG
#include <assert.h>

static void
begin (struct newtm_context **ctx, const void *object, size_t size,
       enum newtm_object_kind kind)
{
  struct newtm_hash_owner_lease lease = { 0 };
  bool outer;
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, ctx, &outer) == NEWTM_LIFECYCLE_OK && outer);
  assert (newtm_register (*ctx, object, size, kind) == NEWTM_LIFECYCLE_OK);
}

static void
discard (struct newtm_context **ctx)
{
  assert (newtm_discard (ctx) == NEWTM_LIFECYCLE_OK && !*ctx);
}

static void
check_intervals (struct newtm_interval *list, const unsigned char *expected,
                 size_t size)
{
  struct newtm_interval *previous = NULL;
  size_t cursor = 0;
  for (struct newtm_interval *p = list; p; p = p->next)
    {
      assert (p->begin < p->end && p->end <= size);
      assert (!previous || previous->end < p->begin);
      while (cursor < p->begin) assert (!expected[cursor++]);
      while (cursor < p->end) assert (expected[cursor++]);
      previous = p;
    }
  while (cursor < size) assert (!expected[cursor++]);
}

static void
aggregate_model (void)
{
  enum { SIZE = 97 };
  unsigned char original[SIZE], model[SIZE], valid[SIZE] = { 0 };
  unsigned char modified[SIZE] = { 0 }, source[SIZE], output[SIZE];
  for (int i = 0; i < SIZE; ++i) original[i] = model[i] = (unsigned char) i;
  struct newtm_context *ctx = NULL;
  begin (&ctx, original, SIZE, NEWTM_OBJECT_AGGREGATE);
  uint32_t random = 12345;
  for (unsigned int step = 0; step < 500; ++step)
    {
      random = random * 1664525u + 1013904223u;
      size_t offset = random % SIZE;
      size_t size = 1 + (random >> 8) % (SIZE - offset);
      if (step % 3)
        {
          memset (source, (unsigned char) step, size);
          assert (newtm_write (ctx, original, offset, source, size)
                  == NEWTM_ACCESS_OK);
          memcpy (model + offset, source, size);
          memset (modified + offset, 1, size);
          for (size_t i = 0; i < size; ++i)
            assert (source[i] == (unsigned char) step);
        }
      else
        {
          memset (output, 0xee, sizeof output);
          assert (newtm_read (ctx, original, offset, output, size)
                  == NEWTM_ACCESS_OK);
          assert (!memcmp (output, model + offset, size));
          for (size_t i = size; i < SIZE; ++i) assert (output[i] == 0xee);
        }
      memset (valid + offset, 1, size);
      struct newtm_aggregate_value *v = &ctx->records->value.aggregate;
      check_intervals (v->valid, valid, SIZE);
      check_intervals (v->modified, modified, SIZE);
      for (size_t i = 0; i < SIZE; ++i)
        {
          assert (original[i] == (unsigned char) i);
          if (valid[i]) assert (v->bytes[i] == model[i]);
          assert (!modified[i] || valid[i]);
        }
      /* Distinct ownership: merging/freeing one list cannot damage the other. */
      for (struct newtm_interval *a = v->valid; a; a = a->next)
        for (struct newtm_interval *b = v->modified; b; b = b->next)
          assert (a != b);
    }
  assert (newtm_read (ctx, original, 0, output, SIZE) == NEWTM_ACCESS_OK);
  assert (!memcmp (output, model, SIZE));
  discard (&ctx);
  for (int i = 0; i < SIZE; ++i) assert (original[i] == (unsigned char) i);
}

static void
guarded_fields (void)
{
  long page = sysconf (_SC_PAGESIZE);
  assert (page > 0);
  size_t size = (size_t) page * 3;
  unsigned char *object = mmap (NULL, size, PROT_NONE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert (object != MAP_FAILED);
  assert (!mprotect (object + page, page, PROT_READ | PROT_WRITE));
  memset (object + page, 0x33, page);
  struct newtm_context *ctx = NULL;
  begin (&ctx, object, size, NEWTM_OBJECT_AGGREGATE);
  unsigned char first[] = { 1, 2, 3, 4 }, second[] = { 5, 6, 7, 8 };
  unsigned char output[16];
  /* Whole aggregate is inaccessible except one field's page. */
  assert (newtm_read (ctx, object, page + 4, output, 4) == NEWTM_ACCESS_OK);
  for (int i = 0; i < 4; ++i) assert (output[i] == 0x33);
  assert (newtm_write (ctx, object, page + 6, first, 4) == NEWTM_ACCESS_OK);
  assert (newtm_write (ctx, object, page + 12, second, 4) == NEWTM_ACCESS_OK);
  assert (newtm_read (ctx, object, page, output, sizeof output) == NEWTM_ACCESS_OK);
  for (int i = 0; i < 16; ++i)
    assert (output[i] == (i >= 6 && i < 10 ? first[i - 6]
                         : i >= 12 ? second[i - 12] : 0x33));
  /* Writes need not read even the original bytes being replaced. */
  assert (newtm_write (ctx, object, 0, first, 4) == NEWTM_ACCESS_OK);
  assert (newtm_read (ctx, object, 0, output, 4) == NEWTM_ACCESS_OK);
  assert (!memcmp (output, first, 4));
  assert (!mprotect (object + page, page, PROT_NONE));
  /* A valid cached range must never be reloaded from original memory. */
  assert (newtm_read (ctx, object, page, output, 16) == NEWTM_ACCESS_OK);
  assert (!memcmp (output + 6, first, 4) && !memcmp (output + 12, second, 4));
  discard (&ctx);
  begin (&ctx, object, 4, NEWTM_OBJECT_SCALAR);
  assert (newtm_write (ctx, object, 0, first, 4) == NEWTM_ACCESS_OK);
  assert (newtm_read (ctx, object, 0, output, 4) == NEWTM_ACCESS_OK);
  assert (!memcmp (output, first, 4)); /* Scalar write also needs no original. */
  discard (&ctx);
  assert (!mprotect (object + page, page, PROT_READ));
  for (long i = 0; i < page; ++i) assert (object[page + i] == 0x33);
  assert (!munmap (object, size));
}

static void
interval_shapes (void)
{
  unsigned char original[32] = { 0 }, source[32], output[32];
  memset (source, 0x55, sizeof source);
  struct newtm_context *ctx = NULL;
  begin (&ctx, original, sizeof original, NEWTM_OBJECT_AGGREGATE);
  const size_t offsets[] = { 8, 16, 12, 4, 20 };
  for (size_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i)
    assert (newtm_write (ctx, original, offsets[i], source, 4) == NEWTM_ACCESS_OK);
  struct newtm_aggregate_value *v = &ctx->records->value.aggregate;
  assert (v->valid->begin == 4 && v->valid->end == 24 && !v->valid->next);
  assert (v->modified->begin == 4 && v->modified->end == 24 && !v->modified->next);
  assert (newtm_read (ctx, original, 0, output, 32) == NEWTM_ACCESS_OK);
  for (int i = 0; i < 32; ++i)
    assert (output[i] == (i >= 4 && i < 24 ? 0x55 : 0));
  assert (v->valid->begin == 0 && v->valid->end == 32 && !v->valid->next);
  assert (v->modified->begin == 4 && v->modified->end == 24 && !v->modified->next);
  struct newtm_interval *valid = v->valid, *modified = v->modified;
  assert (newtm_write (ctx, original, 30, source, 4) == NEWTM_ACCESS_INVALID);
  assert (ctx->failure.reason == NEWTM_REASON_BOUNDS);
  assert (ctx->failure.offset == 30 && ctx->failure.size == 4);
  assert (v->valid == valid && v->modified == modified);
  assert (!memcmp (v->bytes, output, 32));
  for (int i = 0; i < 32; ++i) assert (!original[i]);
  discard (&ctx);
}

static void
scalars_and_failures (void)
{
  int object = 41, source = 99, output = -7;
  struct newtm_context *a = NULL, *b = NULL;
  begin (&a, &object, sizeof object, NEWTM_OBJECT_SCALAR);
  assert (newtm_read (a, &object, 0, &output, sizeof output) == NEWTM_ACCESS_OK);
  assert (output == 41 && a->records->value.scalar.valid);
  assert (!a->records->value.scalar.modified);
  assert (newtm_write (a, &object, 0, &source, sizeof source) == NEWTM_ACCESS_OK);
  assert (object == 41 && source == 99 && a->records->value.scalar.modified);
  bool outer;
  assert (newtm_begin (NULL, &a, &outer) == NEWTM_LIFECYCLE_OK && !outer);
  assert (newtm_read (a, &object, 0, &output, sizeof output) == NEWTM_ACCESS_OK);
  assert (output == 99);
  assert (newtm_end (&a) == NEWTM_LIFECYCLE_OK);
  begin (&b, &object, sizeof object, NEWTM_OBJECT_SCALAR);
  output = -7;
  assert (newtm_read (b, &object, 0, &output, sizeof output)
          == NEWTM_ACCESS_CONFLICT && output == -7);
  assert (!b->records->value.scalar.bytes);
  discard (&b);
  begin (&b, &object, sizeof object, NEWTM_OBJECT_SCALAR);
  long page = sysconf (_SC_PAGESIZE);
  assert (page > 0);
  void *unreadable = mmap (NULL, page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert (unreadable != MAP_FAILED);
  assert (newtm_write (b, &object, 0, unreadable, sizeof object)
          == NEWTM_ACCESS_CONFLICT); /* No premature read of SOURCE. */
  assert (!b->records->value.scalar.bytes && object == 41);
  assert (!munmap (unreadable, page));
  discard (&b);
  discard (&a);

  /* Failed promotion leaves cached data and the read reservation untouched. */
  begin (&a, &object, sizeof object, NEWTM_OBJECT_SCALAR);
  begin (&b, &object, sizeof object, NEWTM_OBJECT_SCALAR);
  assert (newtm_read (a, &object, 0, &output, sizeof output) == NEWTM_ACCESS_OK);
  assert (newtm_read (b, &object, 0, &output, sizeof output) == NEWTM_ACCESS_OK);
  assert (newtm_write (a, &object, 0, &source, sizeof source)
          == NEWTM_ACCESS_CONFLICT);
  assert (!a->records->value.scalar.modified);
  assert (!memcmp (a->records->value.scalar.bytes, &object, sizeof object));
  discard (&a);
  discard (&b);
  for (int mode = 0; mode < 7; ++mode)
    {
      begin (&a, &object, sizeof object, NEWTM_OBJECT_SCALAR);
      assert (newtm_write (a, &object, 0, &source, sizeof source) == NEWTM_ACCESS_OK);
      size_t offset = mode == 0 ? SIZE_MAX : mode == 1 ? 1 : 0;
      size_t size = mode == 2 ? 0 : mode == 3 ? SIZE_MAX
        : mode == 4 ? sizeof output - 1 : sizeof output;
      void *destination = mode == 5 ? NULL : mode == 6 ? &object : &output;
      output = -7;
      assert (newtm_read (a, &object, offset, destination, size)
              == NEWTM_ACCESS_INVALID);
      assert (output == -7 && object == 41 && source == 99);
      assert (!memcmp (a->records->value.scalar.bytes, &source, sizeof source));
      assert (newtm_read (a, &object, 0, &output, sizeof output)
              == NEWTM_ACCESS_INVALID && output == -7);
      discard (&a);
    }
  output = -7;
  assert (newtm_read (NULL, &object, 0, &output, sizeof output)
          == NEWTM_ACCESS_INVALID && output == -7);
  assert (newtm_write (NULL, &object, 0, &source, sizeof source)
          == NEWTM_ACCESS_INVALID && source == 99);
  begin (&a, &object, sizeof object, NEWTM_OBJECT_SCALAR);
  assert (newtm_read (a, &source, 0, &output, sizeof output)
          == NEWTM_ACCESS_INVALID && output == -7);
  assert (a->failure.reason == NEWTM_REASON_UNREGISTERED);
  discard (&a);
}

struct shared_values
{
  pthread_barrier_t barrier;
  int original;
};
struct value_worker { struct shared_values *shared; int index; };

static void
wait_values (struct shared_values *s)
{
  int result = pthread_barrier_wait (&s->barrier);
  assert (result == 0 || result == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void *
value_worker (void *argument)
{
  struct value_worker *w = argument;
  struct shared_values *s = w->shared;
  for (int round = 0; round < 32; ++round)
    {
      struct newtm_context *ctx = NULL;
      int output = -1, source = 99 + round;
      begin (&ctx, &s->original, sizeof s->original, NEWTM_OBJECT_SCALAR);
      assert (newtm_read (ctx, &s->original, 0, &output, sizeof output)
              == NEWTM_ACCESS_OK && output == 41);
      wait_values (s);
      if (w->index == 0)
        assert (newtm_write (ctx, &s->original, 0, &source, sizeof source)
                == NEWTM_ACCESS_CONFLICT);
      wait_values (s);
      if (w->index == 0) discard (&ctx);
      wait_values (s);
      if (w->index == 1)
        {
          assert (newtm_write (ctx, &s->original, 0, &source, sizeof source)
                  == NEWTM_ACCESS_OK);
          assert (newtm_read (ctx, &s->original, 0, &output, sizeof output)
                  == NEWTM_ACCESS_OK && output == source);
          discard (&ctx);
        }
      wait_values (s);
    }
  return NULL;
}

static void
concurrent_values (void)
{
  struct shared_values shared = { .original = 41 };
  assert (!pthread_barrier_init (&shared.barrier, NULL, 2));
  pthread_t threads[2];
  struct value_worker workers[2] = { { &shared, 0 }, { &shared, 1 } };
  for (int i = 0; i < 2; ++i)
    assert (!pthread_create (&threads[i], NULL, value_worker, &workers[i]));
  for (int i = 0; i < 2; ++i) assert (!pthread_join (threads[i], NULL));
  assert (!pthread_barrier_destroy (&shared.barrier));
  assert (shared.original == 41);
}

int
main (void)
{
  aggregate_model ();
  guarded_fields ();
  interval_shapes ();
  scalars_and_failures ();
  concurrent_values ();
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  puts ("[teste] values OK: scalar values, partial aggregates, interval unions, preserved writes");
  return 0;
}
