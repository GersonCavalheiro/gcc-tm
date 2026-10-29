/* Failed-attempt epilogue contracts, without a compiler/GOMP retry bridge.
   Tests save no owning copies and never use a consumed context. */
#include "newtm-context.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>

static struct newtm_context *slot;
static int original = 17;
static unsigned int releases, lease_releases;
static bool fatal_expected;
static struct newtm_context **active_slot = &slot;

extern void __real_newtm_hash_release (struct newtm_hash_table *, struct newtm_hash_entry *);
void
__wrap_newtm_hash_release (struct newtm_hash_table *table, struct newtm_hash_entry *entry)
{
  assert (!*active_slot && original == 17);
  ++releases;
  __real_newtm_hash_release (table, entry);
}

extern enum newtm_hash_status __real_newtm_hash_owner_release (struct newtm_hash_owner_lease *);
enum newtm_hash_status
__wrap_newtm_hash_owner_release (struct newtm_hash_owner_lease *lease)
{
  assert (!*active_slot && original == 17 && releases == 1);
  assert (!newtm_hash_count (lease->table));
  ++lease_releases;
  return __real_newtm_hash_owner_release (lease);
}

extern void __real_gomp_fatal (const char *, ...) __attribute__ ((noreturn));
void __wrap_gomp_fatal (const char *, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
void
__wrap_gomp_fatal (const char *format, ...)
{
  assert (fatal_expected && !*active_slot && releases == 1 && lease_releases == 1);
  assert (original == 17);
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  puts ("[teste] fatal after complete cleanup; no retry");
  char message[512];
  va_list args;
  va_start (args, format);
  vsnprintf (message, sizeof message, format, args);
  va_end (args);
  __real_gomp_fatal ("%s", message);
}

static enum newtm_lifecycle_status
invalid_body (struct newtm_context *ctx, void *argument)
{
  static unsigned int calls;
  assert (++calls == 1); /* Permanent errors must never reinvoke this body. */
  active_slot = ctx->owning_slot; /* Test-only observation during live control. */
  assert (newtm_register (ctx, &original, sizeof original, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_OK);
  int value = 99;
  assert (newtm_write (ctx, &original, 0, &value, sizeof value) == NEWTM_ACCESS_OK);
  const char *mode = argument;
  if (strcmp (mode, "execute-unbalanced") == 0)
    {
      bool outer;
      assert (newtm_begin (NULL, ctx->owning_slot, &outer)
              == NEWTM_LIFECYCLE_OK && !outer);
      return NEWTM_LIFECYCLE_OK;
    }
  if (strcmp (mode, "execute-unknown") == 0)
    return (enum newtm_lifecycle_status) 99;
  int output = -1;
  assert (newtm_read (ctx, &original, SIZE_MAX, &output, sizeof output)
          == NEWTM_ACCESS_INVALID && output == -1);
  if (strcmp (mode, "execute-lies") == 0) return NEWTM_LIFECYCLE_OK;
  if (strcmp (mode, "execute-conflict-invalid") == 0) return NEWTM_LIFECYCLE_CONFLICT;
  return NEWTM_LIFECYCLE_INVALID;
}

int
main (int argc, char **argv)
{
  if (argc == 2 && strncmp (argv[1], "execute-", 8) == 0)
    {
      fatal_expected = true;
      newtm_execute (invalid_body, argv[1]);
      assert (0);
    }
  struct newtm_hash_owner_lease lease = { 0 };
  bool outer;
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_begin (&lease, &slot, &outer) == NEWTM_LIFECYCLE_OK && outer);
  assert (newtm_register (slot, &original, sizeof original, NEWTM_OBJECT_SCALAR)
          == NEWTM_LIFECYCLE_OK);
  int private_value = 99;
  assert (newtm_write (slot, &original, 0, &private_value, sizeof private_value)
          == NEWTM_ACCESS_OK);
  assert (newtm_begin (NULL, &slot, &outer) == NEWTM_LIFECYCLE_OK && !outer);
  if (argc == 2 && strcmp (argv[1], "invalid") == 0)
    {
      int output = -1;
      assert (newtm_read (slot, &original, SIZE_MAX, &output, sizeof output)
              == NEWTM_ACCESS_INVALID && output == -1);
      fatal_expected = true;
      newtm_after_failure (&slot, 1);
      assert (0);
    }
  assert (newtm_fail (slot, NEWTM_ACCESS_CONFLICT) == NEWTM_LIFECYCLE_CONFLICT);
  if (argc == 2 && strcmp (argv[1], "zero") == 0)
    {
      fatal_expected = true;
      newtm_after_failure (&slot, 0);
      assert (0);
    }
  if (argc == 2 && strcmp (argv[1], "invalid-dominates") == 0)
    {
      assert (newtm_fail (slot, NEWTM_ACCESS_INVALID) == NEWTM_LIFECYCLE_INVALID);
      assert (newtm_fail (slot, NEWTM_ACCESS_CONFLICT) == NEWTM_LIFECYCLE_INVALID);
      fatal_expected = true;
      newtm_after_failure (&slot, 1);
      assert (0);
    }
  assert (newtm_after_failure (&slot, 1) == NEWTM_LIFECYCLE_CONFLICT);
  assert (!slot && releases == 1 && lease_releases == 1 && original == 17);
  if (argc == 2 && strcmp (argv[1], "twice") == 0)
    {
      fatal_expected = true;
      newtm_after_failure (&slot, 2);
      assert (0);
    }
  const size_t attempts[] = { 0, 1, 2, 10, 11, 12, SIZE_MAX };
  for (size_t i = 0; i < sizeof attempts / sizeof attempts[0]; ++i)
    assert (newtm_backoff (attempts[i]) == (attempts[i]
            ? NEWTM_LIFECYCLE_OK : NEWTM_LIFECYCLE_INVALID));
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  puts ("[teste] failure epilogue OK: one discard, bounded backoff, unpublished values");
  return 0;
}
