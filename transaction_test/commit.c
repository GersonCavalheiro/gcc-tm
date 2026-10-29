/* External confirmation: actual publication, failure before the first copy,
   protected untouched ranges, all reservations retained during publication,
   and final cleanup.  Link wrapping adds no production hooks or public ABI. */
#include "newtm-context.h"
#undef NDEBUG
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static void
begin (struct newtm_context **ctx)
{
  struct newtm_hash_owner_lease lease = { 0 };
  bool outer;
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, ctx, &outer) == NEWTM_LIFECYCLE_OK && outer);
}

static void
reg (struct newtm_context *ctx, void *object, size_t size,
     enum newtm_object_kind kind)
{
  assert (newtm_register (ctx, object, size, kind) == NEWTM_LIFECYCLE_OK);
}

static void
put (struct newtm_context *ctx, void *object, size_t offset,
     const void *source, size_t size)
{
  assert (newtm_write (ctx, object, offset, source, size) == NEWTM_ACCESS_OK);
}

static void
normal_and_guarded (void)
{
  int scalar = 1, pending = 9, readback = 0;
  struct newtm_context *ctx = NULL;
  begin (&ctx);
  struct newtm_hash_owner_lease observer = { 0 };
  assert (newtm_hash_owner_acquire (&observer) == NEWTM_HASH_OK);
  reg (ctx, &scalar, sizeof scalar, NEWTM_OBJECT_SCALAR);
  put (ctx, &scalar, 0, &pending, sizeof pending);
  struct newtm_context *copy = ctx;
  assert (newtm_end (&copy) == NEWTM_LIFECYCLE_INVALID && copy == ctx && scalar == 1);
  bool outer;
  assert (newtm_begin (NULL, &ctx, &outer) == NEWTM_LIFECYCLE_OK && !outer);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_OK && ctx && scalar == 1);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_OK && !ctx && scalar == 9);
  assert (!newtm_hash_count (observer.table));
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_INVALID);
  assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_INVALID);
  begin (&ctx);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_OK && !ctx);
  /* Read-only confirmation releases only this reader; the survivor can promote. */
  struct newtm_context *other = NULL;
  begin (&ctx); begin (&other);
  reg (ctx, &scalar, sizeof scalar, NEWTM_OBJECT_SCALAR);
  reg (other, &scalar, sizeof scalar, NEWTM_OBJECT_SCALAR);
  assert (newtm_read (ctx, &scalar, 0, &readback, sizeof readback) == NEWTM_ACCESS_OK);
  assert (newtm_read (other, &scalar, 0, &readback, sizeof readback) == NEWTM_ACCESS_OK);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_OK && !ctx && scalar == 9);
  pending = 17;
  put (other, &scalar, 0, &pending, sizeof pending);
  assert (newtm_end (&other) == NEWTM_LIFECYCLE_OK && !other && scalar == 17);

  long page = sysconf (_SC_PAGESIZE);
  assert (page > 0);
  unsigned char *aggregate = mmap (NULL, 3 * page, PROT_NONE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  int *readonly = mmap (NULL, page, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert (aggregate != MAP_FAILED && readonly != MAP_FAILED);
  *readonly = 7;
  assert (!mprotect (readonly, page, PROT_READ));
  assert (!mprotect (aggregate + page, page, PROT_READ | PROT_WRITE));
  memset (aggregate + page, 0x33, page);
  begin (&ctx);
  reg (ctx, aggregate, 3 * page, NEWTM_OBJECT_AGGREGATE);
  reg (ctx, readonly, sizeof *readonly, NEWTM_OBJECT_SCALAR);
  assert (newtm_read (ctx, readonly, 0, &readback, sizeof readback)
          == NEWTM_ACCESS_OK && readback == 7);
  unsigned char source[4] = { 1, 2, 3, 4 };
  put (ctx, aggregate, page + 8, source, 4);
  put (ctx, aggregate, page + 20, source, 4);
  /* A valid but unmodified field must not be published. */
  unsigned char output[4];
  assert (newtm_read (ctx, aggregate, page + 12, output, 4) == NEWTM_ACCESS_OK);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_OK && !ctx);
  for (long i = 0; i < page; ++i)
    assert (aggregate[page + i] == (i >= 8 && i < 12 ? source[i - 8]
                                   : i >= 20 && i < 24 ? source[i - 20] : 0x33));
  assert (*readonly == 7);
  /* An object registered but never accessed need not be readable/writable. */
  begin (&ctx);
  reg (ctx, aggregate, page, NEWTM_OBJECT_AGGREGATE);
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_OK && !ctx);
  assert (!munmap (aggregate, 3 * page) && !munmap (readonly, page));
  assert (!newtm_hash_count (observer.table));
  assert (newtm_hash_owner_release (&observer) == NEWTM_HASH_OK);
}

static void
edit_writer (const void *object, size_t size, void **data, void *argument)
{
  (void) object; (void) size;
  struct newtm_record *record = argument;
  struct newtm_object_policy *policy = *data;
  assert (policy && !policy->readers);
  policy->writer = policy->writer ? NULL : record;
}

static void
validation_before_publication (void)
{
  for (int mode = 0; mode < 7; ++mode)
    {
      unsigned char bad[32] = { 0 }, good[32] = { 0 }, source[4] = { 1, 2, 3, 4 };
      struct newtm_context *ctx = NULL;
      begin (&ctx);
      reg (ctx, bad, sizeof bad, NEWTM_OBJECT_AGGREGATE);
      put (ctx, bad, 4, source, 4);
      struct newtm_record *record = ctx->records;
      reg (ctx, good, sizeof good, NEWTM_OBJECT_AGGREGATE);
      put (ctx, good, 0, source, 4);
      /* This earlier list entry is ready, but must not publish if a later one
         fails validation.  Restore private injections before real discard. */
      struct newtm_interval *modified = record->value.aggregate.modified;
      if (mode == 0) modified->end = 33;
      if (mode == 1) modified->begin = 3; /* Not contained in valid. */
      if (mode == 2) record->reservation = NEWTM_RESERVATION_READ;
      if (mode == 3) record->value.aggregate.modified = record->value.aggregate.valid;
      if (mode == 4) ctx->depth = 0;
      if (mode == 5)
        assert (newtm_fail (ctx, NEWTM_ACCESS_CONFLICT) == NEWTM_LIFECYCLE_CONFLICT);
      if (mode == 6)
        newtm_hash_entry_visit (ctx->lease.table, record->entry, edit_writer, record);
      assert (newtm_end (&ctx) == (mode == 5 ? NEWTM_LIFECYCLE_CONFLICT
                                  : NEWTM_LIFECYCLE_INVALID));
      assert (ctx && ctx->records->next == record);
      for (int i = 0; i < 32; ++i) assert (!bad[i] && !good[i]);
      assert (!memcmp (record->value.aggregate.bytes + 4, source, 4));
      modified->begin = 4; modified->end = 8;
      record->reservation = NEWTM_RESERVATION_WRITE;
      record->value.aggregate.modified = modified;
      if (mode == 4) ctx->depth = 1;
      if (mode == 6)
        newtm_hash_entry_visit (ctx->lease.table, record->entry, edit_writer, record);
      assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_OK && !ctx);
    }
}

/* Only the publishing thread enables these wrappers. */
static _Thread_local struct newtm_context **observed_slot;
static _Thread_local bool published;
static _Thread_local unsigned int copies, releases, lease_releases;
static int objects[2], replacement[2] = { 101, 202 };
static pthread_barrier_t barrier;

static void
wait_barrier (void)
{
  int result = pthread_barrier_wait (&barrier);
  assert (!result || result == PTHREAD_BARRIER_SERIAL_THREAD);
}

extern void *__real_memcpy (void *, const void *, size_t);
void *
__wrap_memcpy (void *destination, const void *source, size_t size)
{
  void *result = __real_memcpy (destination, source, size);
  if (observed_slot && (destination == &objects[0] || destination == &objects[1]))
    {
      assert (*observed_slot && (*observed_slot)->state == NEWTM_ATTEMPT_PUBLISHING);
      published = true;
      if (++copies == 1)
        { wait_barrier (); wait_barrier (); }
    }
  return result;
}

extern void *__real_gomp_malloc (size_t);
extern void *__real_gomp_malloc_cleared (size_t);
void *__wrap_gomp_malloc (size_t size)
{ assert (!published); return __real_gomp_malloc (size); }
void *__wrap_gomp_malloc_cleared (size_t size)
{ assert (!published); return __real_gomp_malloc_cleared (size); }

extern void __real_newtm_hash_release (struct newtm_hash_table *, struct newtm_hash_entry *);
void
__wrap_newtm_hash_release (struct newtm_hash_table *table, struct newtm_hash_entry *entry)
{
  if (observed_slot)
    {
      assert (!*observed_slot && copies == 2);
      assert (objects[0] == replacement[0] && objects[1] == replacement[1]);
      ++releases;
    }
  __real_newtm_hash_release (table, entry);
}

extern enum newtm_hash_status __real_newtm_hash_owner_release (struct newtm_hash_owner_lease *);
enum newtm_hash_status
__wrap_newtm_hash_owner_release (struct newtm_hash_owner_lease *lease)
{
  if (observed_slot)
    {
      assert (!*observed_slot && copies == 2 && releases == 2);
      assert (!newtm_hash_count (lease->table));
      ++lease_releases;
    }
  return __real_newtm_hash_owner_release (lease);
}

static void *
contender (void *unused)
{
  (void) unused;
  wait_barrier ();
  /* The first copy has happened; both objects must remain reserved. */
  for (int i = 0; i < 2; ++i)
    for (int write = 0; write < 2; ++write)
      {
        struct newtm_context *ctx = NULL;
        begin (&ctx);
        reg (ctx, &objects[i], sizeof objects[i], NEWTM_OBJECT_SCALAR);
        int output = -1;
        enum newtm_access_status status = write
          ? newtm_write (ctx, &objects[i], 0, &output, sizeof output)
          : newtm_read (ctx, &objects[i], 0, &output, sizeof output);
        assert (status == NEWTM_ACCESS_CONFLICT && output == -1);
        assert (newtm_discard (&ctx) == NEWTM_LIFECYCLE_OK);
      }
  wait_barrier ();
  return NULL;
}

static void
retained_reservations (void)
{
  struct newtm_context *ctx = NULL;
  begin (&ctx);
  for (int i = 0; i < 2; ++i)
    {
      reg (ctx, &objects[i], sizeof objects[i], NEWTM_OBJECT_SCALAR);
      put (ctx, &objects[i], 0, &replacement[i], sizeof replacement[i]);
    }
  assert (!pthread_barrier_init (&barrier, NULL, 2));
  pthread_t thread;
  assert (!pthread_create (&thread, NULL, contender, NULL));
  observed_slot = &ctx;
  assert (newtm_end (&ctx) == NEWTM_LIFECYCLE_OK && !ctx);
  observed_slot = NULL;
  published = false;
  assert (copies == 2 && releases == 2 && lease_releases == 1);
  assert (!pthread_join (thread, NULL));
  assert (!pthread_barrier_destroy (&barrier));
}

int
main (void)
{
  normal_and_guarded ();
  validation_before_publication ();
  retained_reservations ();
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  puts ("[teste] commit OK: preflight, modified ranges, retained reservations, complete cleanup");
  return 0;
}
