/* Global range admission, including a hash acquisition still pending.
   Wrapping is test-only; no original object values are accessed. */
#include "newtm-context.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>

static unsigned char memory[64];
static pthread_barrier_t gate;
static _Thread_local bool pause_acquire;
static _Thread_local bool fail_acquire;

static void wait_gate (void)
{
  int result = pthread_barrier_wait (&gate);
  assert (!result || result == PTHREAD_BARRIER_SERIAL_THREAD);
}
static void begin (struct newtm_context **ctx)
{
  struct newtm_hash_owner_lease lease = { 0 };
  bool outer;
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, ctx, &outer) == NEWTM_LIFECYCLE_OK && outer);
}
static void discard (struct newtm_context **ctx)
{ assert (newtm_discard (ctx) == NEWTM_LIFECYCLE_OK && !*ctx); }
static enum newtm_lifecycle_status reg (struct newtm_context *ctx, size_t start, size_t size)
{ return newtm_register (ctx, memory + start, size, NEWTM_OBJECT_AGGREGATE); }

extern enum newtm_hash_status __real_newtm_hash_acquire
  (struct newtm_hash_table *, const void *, size_t, struct newtm_hash_entry **);
enum newtm_hash_status
__wrap_newtm_hash_acquire (struct newtm_hash_table *table, const void *address,
                          size_t size, struct newtm_hash_entry **out)
{
  if (fail_acquire) return NEWTM_HASH_SIZE_MISMATCH;
  if (pause_acquire) { wait_gate (); wait_gate (); }
  return __real_newtm_hash_acquire (table, address, size, out);
}

static void *pending (void *unused)
{
  (void) unused;
  struct newtm_context *ctx = NULL;
  begin (&ctx);
  pause_acquire = true;
  assert (reg (ctx, 0, 16) == NEWTM_LIFECYCLE_OK);
  pause_acquire = false;
  wait_gate (); wait_gate ();
  discard (&ctx);
  return NULL;
}

struct racer { int index; enum newtm_lifecycle_status status; };
static void *race (void *argument)
{
  struct racer *r = argument;
  struct newtm_context *ctx = NULL;
  begin (&ctx);
  wait_gate ();
  r->status = reg (ctx, 4 * r->index, 16);
  if (r->status == NEWTM_LIFECYCLE_INVALID)
    assert (ctx->failure.reason == NEWTM_REASON_OVERLAPPING_REGISTRATION
            && !ctx->records);
  wait_gate (); wait_gate ();
  discard (&ctx);
  return NULL;
}

int main (void)
{
  struct newtm_hash_owner_lease observer = { 0 };
  assert (newtm_hash_owner_acquire (&observer) == NEWTM_HASH_OK);
  struct newtm_context *a = NULL, *b = NULL;
  begin (&a); begin (&b);
  assert (reg (a, 0, 16) == NEWTM_LIFECYCLE_OK);
  assert (reg (a, 0, 16) == NEWTM_LIFECYCLE_OK);
  assert (reg (b, 0, 16) == NEWTM_LIFECYCLE_OK);
  assert (newtm_hash_count (observer.table) == 1);
  assert (reg (b, 8, 16) == NEWTM_LIFECYCLE_INVALID);
  assert (b->failure.reason == NEWTM_REASON_OVERLAPPING_REGISTRATION);
  assert (b->records && !b->records->next);
  discard (&b);
  assert (reg (a, 16, 16) == NEWTM_LIFECYCLE_OK); /* Adjacent is not overlap. */
  discard (&a);
  begin (&a);
  assert (reg (a, 8, 24) == NEWTM_LIFECYCLE_OK); /* Ranges were removed. */
  begin (&b);
  assert (reg (b, 0, 16) == NEWTM_LIFECYCLE_INVALID); /* Reverse direction. */
  discard (&b); discard (&a);
  begin (&a); begin (&b);
  assert (reg (a, 0, 16) == NEWTM_LIFECYCLE_OK);
  assert (newtm_register (b, memory, 16, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_INVALID);
  assert (b->failure.reason == NEWTM_REASON_INCOMPATIBLE_REGISTRATION);
  discard (&b); discard (&a);
  begin (&a);
  assert (newtm_register (a, (void *) (UINTPTR_MAX - 1), 4, NEWTM_OBJECT_AGGREGATE)
          == NEWTM_LIFECYCLE_INVALID && !a->records);
  discard (&a);

  begin (&a);
  fail_acquire = true;
  assert (reg (a, 0, 16) == NEWTM_LIFECYCLE_INVALID && !a->records);
  fail_acquire = false;
  discard (&a);
  begin (&a);
  assert (reg (a, 8, 16) == NEWTM_LIFECYCLE_OK); /* Failed acquisition rolled back index. */
  discard (&a);

  assert (!pthread_barrier_init (&gate, NULL, 2));
  pthread_t thread;
  assert (!pthread_create (&thread, NULL, pending, NULL));
  wait_gate ();
  assert (!newtm_hash_count (observer.table)); /* Candidate precedes hash insert. */
  begin (&a);
  assert (reg (a, 8, 16) == NEWTM_LIFECYCLE_INVALID && !a->records);
  discard (&a);
  wait_gate (); wait_gate ();
  assert (newtm_hash_count (observer.table) == 1);
  wait_gate ();
  assert (!pthread_join (thread, NULL));
  assert (!pthread_barrier_destroy (&gate));

  for (int round = 0; round < 32; ++round)
    {
      assert (!pthread_barrier_init (&gate, NULL, 3));
      pthread_t threads[2];
      struct racer racers[2] = { { 0, NEWTM_LIFECYCLE_INVALID },
                                { 1, NEWTM_LIFECYCLE_INVALID } };
      for (int i = 0; i < 2; ++i)
        assert (!pthread_create (&threads[i], NULL, race, &racers[i]));
      wait_gate (); wait_gate ();
      assert ((racers[0].status == NEWTM_LIFECYCLE_OK)
              != (racers[1].status == NEWTM_LIFECYCLE_OK));
      assert (newtm_hash_count (observer.table) == 1);
      wait_gate ();
      for (int i = 0; i < 2; ++i) assert (!pthread_join (threads[i], NULL));
      assert (!pthread_barrier_destroy (&gate));
      assert (!newtm_hash_count (observer.table));
    }
  begin (&a);
  assert (reg (a, 0, 64) == NEWTM_LIFECYCLE_OK);
  assert (newtm_end (&a) == NEWTM_LIFECYCLE_OK && !a);
  begin (&a);
  assert (reg (a, 4, 16) == NEWTM_LIFECYCLE_OK); /* Commit also removes index. */
  discard (&a);
  assert (newtm_hash_owner_release (&observer) == NEWTM_HASH_OK);
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  puts ("[teste] overlap OK: global atomic admission, pending acquisitions, cleanup");
  return 0;
}
