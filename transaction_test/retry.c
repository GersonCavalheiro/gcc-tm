/* Whole prepared-body restart after a real nested conflict.  Test-only lease
   wrapping releases a competing transaction AFTER failed-attempt teardown. */
#include "newtm-context.h"
#include <pthread.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>

static int prefix = 10, inner = 20, contended = 5;
static pthread_barrier_t ready, cleaned, unlocked;
static _Thread_local bool controller;
static struct newtm_context **observed_slot;
static unsigned int calls, inner_calls, releases, lease_releases;

static void
wait_barrier (pthread_barrier_t *barrier)
{
  int result = pthread_barrier_wait (barrier);
  assert (!result || result == PTHREAD_BARRIER_SERIAL_THREAD);
}

extern void __real_newtm_hash_release (struct newtm_hash_table *, struct newtm_hash_entry *);
void
__wrap_newtm_hash_release (struct newtm_hash_table *table, struct newtm_hash_entry *entry)
{
  if (controller)
    { assert (!*observed_slot); ++releases; }
  __real_newtm_hash_release (table, entry);
}

extern enum newtm_hash_status __real_newtm_hash_owner_release (struct newtm_hash_owner_lease *);
enum newtm_hash_status
__wrap_newtm_hash_owner_release (struct newtm_hash_owner_lease *lease)
{
  if (controller)
    {
      assert (!*observed_slot);
      ++lease_releases;
      assert (releases == 3 * lease_releases);
      assert (newtm_hash_count (lease->table) == (lease_releases == 1 ? 1 : 0));
      if (lease_releases == 1)
        assert (prefix == 10 && inner == 20 && contended == 5);
    }
  enum newtm_hash_status status = __real_newtm_hash_owner_release (lease);
  if (controller && lease_releases == 1)
    {
      wait_barrier (&cleaned);
      wait_barrier (&unlocked);
    }
  return status;
}

static void *
locker (void *unused)
{
  (void) unused;
  struct newtm_context *ctx = NULL;
  struct newtm_hash_owner_lease lease = { 0 };
  bool outer;
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, &ctx, &outer) == NEWTM_LIFECYCLE_OK && outer);
  assert (newtm_register (ctx, &contended, sizeof contended, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_OK);
  int value = 50;
  assert (newtm_write (ctx, &contended, 0, &value, sizeof value) == NEWTM_ACCESS_OK);
  wait_barrier (&ready);
  wait_barrier (&cleaned);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_OK && !ctx);
  wait_barrier (&unlocked);
  return NULL;
}

static enum newtm_lifecycle_status
nested_body (struct newtm_context *ctx, int *result)
{
  ++inner_calls; /* Test instrumentation, outside program transactional state. */
  bool outer;
  assert (newtm_begin (NULL, ctx->owning_slot, &outer) == NEWTM_LIFECYCLE_OK && !outer);
  assert (newtm_register (ctx, &inner, sizeof inner, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_OK);
  int value;
  assert (newtm_read (ctx, &inner, 0, &value, sizeof value) == NEWTM_ACCESS_OK);
  assert (value == 20); /* Failed attempt's private increment was discarded. */
  ++value;
  assert (newtm_write (ctx, &inner, 0, &value, sizeof value) == NEWTM_ACCESS_OK);
  assert (newtm_register (ctx, &contended, sizeof contended, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_OK);
  int readback = -1;
  enum newtm_access_status access
    = newtm_read (ctx, &contended, 0, &readback, sizeof readback);
  if (access == NEWTM_ACCESS_CONFLICT)
    {
      assert (readback == -1 && calls == 1 && ctx->depth == 2);
      return NEWTM_LIFECYCLE_CONFLICT; /* Do not resume/consume an invalid result. */
    }
  if (access != NEWTM_ACCESS_OK) return NEWTM_LIFECYCLE_INVALID;
  *result = readback;
  return newtm_end (ctx->owning_slot);
}

static enum newtm_lifecycle_status
outer_body (struct newtm_context *ctx, void *unused)
{
  (void) unused;
  observed_slot = ctx->owning_slot; /* Test-only lifetime observation in hooks. */
  ++calls;
  assert (calls <= 2 && ctx->depth == 1 && !ctx->records);
  assert (newtm_register (ctx, &prefix, sizeof prefix, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_OK);
  int value, result = -1;
  assert (newtm_read (ctx, &prefix, 0, &value, sizeof value) == NEWTM_ACCESS_OK);
  assert (value == 10);
  ++value;
  assert (newtm_write (ctx, &prefix, 0, &value, sizeof value) == NEWTM_ACCESS_OK);
  enum newtm_lifecycle_status status = nested_body (ctx, &result);
  if (status != NEWTM_LIFECYCLE_OK) return status;
  assert (calls == 2 && result == 50);
  value += result;
  assert (newtm_write (ctx, &prefix, 0, &value, sizeof value) == NEWTM_ACCESS_OK);
  return NEWTM_LIFECYCLE_OK;
}

int
main (void)
{
  assert (!pthread_barrier_init (&ready, NULL, 2));
  assert (!pthread_barrier_init (&cleaned, NULL, 2));
  assert (!pthread_barrier_init (&unlocked, NULL, 2));
  struct newtm_hash_owner_lease observer = { 0 };
  assert (newtm_hash_owner_acquire (&observer) == NEWTM_HASH_OK);
  pthread_t thread;
  assert (!pthread_create (&thread, NULL, locker, NULL));
  wait_barrier (&ready);
  controller = true;
  assert (newtm_execute (outer_body, NULL) == NEWTM_LIFECYCLE_OK);
  controller = false;
  assert (calls == 2 && inner_calls == 2 && releases == 6 && lease_releases == 2);
  assert (prefix == 61 && inner == 21 && contended == 50);
  assert (!newtm_hash_count (observer.table));
  assert (newtm_hash_owner_release (&observer) == NEWTM_HASH_OK);
  assert (!pthread_join (thread, NULL));
  assert (!pthread_barrier_destroy (&ready));
  assert (!pthread_barrier_destroy (&cleaned));
  assert (!pthread_barrier_destroy (&unlocked));
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  puts ("[teste] retry OK: whole outer body restarted, nested failure propagated, each attempt cleaned once");
  return 0;
}
