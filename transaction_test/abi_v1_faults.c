/* Static-link fault injection at the adapter boundary; no runtime hooks. */
#include "newtm-abi.h"
#include "newtm-context.h"
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static int mode, releases;
static newtm_slot_v1 slot;
static int object = 7;
extern enum newtm_lifecycle_status __real_newtm_begin
  (struct newtm_hash_owner_lease *, struct newtm_context **, bool *);
enum newtm_lifecycle_status
__wrap_newtm_begin (struct newtm_hash_owner_lease *lease,
                    struct newtm_context **out, bool *outer)
{
  if (mode == 1 || mode == 2)
    {
      assert (lease && lease->active && !*out);
      *outer = false;
      return mode == 1 ? NEWTM_LIFECYCLE_INVALID : (enum newtm_lifecycle_status) 999;
    }
  return __real_newtm_begin (lease, out, outer);
}
extern enum newtm_hash_status __real_newtm_hash_owner_release
  (struct newtm_hash_owner_lease *);
enum newtm_hash_status
__wrap_newtm_hash_owner_release (struct newtm_hash_owner_lease *lease)
{
  assert (!slot);
  ++releases;
  return __real_newtm_hash_owner_release (lease);
}
extern enum newtm_access_status __real_newtm_read
  (struct newtm_context *, const void *, size_t, void *, size_t);
enum newtm_access_status
__wrap_newtm_read (struct newtm_context *ctx, const void *o, size_t off,
                   void *dest, size_t size)
{
  if (mode == 3) return (enum newtm_access_status) 999;
  return __real_newtm_read (ctx, o, off, dest, size);
}
void __wrap_gomp_fatal (const char *, ...) __attribute__((noreturn,format(printf,1,2)));
void
__wrap_gomp_fatal (const char *format, ...)
{
  assert (mode == 4 && !slot && releases == 1 && object == 7);
  assert (strstr (format, "invalid attempt"));
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  exit (0); /* Reached only after complete discard, never retry. */
}
int main (int argc, char **argv)
{
  uint32_t outer = 99;
  if (argc > 1)
    {
      mode = 4;
      assert (!strcmp (argv[1], "fatal"));
      assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == NEWTM_ABI_OK);
      assert (GOMP_transaction_stm_v1_register (slot, &object, sizeof object,
                                               NEWTM_ABI_SCALAR) == NEWTM_ABI_OK);
      int v = 19;
      assert (GOMP_transaction_stm_v1_write (slot, &object, 0, &v, sizeof v)
              == NEWTM_ABI_OK);
      assert (GOMP_transaction_stm_v1_fail (slot, 999) == NEWTM_ABI_INVALID);
      (void) GOMP_transaction_stm_v1_after_failure (&slot, 1);
      abort ();
    }
  for (mode = 1; mode <= 2; ++mode)
    {
      assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == NEWTM_ABI_INVALID);
      assert (!slot && !outer && releases == mode);
    }
  mode = 3;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == NEWTM_ABI_OK && outer);
  int result = -91;
  assert (GOMP_transaction_stm_v1_read (slot, &object, 0, &result, sizeof result)
          == NEWTM_ABI_INVALID && result == -91);
  assert (GOMP_transaction_stm_v1_fail (slot, 999) == NEWTM_ABI_INVALID);
  assert (GOMP_transaction_stm_v1_discard (&slot) == NEWTM_ABI_OK && !slot);
  assert (releases == 3);
  /* Closing admission proves there are no leaked leases from failed begins. */
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  outer = 99;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == NEWTM_ABI_INVALID);
  assert (!slot && !outer && releases == 3);
  return 0;
}
