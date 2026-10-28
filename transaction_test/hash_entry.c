/* Test the real implementation and its private mutex invariants without
   exporting interfaces or adding production hooks. Linux x86_64 build. */
#include "../gcc-src-tm/libgomp/newtm-hash.c"

#undef NDEBUG
#include <assert.h>
#include <pthread.h>

struct visit_arguments
{
  struct newtm_hash_table *table;
  struct newtm_hash_entry *entry;
  const void *address;
  size_t size;
  size_t calls;
  size_t value;
  void *replacement;
  void *previous;
};

static unsigned int destroyed;

static void
destroy_metadata (void *data)
{
  ++destroyed;
  free (data);
}

static void
check_callback (const void *address, size_t size, void **slot,
		void *argument)
{
  struct visit_arguments *args = argument;
  assert (address == args->address && size == args->size);
  assert (slot == &args->entry->policy_data);
  /* This test uses the Linux integer/futex mutex backend. Inspection is
     test-only: callbacks never reenter a hash primitive to check the lock. */
  assert (__atomic_load_n (&args->table->buckets[args->entry->bucket_index].mutex,
			  __ATOMIC_RELAXED) != 0);
  ++args->calls;
  args->previous = *slot;
  *slot = args->replacement;
}

static void
null_argument_callback (const void *address, size_t size, void **slot,
			void *argument)
{
  assert (address != NULL && size != 0 && slot != NULL && argument == NULL);
  assert (*slot != NULL);
  ++*(size_t *) *slot;
}

static void
check_queries_and_visits (void)
{
  struct { char bytes[17]; long value; } object = { { 1 }, 123 };
  struct newtm_hash_table *table;
  struct newtm_hash_entry *entry, *lookup;
  assert (newtm_hash_create (3, destroy_metadata, &table) == NEWTM_HASH_OK);
  assert (newtm_hash_acquire (table, &object, sizeof (object), &entry)
	  == NEWTM_HASH_OK);
  assert (newtm_hash_entry_address (entry) == &object);
  assert (newtm_hash_entry_size (entry) == sizeof (object));
  assert (entry->references == 1 && entry->policy_data == NULL);

  size_t *first = malloc (sizeof (*first));
  size_t *second = malloc (sizeof (*second));
  assert (first != NULL && second != NULL);
  *first = 10;
  *second = 20;
  struct visit_arguments args = {
    .table = table, .entry = entry, .address = &object, .size = sizeof (object),
    .replacement = first
  };
  newtm_hash_entry_visit (table, entry, check_callback, &args);
  assert (args.calls == 1 && args.previous == NULL && entry->references == 1);
  args.replacement = second;
  newtm_hash_entry_visit (table, entry, check_callback, &args);
  assert (args.calls == 2 && args.previous == first && *first == 10);
  /* The callback transfers old metadata to the caller; the test has no
     concurrent users of that old allocation and frees it outside the lock. */
  free (args.previous);
  newtm_hash_entry_visit (table, entry, null_argument_callback, NULL);
  assert (*second == 21 && entry->references == 1);

  assert (newtm_hash_lookup_acquire (table, &object, &lookup) == NEWTM_HASH_OK);
  newtm_hash_release (table, entry);
  /* Lookup's independent reference still permits both immutable queries. */
  assert (newtm_hash_entry_address (lookup) == &object);
  assert (newtm_hash_entry_size (lookup) == sizeof (object));
  assert (lookup->references == 1 && destroyed == 0);
  assert (object.bytes[0] == 1 && object.value == 123);
  newtm_hash_release (table, lookup);
  assert (destroyed == 1);
  assert (newtm_hash_destroy (table) == NEWTM_HASH_OK);
}

enum { WORKERS = 4, ITERATIONS = 64 };
static pthread_barrier_t barrier;
static struct newtm_hash_table *shared_table;
static struct newtm_hash_entry *shared_entry;
static int shared_object;

static void
wait_barrier (void)
{
  int status = pthread_barrier_wait (&barrier);
  assert (status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void *
visit_worker (void *argument)
{
  (void) argument;
  struct newtm_hash_entry *entry;
  assert (newtm_hash_lookup_acquire (shared_table, &shared_object, &entry)
	  == NEWTM_HASH_OK);
  wait_barrier ();
  for (unsigned int i = 0; i < ITERATIONS; ++i)
    {
      assert (newtm_hash_entry_address (entry) == &shared_object);
      assert (newtm_hash_entry_size (entry) == sizeof (shared_object));
      newtm_hash_entry_visit (shared_table, entry, null_argument_callback, NULL);
    }
  newtm_hash_release (shared_table, entry);
  return NULL;
}

static void
check_concurrent_visits (void)
{
  pthread_t threads[WORKERS];
  assert (newtm_hash_create (1, destroy_metadata, &shared_table) == NEWTM_HASH_OK);
  assert (newtm_hash_acquire (shared_table, &shared_object, sizeof (shared_object),
			    &shared_entry) == NEWTM_HASH_OK);
  size_t *value = calloc (1, sizeof (*value));
  assert (value != NULL);
  struct visit_arguments args = {
    .table = shared_table, .entry = shared_entry, .address = &shared_object,
    .size = sizeof (shared_object), .replacement = value
  };
  newtm_hash_entry_visit (shared_table, shared_entry, check_callback, &args);
  assert (pthread_barrier_init (&barrier, NULL, WORKERS + 1) == 0);
  for (unsigned int i = 0; i < WORKERS; ++i)
    assert (pthread_create (&threads[i], NULL, visit_worker, NULL) == 0);
  wait_barrier ();
  for (unsigned int i = 0; i < WORKERS; ++i)
    assert (pthread_join (threads[i], NULL) == 0);
  assert (pthread_barrier_destroy (&barrier) == 0);
  /* All workers are quiescent; the main reference keeps metadata alive. */
  assert (*value == WORKERS * ITERATIONS && shared_entry->references == 1);
  newtm_hash_release (shared_table, shared_entry);
  assert (destroyed == 2);
  assert (newtm_hash_destroy (shared_table) == NEWTM_HASH_OK);
}

static void *
independent_bucket_worker (void *argument)
{
  struct visit_arguments *args = argument;
  newtm_hash_entry_visit (args->table, args->entry, check_callback, args);
  return NULL;
}

static void
check_independent_buckets (void)
{
  char objects[256];
  struct newtm_hash_table *table;
  struct newtm_hash_entry *first, *second;
  assert (newtm_hash_create (3, NULL, &table) == NEWTM_HASH_OK);
  assert (newtm_hash_acquire (table, &objects[0], 1, &first) == NEWTM_HASH_OK);
  size_t i;
  for (i = 1; i < sizeof (objects); ++i)
    if (newtm_hash_bucket_index (&objects[i], 3) != first->bucket_index)
      break;
  assert (i < sizeof (objects));
  assert (newtm_hash_acquire (table, &objects[i], 1, &second) == NEWTM_HASH_OK);
  struct visit_arguments args = {
    .table = table, .entry = second, .address = &objects[i], .size = 1
  };
  pthread_t thread;
  /* Test-only external lock: a visit in another bucket must complete before
     we unlock. No callback waits or performs hash reentry. Target has timeout. */
  gomp_mutex_lock (&table->buckets[first->bucket_index].mutex);
  assert (pthread_create (&thread, NULL, independent_bucket_worker, &args) == 0);
  assert (pthread_join (thread, NULL) == 0);
  gomp_mutex_unlock (&table->buckets[first->bucket_index].mutex);
  assert (args.calls == 1 && args.previous == NULL);
  newtm_hash_release (table, first);
  newtm_hash_release (table, second);
  assert (newtm_hash_destroy (table) == NEWTM_HASH_OK);
}

int
main (void)
{
  check_queries_and_visits ();
  check_concurrent_visits ();
  check_independent_buckets ();
  puts ("[teste] hash entry address/size/visit OK");
  return 0;
}
