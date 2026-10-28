/* Exercise the real implementation without exporting hidden interfaces.
   White-box access is restricted to assertions, overflow injection and checking
   that metadata destruction occurs outside the bucket lock.  */
#include "../gcc-src-tm/libgomp/newtm-hash.c"

#undef NDEBUG
#include <assert.h>
#include <pthread.h>

static struct newtm_hash_table *metadata_table;
static unsigned int metadata_destroyed;

static void
check_mixing (void)
{
  const size_t capacities[] = { 1, 3, 1009, 1024, 1536 };
  const uintptr_t strides[] = { 1, 2, 8, 64, 4096, 65536 };
  const uintptr_t bases[] = { 0x10000, (UINTPTR_MAX >> 2) + 0x10000 };
  enum { SAMPLES = 32768 };
  /* Synthetic integer addresses test only the mixer; never dereference or
     register them as program objects. Exercise the native uintptr_t width. */
  for (size_t c = 0; c < sizeof (capacities) / sizeof (capacities[0]); ++c)
    for (size_t b = 0; b < sizeof (bases) / sizeof (bases[0]); ++b)
      for (size_t s = 0; s < sizeof (strides) / sizeof (strides[0]); ++s)
	{
	  size_t capacity = capacities[c];
	  size_t *counts = calloc (capacity, sizeof (*counts));
	  assert (counts != NULL);
	  for (uintptr_t i = 0; i < SAMPLES; ++i)
	    {
	      uintptr_t address = bases[b] + i * strides[s];
	      size_t index = newtm_hash_bucket_index ((const void *) address,
						     capacity);
	      assert (index < capacity);
	      ++counts[index];
	    }
	  for (size_t i = 0; i < capacity; ++i)
	    {
	      assert (counts[i] > 0);
	      /* Broad concentration check, not a demand for perfect uniformity. */
	      assert (counts[i] <= 4 * ((SAMPLES + capacity - 1) / capacity));
	    }
	  free (counts);
	}
}

static void
destroy_metadata (void *data)
{
  /* Direct lock check, not reentry through a hash operation. */
  gomp_mutex_lock (&metadata_table->buckets[0].mutex);
  gomp_mutex_unlock (&metadata_table->buckets[0].mutex);
  ++metadata_destroyed;
  free (data);
}

static void
check_basic (size_t capacity)
{
  struct newtm_hash_table *table = NULL;
  struct newtm_hash_entry *entry = NULL, *again = NULL, *other = NULL;
  char objects[2] = { 17, 29 };
  assert (newtm_hash_create (capacity, NULL, &table) == NEWTM_HASH_OK);
  assert (newtm_hash_lookup_acquire (table, &objects[0], &entry)
	  == NEWTM_HASH_NOT_FOUND);
  assert (entry == NULL);
  for (size_t i = 0; i < capacity; ++i)
    assert (table->buckets[i].entry_count == 0);

  assert (newtm_hash_acquire (table, &objects[0], 1, &entry) == NEWTM_HASH_OK);
  assert (entry->address == &objects[0] && entry->size == 1);
  assert (entry->references == 1 && entry->policy_data == NULL);
  assert (entry->bucket_index < capacity);
  assert (newtm_hash_acquire (table, &objects[0], 1, &again) == NEWTM_HASH_OK);
  assert (again == entry && entry->references == 2);
  assert (newtm_hash_lookup_acquire (table, &objects[0], &again) == NEWTM_HASH_OK);
  assert (again == entry && entry->references == 3);
  again = entry;
  assert (newtm_hash_acquire (table, &objects[0], 2, &again)
	  == NEWTM_HASH_SIZE_MISMATCH);
  assert (again == NULL && entry->references == 3 && entry->size == 1);
  assert (newtm_hash_acquire (table, &objects[1], 1, &other) == NEWTM_HASH_OK);
  assert (other != entry && other->address == &objects[1]);
  assert (newtm_hash_destroy (table) == NEWTM_HASH_BUSY);

  newtm_hash_release (table, entry);
  newtm_hash_release (table, entry);
  assert (entry->references == 1); /* lookup's reference still protects it. */
  newtm_hash_release (table, entry);
  assert (newtm_hash_lookup_acquire (table, &objects[0], &again)
	  == NEWTM_HASH_NOT_FOUND);
  assert (again == NULL);
  newtm_hash_release (table, other);
  assert (objects[0] == 17 && objects[1] == 29);
  assert (newtm_hash_destroy (table) == NEWTM_HASH_OK);
}

static void
check_failures (void)
{
  struct newtm_hash_table *table = NULL;
  struct newtm_hash_entry *entry = NULL, *out = NULL;
  int objects[2] = { 1, 2 };
  assert (newtm_hash_create (1, NULL, &table) == NEWTM_HASH_OK);
#define FAIL_ACQUIRE(T, A, S, EXPECTED) \
  do { \
    out = (struct newtm_hash_entry *) (uintptr_t) 1; \
    assert (newtm_hash_acquire ((T), (A), (S), &out) == (EXPECTED)); \
    assert (out == NULL); \
  } while (0)
#define FAIL_LOOKUP(T, A, EXPECTED) \
  do { \
    out = (struct newtm_hash_entry *) (uintptr_t) 1; \
    assert (newtm_hash_lookup_acquire ((T), (A), &out) == (EXPECTED)); \
    assert (out == NULL); \
  } while (0)
  FAIL_ACQUIRE (NULL, objects, sizeof (int), NEWTM_HASH_INVALID);
  FAIL_ACQUIRE (table, NULL, sizeof (int), NEWTM_HASH_INVALID);
  FAIL_ACQUIRE (table, objects, 0, NEWTM_HASH_INVALID);
  assert (newtm_hash_acquire (table, objects, sizeof (int), NULL)
	  == NEWTM_HASH_INVALID);
  FAIL_LOOKUP (NULL, objects, NEWTM_HASH_INVALID);
  FAIL_LOOKUP (table, NULL, NEWTM_HASH_INVALID);
  assert (newtm_hash_lookup_acquire (table, objects, NULL) == NEWTM_HASH_INVALID);
  assert (table->buckets[0].entries == NULL);
  assert (table->buckets[0].entry_count == 0);

  assert (newtm_hash_acquire (table, objects, sizeof (int), &entry)
	  == NEWTM_HASH_OK);
  /* Controlled overflow injection; production state is restored before use. */
  entry->references = SIZE_MAX;
  FAIL_ACQUIRE (table, objects, sizeof (int), NEWTM_HASH_OVERFLOW);
  FAIL_LOOKUP (table, objects, NEWTM_HASH_OVERFLOW);
  FAIL_ACQUIRE (table, objects, sizeof (objects), NEWTM_HASH_SIZE_MISMATCH);
  assert (entry->references == SIZE_MAX && entry->next == NULL);
  entry->references = 1;
  table->buckets[0].entry_count = SIZE_MAX;
  FAIL_ACQUIRE (table, &objects[1], sizeof (int), NEWTM_HASH_OVERFLOW);
  assert (table->buckets[0].entries == entry && entry->next == NULL);
  assert (entry->references == 1 && table->buckets[0].entry_count == SIZE_MAX);
  table->buckets[0].entry_count = 1;
  newtm_hash_release (table, entry);
  assert (newtm_hash_destroy (table) == NEWTM_HASH_OK);
#undef FAIL_ACQUIRE
#undef FAIL_LOOKUP
}

static void
install_metadata (const void *address, size_t size, void **slot, void *argument)
{
  assert (address != NULL && size != 0 && *slot == NULL && argument != NULL);
  *slot = argument;
}

static void
check_metadata (void)
{
  int object = 123;
  struct newtm_hash_entry *entry, *again;
  assert (newtm_hash_create (1, destroy_metadata, &metadata_table)
	  == NEWTM_HASH_OK);
  assert (newtm_hash_acquire (metadata_table, &object, sizeof (object), &entry)
	  == NEWTM_HASH_OK);
  void *data = malloc (8);
  assert (data != NULL);
  newtm_hash_entry_visit (metadata_table, entry, install_metadata, data);
  assert (newtm_hash_lookup_acquire (metadata_table, &object, &again)
	  == NEWTM_HASH_OK);
  newtm_hash_release (metadata_table, entry);
  assert (metadata_destroyed == 0);
  newtm_hash_release (metadata_table, again);
  assert (metadata_destroyed == 1 && object == 123);
  assert (newtm_hash_destroy (metadata_table) == NEWTM_HASH_OK);
  metadata_table = NULL;
}

enum { WORKERS = 4 };
static pthread_barrier_t barrier;
static struct newtm_hash_table *shared_table;
static int shared_object;
static char distinct_objects[WORKERS];
static struct newtm_hash_entry *held[WORKERS];

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
  struct newtm_hash_entry *lookup, *distinct;
  wait_barrier ();
  assert (newtm_hash_acquire (shared_table, &shared_object,
			    sizeof (shared_object), &held[id]) == NEWTM_HASH_OK);
  assert (newtm_hash_lookup_acquire (shared_table, &shared_object, &lookup)
	  == NEWTM_HASH_OK);
  assert (lookup == held[id]);
  assert (newtm_hash_acquire (shared_table, &distinct_objects[id], 1, &distinct)
	  == NEWTM_HASH_OK);
  wait_barrier ();
  /* Main inspects retained entries while all workers wait at the next barrier. */
  wait_barrier ();
  newtm_hash_release (shared_table, lookup);
  newtm_hash_release (shared_table, held[id]);
  newtm_hash_release (shared_table, distinct);
  return NULL;
}

static void
check_concurrency (size_t capacity)
{
  pthread_t threads[WORKERS];
  assert (newtm_hash_create (capacity, NULL, &shared_table) == NEWTM_HASH_OK);
  assert (pthread_barrier_init (&barrier, NULL, WORKERS + 1) == 0);
  for (size_t i = 0; i < WORKERS; ++i)
    assert (pthread_create (&threads[i], NULL, worker, (void *) i) == 0);
  wait_barrier ();
  wait_barrier ();
  for (size_t i = 1; i < WORKERS; ++i)
    assert (held[i] == held[0]);
  assert (held[0]->references == 2 * WORKERS);
  size_t entries = 0;
  for (size_t i = 0; i < capacity; ++i)
    entries += shared_table->buckets[i].entry_count;
  assert (entries == WORKERS + 1);
  wait_barrier ();
  for (size_t i = 0; i < WORKERS; ++i)
    assert (pthread_join (threads[i], NULL) == 0);
  assert (pthread_barrier_destroy (&barrier) == 0);
  assert (newtm_hash_destroy (shared_table) == NEWTM_HASH_OK);
  shared_table = NULL;
}

int
main (void)
{
  check_mixing ();
  check_basic (1); /* Force collision chaining. */
  check_basic (3);
  check_basic (1024);
  check_failures ();
  check_metadata ();
  check_concurrency (1);
  check_concurrency (3);
  check_concurrency (1024);
  puts ("[teste] hash acquire/lookup/release OK");
  return 0;
}
