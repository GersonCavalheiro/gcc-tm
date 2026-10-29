/* Read/write acceptance contracts: failures preserve caller buffers and prior
   private values.  Private state is used to check unpublished data, never to
   infer success from trace output. */
#include "newtm-context.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>

enum { EXTENT = 32 };

static void
begin (struct newtm_context **ctx, void *object)
{
  struct newtm_hash_owner_lease lease = { 0 };
  bool outer;
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, ctx, &outer) == NEWTM_LIFECYCLE_OK && outer);
  assert (newtm_register (*ctx, object, EXTENT, NEWTM_OBJECT_AGGREGATE)
          == NEWTM_LIFECYCLE_OK);
}

static void
discard (struct newtm_context **ctx)
{
  assert (newtm_discard (ctx) == NEWTM_LIFECYCLE_OK && !*ctx);
}

struct access_request
{
  struct newtm_context *ctx;
  const void *object;
  size_t offset, size;
  void *buffer;
  bool write;
  enum newtm_access_status status;
};

static void *
access_request (void *argument)
{
  struct access_request *r = argument;
  r->status = r->write
    ? newtm_write (r->ctx, r->object, r->offset, r->buffer, r->size)
    : newtm_read (r->ctx, r->object, r->offset, r->buffer, r->size);
  return NULL;
}

static void
invalid_accesses (void)
{
  for (int write = 0; write < 2; ++write)
    for (int mode = 0; mode < 20; ++mode)
      {
        unsigned char original[EXTENT], buffer[EXTENT], previous[EXTENT];
        unsigned char unknown[EXTENT];
        memset (original, 0x11, EXTENT);
        memset (buffer, 0xa5, EXTENT);
        struct newtm_context *ctx = NULL;
        begin (&ctx, original);
        /* Load all bytes, then retain a previous partial write. */
        assert (newtm_read (ctx, original, 0, previous, EXTENT) == NEWTM_ACCESS_OK);
        assert (newtm_write (ctx, original, 8, buffer, 8) == NEWTM_ACCESS_OK);
        memcpy (previous, ctx->records->value.aggregate.bytes, EXTENT);
        struct newtm_record *record = ctx->records;
        struct newtm_interval *valid = record->value.aggregate.valid;
        struct newtm_interval *modified = record->value.aggregate.modified;
        struct access_request r = { ctx, original, 3, 4, buffer, write,
                                    NEWTM_ACCESS_OK };
        enum newtm_access_status expected = NEWTM_ACCESS_INVALID;
        switch (mode)
          {
          case 0: r.ctx = NULL; break;
          case 1: break; /* Executed from another thread below. */
          case 2:
            assert (newtm_fail (ctx, NEWTM_ACCESS_CONFLICT)
                    == NEWTM_LIFECYCLE_CONFLICT);
            expected = NEWTM_ACCESS_CONFLICT;
            break;
          case 3:
            assert (newtm_fail (ctx, NEWTM_ACCESS_INVALID)
                    == NEWTM_LIFECYCLE_INVALID);
            break;
          case 4: ctx->state = NEWTM_ATTEMPT_PUBLISHING; break;
          case 5: ctx->state = NEWTM_ATTEMPT_CLOSED; break;
          case 6: ctx->depth = 0; break;
          case 7: r.object = NULL; break;
          case 8: r.object = unknown; break;
          case 9: r.size = 0; break;
          case 10: r.offset = EXTENT; break;
          case 11: r.offset = SIZE_MAX; break;
          case 12: r.size = SIZE_MAX; break;
          case 13: r.buffer = NULL; break;
          case 14: r.buffer = original + 2; break;
          case 15: r.buffer = record->value.aggregate.bytes + 2; break;
          case 16: r.buffer = ctx; break;
          case 17: r.buffer = record; break;
          case 18: r.buffer = &ctx; break;
          case 19: r.buffer = valid; break;
          }
        enum newtm_attempt_state state = ctx->state;
        struct newtm_failure_info failure = ctx->failure;
        if (mode == 1)
          {
            pthread_t thread;
            assert (!pthread_create (&thread, NULL, access_request, &r));
            assert (!pthread_join (thread, NULL));
          }
        else
          access_request (&r);
        assert (r.status == expected);
        for (int i = 0; i < EXTENT; ++i)
          assert (original[i] == 0x11 && buffer[i] == 0xa5);
        assert (ctx->records == record && !record->next);
        assert (record->reservation == NEWTM_RESERVATION_WRITE);
        assert (record->value.aggregate.valid == valid);
        assert (record->value.aggregate.modified == modified);
        assert (valid->begin == 0 && valid->end == EXTENT && !valid->next);
        assert (modified->begin == 8 && modified->end == 16 && !modified->next);
        assert (!memcmp (record->value.aggregate.bytes, previous, EXTENT));
        assert (ctx->depth == (mode == 6 ? 0 : 1));
        if (mode <= 5)
          {
            assert (ctx->state == state);
            assert (ctx->failure.reason == failure.reason
                    && ctx->failure.object == failure.object
                    && ctx->failure.offset == failure.offset
                    && ctx->failure.size == failure.size);
          }
        else
          {
            assert (ctx->state == NEWTM_ATTEMPT_INVALID);
            assert (ctx->failure.object == r.object
                    && ctx->failure.offset == r.offset
                    && ctx->failure.size == r.size);
          }
        /* Restore only artificial state/depth injections, so discard can check
           real reservation cleanup.  Real failures remain condemned. */
        if (mode == 4 || mode == 5) ctx->state = NEWTM_ATTEMPT_ACTIVE;
        if (mode == 6) ctx->depth = 1;
        discard (&ctx);
      }
}

static void
conflict_subranges (void)
{
  for (int write = 0; write < 2; ++write)
    {
      unsigned char original[EXTENT], side[EXTENT], buffer[EXTENT];
      memset (original, 0x11, EXTENT);
      memset (side, 0x22, EXTENT);
      memset (buffer, 0xa5, EXTENT);
      struct newtm_context *a = NULL, *b = NULL;
      begin (&a, side);
      assert (newtm_write (a, side, 5, buffer, 7) == NEWTM_ACCESS_OK);
      struct newtm_record *saved = a->records;
      assert (newtm_register (a, original, EXTENT, NEWTM_OBJECT_AGGREGATE)
              == NEWTM_LIFECYCLE_OK);
      struct newtm_record *target = a->records;
      begin (&b, original);
      assert (newtm_write (b, original, 0, buffer, 8) == NEWTM_ACCESS_OK);
      struct access_request r = { a, original, 3, 4, buffer, write,
                                  NEWTM_ACCESS_OK };
      access_request (&r);
      assert (r.status == NEWTM_ACCESS_CONFLICT);
      assert (a->state == NEWTM_ATTEMPT_CONFLICTED && a->depth == 1);
      assert (a->failure.reason == NEWTM_REASON_CONFLICT);
      assert (a->failure.object == original && a->failure.offset == 3
              && a->failure.size == 4);
      assert (!target->value.aggregate.bytes && !target->value.aggregate.valid
              && !target->value.aggregate.modified);
      assert (target->reservation == NEWTM_RESERVATION_NONE);
      assert (saved->reservation == NEWTM_RESERVATION_WRITE);
      for (int i = 5; i < 12; ++i)
        assert (saved->value.aggregate.bytes[i] == 0xa5);
      assert (b->state == NEWTM_ATTEMPT_ACTIVE);
      for (int i = 0; i < EXTENT; ++i)
        assert (original[i] == 0x11 && side[i] == 0x22 && buffer[i] == 0xa5);
      /* Further errors on the condemned attempt cannot replace the first cause
         or its subrange, nor consume a result. */
      assert (newtm_read (a, NULL, SIZE_MAX, buffer, SIZE_MAX)
              == NEWTM_ACCESS_CONFLICT);
      assert (a->failure.object == original && a->failure.offset == 3
              && a->failure.size == 4);
      discard (&a);
      unsigned char readback[8];
      assert (newtm_read (b, original, 0, readback, sizeof readback)
              == NEWTM_ACCESS_OK && !memcmp (readback, buffer, sizeof readback));
      discard (&b);
      for (int i = 0; i < EXTENT; ++i)
        assert (original[i] == 0x11 && side[i] == 0x22);
    }
}

int
main (void)
{
  invalid_accesses ();
  conflict_subranges ();
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  puts ("[teste] access OK: validation, reservations, unchanged outputs and original memory");
  return 0;
}
