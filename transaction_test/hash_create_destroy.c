/* White-box test of table lifetime, independent of the acquisition path.
   Include the actual implementation to place a retained entry in a bucket
   and test BUSY without adding public test hooks or exporting hidden symbols.
   Link against the matching build's static libgomp for runtime support.  */
#include "../gcc-src-tm/libgomp/newtm-hash.c"

#undef NDEBUG
#include <assert.h>

static unsigned int destroyed_metadata;

static void
destroy_metadata (void *data)
{
  (void) data;
  ++destroyed_metadata;
}

static void
check_table (size_t capacity, newtm_hash_policy_destroy_fn destroy_policy)
{
  struct newtm_hash_table *table = NULL;
  assert (newtm_hash_create (capacity, destroy_policy, &table) == NEWTM_HASH_OK);
  assert (table != NULL);
  assert (table->bucket_count == capacity);
  assert (table->destroy_policy == destroy_policy);
  struct newtm_hash_bucket *buckets = table->buckets;
  assert (buckets != NULL);
  for (size_t i = 0; i < capacity; ++i)
    {
      assert (buckets[i].entries == NULL);
      assert (buckets[i].entry_count == 0);
      gomp_mutex_lock (&buckets[i].mutex);
      gomp_mutex_unlock (&buckets[i].mutex);
    }

  /* No concurrent users. A retained, idle entry is legal for destroy, but
     must cause BUSY. Put it last to catch partial teardown of earlier buckets.
     This is test setup, not an implementation of acquire/release.  */
  int object = 42;
  struct newtm_hash_entry entry = {
    .address = &object, .size = sizeof (object), .references = 1,
    .bucket_index = capacity - 1,
    .policy_data = destroy_policy != NULL ? &object : NULL, .next = NULL
  };
  buckets[capacity - 1].entries = &entry;
  buckets[capacity - 1].entry_count = 1;
  for (unsigned int attempt = 0; attempt < 2; ++attempt)
    {
      assert (newtm_hash_destroy (table) == NEWTM_HASH_BUSY);
      assert (table->buckets == buckets);
      assert (table->bucket_count == capacity);
      assert (table->destroy_policy == destroy_policy);
      assert (entry.references == 1 && entry.address == &object);
      assert (entry.size == sizeof (object));
      assert (entry.policy_data == (destroy_policy != NULL ? &object : NULL));
      assert (entry.bucket_index == capacity - 1 && entry.next == NULL);
      assert (object == 42 && destroyed_metadata == 0);
      for (size_t i = 0; i < capacity; ++i)
        {
          assert (buckets[i].entries == (i == capacity - 1 ? &entry : NULL));
          assert (buckets[i].entry_count == (i == capacity - 1 ? 1 : 0));
          gomp_mutex_lock (&buckets[i].mutex);
          gomp_mutex_unlock (&buckets[i].mutex);
        }
    }

  buckets[capacity - 1].entries = NULL;
  buckets[capacity - 1].entry_count = 0;
  assert (newtm_hash_destroy (table) == NEWTM_HASH_OK);
  assert (destroyed_metadata == 0 && object == 42);
}

int
main (void)
{
  struct newtm_hash_table *out = (struct newtm_hash_table *) (uintptr_t) 1;
  assert (newtm_hash_create (0, NULL, &out) == NEWTM_HASH_INVALID);
  assert (out == NULL);
  assert (newtm_hash_create (1, NULL, NULL) == NEWTM_HASH_INVALID);
  assert (newtm_hash_destroy (NULL) == NEWTM_HASH_INVALID);

  out = (struct newtm_hash_table *) (uintptr_t) 1;
  size_t overflowing = SIZE_MAX / sizeof (struct newtm_hash_bucket) + 1;
  assert (newtm_hash_create (overflowing, NULL, &out) == NEWTM_HASH_OVERFLOW);
  assert (out == NULL);

  struct newtm_hash_table *first = NULL;
  struct newtm_hash_table *second = NULL;
  assert (newtm_hash_create (1, NULL, &first) == NEWTM_HASH_OK);
  assert (newtm_hash_create (3, destroy_metadata, &second) == NEWTM_HASH_OK);
  assert (first != second && first->buckets != second->buckets);
  assert (newtm_hash_destroy (first) == NEWTM_HASH_OK);
  assert (newtm_hash_destroy (second) == NEWTM_HASH_OK);

  check_table (1, NULL);
  check_table (3, destroy_metadata);
  check_table (NEWTM_HASH_DEFAULT_BUCKETS, destroy_metadata);
  puts ("[teste] hash create/destroy OK");
  return 0;
}
