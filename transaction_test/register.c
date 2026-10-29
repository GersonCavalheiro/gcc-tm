/* Registration contracts against real hash/context code.  Private hash access
   is used only to inspect reference counts and inject restored overflow. */
#include "../gcc-src-tm/libgomp/newtm-hash.c"
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#undef NDEBUG
#include <assert.h>

static void
begin (struct newtm_context **slot)
{
  struct newtm_hash_owner_lease lease = { 0 };
  bool outermost = false;
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, slot, &outermost) == NEWTM_LIFECYCLE_OK);
  assert (outermost && !lease.active);
}

static void
empty_policy (const void *object, size_t size, void **data, void *arg)
{
  struct newtm_record *r = arg;
  assert (object == r->object && size == r->size && !*data);
}

static void
check_record (struct newtm_context *ctx, const void *object, size_t size,
              enum newtm_object_kind kind)
{
  struct newtm_record *r = ctx->records;
  assert (r && r->context == ctx && r->object == object);
  assert (r->size == size && r->kind == kind && r->entry);
  assert (r->reservation == NEWTM_RESERVATION_NONE && !r->reader_next);
  if (kind == NEWTM_OBJECT_SCALAR)
    assert (!r->value.scalar.bytes && !r->value.scalar.valid
            && !r->value.scalar.modified);
  else
    assert (!r->value.aggregate.bytes && !r->value.aggregate.valid
            && !r->value.aggregate.modified);
  assert (newtm_hash_entry_address (r->entry) == object);
  assert (newtm_hash_entry_size (r->entry) == size);
  newtm_hash_entry_visit (ctx->lease.table, r->entry, empty_policy, r);
}

static void *
foreign_register (void *arg)
{
  int value = 9;
  assert (newtm_register (arg, &value, sizeof value, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_INVALID);
  return NULL;
}

int
main (void)
{
  int value = 42, another = 17;
  struct newtm_context *ctx = NULL, *other = NULL;
  assert (newtm_register (NULL, &value, sizeof value, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_INVALID);
  begin (&ctx);
  struct newtm_hash_table *table = ctx->lease.table;
  pthread_t thread;
  assert (!pthread_create (&thread, NULL, foreign_register, ctx));
  assert (!pthread_join (thread, NULL));
  assert (ctx->state == NEWTM_ATTEMPT_ACTIVE && !ctx->records);

  /* An inaccessible live mapping detects any attempted read or write. */
  long pagesize = sysconf (_SC_PAGESIZE);
  assert (pagesize > 0);
  void *object = mmap (NULL, pagesize, PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert (object != MAP_FAILED);
  assert (newtm_register (ctx, object, pagesize, NEWTM_OBJECT_AGGREGATE)
          == NEWTM_LIFECYCLE_OK);
  check_record (ctx, object, pagesize, NEWTM_OBJECT_AGGREGATE);
  struct newtm_record *aggregate = ctx->records;
  assert (newtm_register (ctx, &value, sizeof value, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_OK);
  check_record (ctx, &value, sizeof value, NEWTM_OBJECT_SCALAR);
  struct newtm_record *scalar = ctx->records;
  assert (scalar->next == aggregate && !aggregate->next);
  assert (scalar->entry->references == 1);
  bool outer;
  assert (newtm_begin (NULL, &ctx, &outer) == NEWTM_LIFECYCLE_OK && !outer);
  for (int i = 0; i < 3; ++i)
    assert (newtm_register (ctx, &value, sizeof value, NEWTM_OBJECT_SCALAR)
            == NEWTM_LIFECYCLE_OK);
  assert (ctx->records == scalar && scalar->entry->references == 1);
  assert (ctx->depth == 2 && newtm_hash_count (table) == 2);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_OK);

  /* Distinct attempts share a hash entry, retaining independent references. */
  begin (&other);
  assert (newtm_register (other, &value, sizeof value, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_OK);
  assert (other->records != scalar && other->records->entry == scalar->entry);
  assert (scalar->entry->references == 2);
  assert (newtm_discard (&other) == NEWTM_LIFECYCLE_OK);
  assert (scalar->entry->references == 1);
  assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_OK);
  assert (!munmap (object, pagesize));

  /* Every malformed description preserves the existing list and references;
     first permanent failure survives subsequent calls. */
  for (int mode = 0; mode < 5; ++mode)
    {
      begin (&ctx);
      assert (newtm_register (ctx, &value, sizeof value, NEWTM_OBJECT_SCALAR)
              == NEWTM_LIFECYCLE_OK);
      scalar = ctx->records;
      const void *address = mode == 0 ? NULL : &value;
      size_t size = mode == 1 ? 0 : sizeof value + (mode == 3);
      enum newtm_object_kind kind = mode == 2
        ? (enum newtm_object_kind) 99
        : mode == 4 ? NEWTM_OBJECT_AGGREGATE : NEWTM_OBJECT_SCALAR;
      assert (newtm_register (ctx, address, size, kind)
              == NEWTM_LIFECYCLE_INVALID);
      assert (ctx->state == NEWTM_ATTEMPT_INVALID && ctx->depth == 1);
      assert (ctx->records == scalar && !scalar->next);
      assert (scalar->entry->references == 1 && newtm_hash_count (table) == 1);
      enum newtm_failure_reason reason = mode >= 3
        ? NEWTM_REASON_INCOMPATIBLE_REGISTRATION : NEWTM_REASON_INVALID_OPERATION;
      assert (ctx->failure.reason == reason && ctx->failure.object == address
              && ctx->failure.size == size && !ctx->failure.offset);
      assert (newtm_register (ctx, &another, sizeof another, NEWTM_OBJECT_SCALAR)
              == NEWTM_LIFECYCLE_INVALID);
      assert (ctx->failure.reason == reason && ctx->failure.object == address);
      assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_OK);
    }

  /* Hash failures happen after candidate allocation: no partial record/reference
     may escape.  Restore injected counters before releasing the real entry. */
  for (int mode = 0; mode < 3; ++mode)
    {
      begin (&ctx);
      assert (newtm_register (ctx, &another, sizeof another, NEWTM_OBJECT_SCALAR)
              == NEWTM_LIFECYCLE_OK);
      scalar = ctx->records;
      struct newtm_hash_entry *held = NULL;
      assert (newtm_hash_acquire (table, &value, sizeof value, &held)
              == NEWTM_HASH_OK);
      struct newtm_hash_bucket *bucket = &table->buckets[held->bucket_index];
      size_t count = bucket->entry_count;
      if (mode == 1) held->references = SIZE_MAX;
      if (mode == 2) bucket->entry_count = SIZE_MAX;
      /* A missing key in the injected full bucket triggers entry overflow. */
      const void *address = &value;
      if (mode == 2)
        {
          uintptr_t candidate = (uintptr_t) &value + 1;
          while (newtm_hash_bucket_index ((const void *) candidate,
                                          table->bucket_count)
                 != held->bucket_index || candidate == (uintptr_t) &another)
            ++candidate;
          address = (const void *) candidate;
        }
      assert (newtm_register (ctx, address, sizeof value + (mode == 0),
                              NEWTM_OBJECT_SCALAR) == NEWTM_LIFECYCLE_INVALID);
      assert (ctx->records == scalar && !scalar->next && ctx->depth == 1);
      assert (held->references == (mode == 1 ? SIZE_MAX : 1));
      held->references = 1;
      bucket->entry_count = count;
      assert (newtm_hash_count (table) == 2);
      newtm_hash_release (table, held);
      assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_OK);
    }

  begin (&ctx);
  assert (newtm_fail (ctx, NEWTM_ACCESS_CONFLICT) == NEWTM_LIFECYCLE_CONFLICT);
  assert (newtm_register (ctx, &value, sizeof value, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_CONFLICT);
  assert (!ctx->records && ctx->depth == 1 && !newtm_hash_count (table));
  assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_OK);
  assert (value == 42 && another == 17);
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  puts ("[teste] registration OK: identity, deduplication, atomic failures, no values/reservations");
  return 0;
}
