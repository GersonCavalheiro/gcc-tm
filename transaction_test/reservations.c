/* Policy tests through private interfaces, with metadata inspected only under
   entry_visit.  Barriers keep conflicting attempts alive until inspection. */
#include "newtm-context.h"
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#undef NDEBUG
#include <assert.h>

struct snapshot
{
  void *policy;
  size_t readers;
  struct newtm_record *writer;
};

static void
inspect (const void *object, size_t size, void **data, void *argument)
{
  (void) object; (void) size;
  struct snapshot *s = argument;
  struct newtm_object_policy *p = *data;
  s->policy = p;
  if (!p) return;
  s->writer = p->writer;
  for (struct newtm_record *r = p->readers; r; r = r->reader_next)
    {
      assert (r->reservation == NEWTM_RESERVATION_READ);
      assert (++s->readers <= 8);
    }
  assert (!s->writer || !s->readers);
  if (s->writer)
    assert (s->writer->reservation == NEWTM_RESERVATION_WRITE);
}

static struct snapshot
snapshot (struct newtm_context *ctx)
{
  struct snapshot s = { 0 };
  newtm_hash_entry_visit (ctx->lease.table, ctx->records->entry, inspect, &s);
  return s;
}

static void
begin (struct newtm_context **slot, const void *object, size_t size)
{
  struct newtm_hash_owner_lease lease = { 0 };
  bool outer;
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, slot, &outer) == NEWTM_LIFECYCLE_OK && outer);
  assert (newtm_register (*slot, object, size, NEWTM_OBJECT_AGGREGATE)
          == NEWTM_LIFECYCLE_OK);
}

static void
discard (struct newtm_context **slot)
{
  assert (newtm_discard (slot) == NEWTM_LIFECYCLE_OK && !*slot);
}

static void *
foreign_reserve (void *argument)
{
  struct newtm_context *ctx = argument;
  assert (newtm_reserve (ctx, ctx->records->object, NEWTM_RESERVATION_READ)
          == NEWTM_ACCESS_INVALID);
  return NULL;
}

static void
matrix (const void *object, size_t size)
{
  struct newtm_context *a = NULL, *b = NULL;
  begin (&a, object, size);
  begin (&b, object, size);
  pthread_t foreign;
  assert (!pthread_create (&foreign, NULL, foreign_reserve, a));
  assert (!pthread_join (foreign, NULL));
  assert (a->state == NEWTM_ATTEMPT_ACTIVE && a->depth == 1);
  assert (!snapshot (a).policy); /* Register creates no metadata/reservations. */
  assert (newtm_reserve (a, object, NEWTM_RESERVATION_READ) == NEWTM_ACCESS_OK);
  void *policy = snapshot (a).policy;
  assert (policy && snapshot (a).readers == 1);
  assert (newtm_reserve (a, object, NEWTM_RESERVATION_READ) == NEWTM_ACCESS_OK);
  assert (snapshot (a).readers == 1 && snapshot (a).policy == policy);
  bool outer;
  assert (newtm_begin (NULL, &a, &outer) == NEWTM_LIFECYCLE_OK && !outer);
  assert (newtm_reserve (a, object, NEWTM_RESERVATION_WRITE) == NEWTM_ACCESS_OK);
  assert (newtm_end (&a) == NEWTM_LIFECYCLE_OK);
  assert (snapshot (a).writer == a->records && !snapshot (a).readers);
  assert (newtm_reserve (a, object, NEWTM_RESERVATION_READ) == NEWTM_ACCESS_OK);
  assert (newtm_reserve (a, object, NEWTM_RESERVATION_WRITE) == NEWTM_ACCESS_OK);
  assert (snapshot (a).writer == a->records); /* No downgrade on own read. */
  assert (!a->records->value.aggregate.bytes);
  assert (newtm_reserve (b, object, NEWTM_RESERVATION_READ)
          == NEWTM_ACCESS_CONFLICT);
  assert (a->state == NEWTM_ATTEMPT_ACTIVE);
  assert (b->state == NEWTM_ATTEMPT_CONFLICTED);
  assert (b->records->reservation == NEWTM_RESERVATION_NONE);
  assert (b->failure.object == object && b->failure.size == size);
  discard (&b);
  begin (&b, object, size);
  assert (newtm_reserve (b, object, NEWTM_RESERVATION_WRITE)
          == NEWTM_ACCESS_CONFLICT);
  discard (&a);
  assert (!snapshot (b).writer && !snapshot (b).readers);
  assert (snapshot (b).policy == policy); /* Retained reference owns metadata. */
  assert (newtm_reserve (b, object, NEWTM_RESERVATION_READ)
          == NEWTM_ACCESS_CONFLICT); /* Condemned attempt cannot resume. */
  discard (&b);

  begin (&a, object, size);
  begin (&b, object, size);
  assert (!snapshot (a).policy); /* Last release destroyed the old metadata. */
  assert (newtm_reserve (a, object, NEWTM_RESERVATION_READ) == NEWTM_ACCESS_OK);
  assert (newtm_reserve (b, object, NEWTM_RESERVATION_READ) == NEWTM_ACCESS_OK);
  assert (snapshot (a).readers == 2);
  assert (newtm_reserve (a, object, NEWTM_RESERVATION_WRITE)
          == NEWTM_ACCESS_CONFLICT);
  assert (snapshot (b).readers == 2 && !snapshot (b).writer);
  assert (a->records->reservation == NEWTM_RESERVATION_READ);
  assert (b->state == NEWTM_ATTEMPT_ACTIVE);
  discard (&a); /* Non-head removal leaves the other reader intact. */
  assert (snapshot (b).readers == 1);
  assert (newtm_reserve (b, object, NEWTM_RESERVATION_WRITE) == NEWTM_ACCESS_OK);
  assert (snapshot (b).writer == b->records && !snapshot (b).readers);
  discard (&b);

  begin (&a, object, size);
  assert (newtm_reserve (a, object, NEWTM_RESERVATION_WRITE) == NEWTM_ACCESS_OK);
  begin (&b, object, size);
  assert (newtm_reserve (b, object, NEWTM_RESERVATION_WRITE)
          == NEWTM_ACCESS_CONFLICT);
  discard (&b);
  discard (&a);

  /* Active readers also exclude a new writer that has no old reservation. */
  begin (&a, object, size);
  begin (&b, object, size);
  assert (newtm_reserve (a, object, NEWTM_RESERVATION_READ) == NEWTM_ACCESS_OK);
  assert (newtm_reserve (b, object, NEWTM_RESERVATION_WRITE)
          == NEWTM_ACCESS_CONFLICT);
  assert (snapshot (a).readers == 1 && !snapshot (a).writer);
  discard (&b);
  discard (&a);

  for (int mode = 0; mode < 4; ++mode)
    {
      begin (&a, object, size);
      const void *address = mode == 0 ? &mode : mode == 1 ? NULL : object;
      enum newtm_reservation request = mode < 2 ? NEWTM_RESERVATION_READ
        : mode == 2 ? NEWTM_RESERVATION_NONE : (enum newtm_reservation) 99;
      assert (newtm_reserve (a, address, request) == NEWTM_ACCESS_INVALID);
      assert (a->state == NEWTM_ATTEMPT_INVALID && a->depth == 1);
      assert (a->failure.reason == (mode == 0 ? NEWTM_REASON_UNREGISTERED
                                   : NEWTM_REASON_INVALID_OPERATION));
      assert (!snapshot (a).policy);
      assert (newtm_reserve (a, object, NEWTM_RESERVATION_READ)
              == NEWTM_ACCESS_INVALID);
      discard (&a);
    }
  assert (newtm_reserve (NULL, object, NEWTM_RESERVATION_READ)
          == NEWTM_ACCESS_INVALID);
}

enum { THREADS = 4, ROUNDS = 16 };
struct race
{
  pthread_barrier_t barrier, allocation_barrier;
  const void *object;
  size_t size;
  enum newtm_reservation mode;
  enum newtm_access_status granted[THREADS], promoted[THREADS];
};
struct worker { struct race *race; int index; };
static _Thread_local struct race *allocation_race;
static _Thread_local size_t allocations;

extern void *__real_gomp_malloc_cleared (size_t);

/* Test-only link wrapping forces every worker to prepare a candidate before
   any can install it.  Waiting here also detects allocation under the bucket:
   such a regression would deadlock and hit the external test timeout. */
void *
__wrap_gomp_malloc_cleared (size_t size)
{
  void *p = __real_gomp_malloc_cleared (size);
  if (allocation_race)
    {
      assert (size == sizeof (struct newtm_object_policy));
      ++allocations;
      int result = pthread_barrier_wait (&allocation_race->allocation_barrier);
      assert (result == 0 || result == PTHREAD_BARRIER_SERIAL_THREAD);
    }
  return p;
}

static void
wait_barrier (struct race *race)
{
  int result = pthread_barrier_wait (&race->barrier);
  assert (result == 0 || result == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void *
worker (void *argument)
{
  struct worker *w = argument;
  struct race *r = w->race;
  for (int round = 0; round < ROUNDS; ++round)
    {
      struct newtm_context *ctx = NULL;
      begin (&ctx, r->object, r->size);
      wait_barrier (r);
      allocation_race = r;
      r->granted[w->index] = newtm_reserve (ctx, r->object, r->mode);
      allocation_race = NULL;
      wait_barrier (r);
      wait_barrier (r); /* Main has inspected simultaneous grants. */
      if (r->mode == NEWTM_RESERVATION_READ)
        r->promoted[w->index]
          = newtm_reserve (ctx, r->object, NEWTM_RESERVATION_WRITE);
      wait_barrier (r);
      wait_barrier (r); /* Keep all old reservations until main inspects. */
      discard (&ctx);
      wait_barrier (r);
      wait_barrier (r); /* Observer releases the final reference before restart. */
    }
  assert (allocations == ROUNDS);
  return NULL;
}

static void
concurrent (const void *object, size_t size, enum newtm_reservation mode)
{
  struct race race = { .object = object, .size = size, .mode = mode };
  assert (!pthread_barrier_init (&race.barrier, NULL, THREADS + 1));
  assert (!pthread_barrier_init (&race.allocation_barrier, NULL, THREADS));
  struct worker workers[THREADS];
  pthread_t threads[THREADS];
  for (int i = 0; i < THREADS; ++i)
    {
      workers[i] = (struct worker) { &race, i };
      assert (!pthread_create (&threads[i], NULL, worker, &workers[i]));
    }
  for (int round = 0; round < ROUNDS; ++round)
    {
      struct newtm_context *observer = NULL;
      begin (&observer, object, size);
      wait_barrier (&race);
      wait_barrier (&race);
      int successes = 0;
      for (int i = 0; i < THREADS; ++i)
        {
          assert (race.granted[i] == NEWTM_ACCESS_OK
                  || race.granted[i] == NEWTM_ACCESS_CONFLICT);
          successes += race.granted[i] == NEWTM_ACCESS_OK;
        }
      struct snapshot s = snapshot (observer);
      assert (s.policy);
      if (mode == NEWTM_RESERVATION_READ)
        assert (successes == THREADS && s.readers == THREADS && !s.writer);
      else
        assert (successes == 1 && s.writer && !s.readers);
      wait_barrier (&race);
      wait_barrier (&race);
      if (mode == NEWTM_RESERVATION_READ)
        {
          for (int i = 0; i < THREADS; ++i)
            assert (race.promoted[i] == NEWTM_ACCESS_CONFLICT);
          assert (snapshot (observer).readers == THREADS);
        }
      wait_barrier (&race);
      wait_barrier (&race);
      s = snapshot (observer);
      assert (s.policy && !s.readers && !s.writer);
      discard (&observer); /* Next round races lazy installation again. */
      wait_barrier (&race);
    }
  for (int i = 0; i < THREADS; ++i) assert (!pthread_join (threads[i], NULL));
  assert (!pthread_barrier_destroy (&race.barrier));
  assert (!pthread_barrier_destroy (&race.allocation_barrier));
}

int
main (void)
{
  long size = sysconf (_SC_PAGESIZE);
  assert (size > 0);
  void *object = mmap (NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert (object != MAP_FAILED);
  matrix (object, size);
  concurrent (object, size, NEWTM_RESERVATION_READ);
  concurrent (object, size, NEWTM_RESERVATION_WRITE);
  assert (!munmap (object, size));
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  puts ("[teste] reservations OK: shared readers, exclusive writer, promotion, conflicts, cleanup");
  return 0;
}
