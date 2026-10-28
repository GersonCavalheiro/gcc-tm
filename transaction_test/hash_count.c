/* Exercise real count and entry operations. Private access is limited to
   overflow injection; no production hooks or exported symbols are added. */
#include "../gcc-src-tm/libgomp/newtm-hash.c"

#undef NDEBUG
#include <assert.h>
#include <pthread.h>

static void
check_counts (size_t capacity)
{
  int objects[2] = { 17, 29 };
  struct newtm_hash_table *table;
  struct newtm_hash_entry *first, *second, *again;
  assert (newtm_hash_create (capacity, NULL, &table) == NEWTM_HASH_OK);
  assert (newtm_hash_count (table) == 0);
  assert (newtm_hash_acquire (table, objects, sizeof (int), &first)
	  == NEWTM_HASH_OK);
  assert (newtm_hash_count (table) == 1);
  assert (newtm_hash_lookup_acquire (table, objects, &again) == NEWTM_HASH_OK);
  assert (again == first && newtm_hash_count (table) == 1);
  assert (first->references == 2);
  assert (newtm_hash_acquire (table, &objects[1], sizeof (int), &second)
	  == NEWTM_HASH_OK);
  assert (newtm_hash_count (table) == 2);
  newtm_hash_release (table, first);
  assert (newtm_hash_count (table) == 2 && again->references == 1);
  newtm_hash_release (table, again);
  assert (newtm_hash_count (table) == 1);
  newtm_hash_release (table, second);
  assert (newtm_hash_count (table) == 0);
  assert (objects[0] == 17 && objects[1] == 29);
  assert (newtm_hash_destroy (table) == NEWTM_HASH_OK);
}

static void
check_saturation (void)
{
  struct newtm_hash_table *table;
  assert (newtm_hash_create (3, NULL, &table) == NEWTM_HASH_OK);
  /* Quiescent test-only counter injection, restored before normal operations. */
  table->buckets[0].entry_count = SIZE_MAX - 1;
  table->buckets[1].entry_count = 1;
  assert (newtm_hash_count (table) == SIZE_MAX); /* Exact boundary. */
  table->buckets[1].entry_count = 2;
  table->buckets[2].entry_count = 7;
  assert (newtm_hash_count (table) == SIZE_MAX); /* Early saturation. */
  for (size_t i = 0; i < 3; ++i)
    {
      /* Also check buckets after the saturation point were unlocked. */
      gomp_mutex_lock (&table->buckets[i].mutex);
      table->buckets[i].entry_count = 0;
      gomp_mutex_unlock (&table->buckets[i].mutex);
    }
  table->buckets[2].entry_count = SIZE_MAX;
  assert (newtm_hash_count (table) == SIZE_MAX);
  table->buckets[2].entry_count = 0;
  assert (newtm_hash_count (table) == 0);
  assert (newtm_hash_destroy (table) == NEWTM_HASH_OK);
}

enum { WORKERS = 4, ITERATIONS = 32 };
static struct newtm_hash_table *shared_table;
static char distinct[WORKERS], common;
static pthread_barrier_t barrier;

static void
wait_barrier (void)
{
  int status = pthread_barrier_wait (&barrier);
  assert (status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void *
worker (void *argument)
{
  size_t id = (size_t) argument;
  struct newtm_hash_entry *a, *b;
  assert (newtm_hash_acquire (shared_table, &common, 1, &a) == NEWTM_HASH_OK);
  assert (newtm_hash_acquire (shared_table, &distinct[id], 1, &b) == NEWTM_HASH_OK);
  wait_barrier ();
  wait_barrier ();
  newtm_hash_release (shared_table, a);
  newtm_hash_release (shared_table, b);
  for (unsigned int i = 0; i < ITERATIONS; ++i)
    {
      assert (newtm_hash_acquire (shared_table, &common, 1, &a) == NEWTM_HASH_OK);
      assert (newtm_hash_acquire (shared_table, &distinct[id], 1, &b)
	      == NEWTM_HASH_OK);
      size_t count = newtm_hash_count (shared_table);
      assert (count >= 4 && count <= WORKERS + 3);
      newtm_hash_release (shared_table, a);
      newtm_hash_release (shared_table, b);
    }
  return NULL;
}

static void
check_concurrent_counts (size_t capacity)
{
  pthread_t threads[WORKERS];
  char objects[2];
  struct newtm_hash_entry *a, *b;
  assert (newtm_hash_create (capacity, NULL, &shared_table) == NEWTM_HASH_OK);
  assert (newtm_hash_acquire (shared_table, &objects[0], 1, &a) == NEWTM_HASH_OK);
  assert (newtm_hash_acquire (shared_table, &objects[1], 1, &b) == NEWTM_HASH_OK);
  assert (pthread_barrier_init (&barrier, NULL, WORKERS + 1) == 0);
  for (size_t i = 0; i < WORKERS; ++i)
    assert (pthread_create (&threads[i], NULL, worker, (void *) i) == 0);
  wait_barrier ();
  assert (newtm_hash_count (shared_table) == WORKERS + 3);
  wait_barrier ();
  for (unsigned int i = 0; i < ITERATIONS; ++i)
    {
      size_t count = newtm_hash_count (shared_table);
      assert (count >= 2 && count <= WORKERS + 3);
    }
  for (unsigned int i = 0; i < WORKERS; ++i)
    assert (pthread_join (threads[i], NULL) == 0);
  assert (pthread_barrier_destroy (&barrier) == 0);
  assert (newtm_hash_count (shared_table) == 2);
  newtm_hash_release (shared_table, a);
  newtm_hash_release (shared_table, b);
  assert (newtm_hash_count (shared_table) == 0);
  assert (newtm_hash_destroy (shared_table) == NEWTM_HASH_OK);
}

/* Deterministically show zero does not imply the last release has returned.
   This destructor's test-only gate waits for main, not for another transaction;
   neither thread calls destroy while release is in progress. */
static pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cond = PTHREAD_COND_INITIALIZER;
static bool destructor_entered, finish_destructor, release_done;
static struct newtm_hash_entry *retiring;

static void
gated_destructor (void *data)
{
  assert (pthread_mutex_lock (&gate_mutex) == 0);
  destructor_entered = true;
  assert (pthread_cond_signal (&gate_cond) == 0);
  while (!finish_destructor)
    assert (pthread_cond_wait (&gate_cond, &gate_mutex) == 0);
  assert (pthread_mutex_unlock (&gate_mutex) == 0);
  free (data);
}

static void
install_metadata (const void *address, size_t size, void **slot, void *argument)
{
  assert (address != NULL && size != 0 && *slot == NULL);
  *slot = argument;
}

static void *
release_worker (void *argument)
{
  (void) argument;
  newtm_hash_release (shared_table, retiring);
  assert (pthread_mutex_lock (&gate_mutex) == 0);
  release_done = true;
  assert (pthread_mutex_unlock (&gate_mutex) == 0);
  return NULL;
}

static void
check_zero_during_cleanup (void)
{
  int object;
  pthread_t thread;
  assert (newtm_hash_create (3, gated_destructor, &shared_table) == NEWTM_HASH_OK);
  assert (newtm_hash_acquire (shared_table, &object, sizeof (object), &retiring)
	  == NEWTM_HASH_OK);
  void *data = malloc (1);
  assert (data != NULL);
  newtm_hash_entry_visit (shared_table, retiring, install_metadata, data);
  assert (pthread_create (&thread, NULL, release_worker, NULL) == 0);
  assert (pthread_mutex_lock (&gate_mutex) == 0);
  while (!destructor_entered)
    assert (pthread_cond_wait (&gate_cond, &gate_mutex) == 0);
  assert (pthread_mutex_unlock (&gate_mutex) == 0);
  assert (newtm_hash_count (shared_table) == 0);
  assert (pthread_mutex_lock (&gate_mutex) == 0);
  assert (!release_done);
  finish_destructor = true;
  assert (pthread_cond_signal (&gate_cond) == 0);
  assert (pthread_mutex_unlock (&gate_mutex) == 0);
  assert (pthread_join (thread, NULL) == 0);
  assert (release_done);
  assert (newtm_hash_destroy (shared_table) == NEWTM_HASH_OK);
  assert (pthread_cond_destroy (&gate_cond) == 0);
  assert (pthread_mutex_destroy (&gate_mutex) == 0);
}

int
main (void)
{
  check_counts (1);
  check_counts (3);
  check_counts (1024);
  check_saturation ();
  check_concurrent_counts (1);
  check_concurrent_counts (3);
  check_concurrent_counts (1024);
  check_zero_during_cleanup ();
  puts ("[teste] hash count OK");
  return 0;
}
