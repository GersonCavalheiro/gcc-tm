/* White-box archive test: generated fatal paths release actual runtime leases. */
#include "newtm-abi.h"
#include "newtm-context.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int first = 5, second = 7;
static unsigned mode, releases, begins, writes, cleanups;
static newtm_slot_v1 *owner;
uint32_t __real_GOMP_transaction_stm_v1_begin (newtm_slot_v1 *, uint32_t *);
uint32_t __wrap_GOMP_transaction_stm_v1_begin (newtm_slot_v1 *slot, uint32_t *outer)
{
  assert (++begins == 1 && !*slot);
  owner = slot;
  return __real_GOMP_transaction_stm_v1_begin (slot, outer);
}
enum newtm_lifecycle_status __real_newtm_begin
  (struct newtm_hash_owner_lease *, struct newtm_context **, bool *);
enum newtm_lifecycle_status __wrap_newtm_begin
  (struct newtm_hash_owner_lease *lease, struct newtm_context **slot, bool *outer)
{
  assert (lease && lease->active && !*slot);
  if (mode == 1) { *outer = false; return NEWTM_LIFECYCLE_INVALID; }
  return __real_newtm_begin (lease, slot, outer);
}
enum newtm_hash_status __real_newtm_hash_owner_release (struct newtm_hash_owner_lease *);
enum newtm_hash_status __wrap_newtm_hash_owner_release (struct newtm_hash_owner_lease *lease)
{
  assert (owner && !*owner && ++releases == 1);
  return __real_newtm_hash_owner_release (lease);
}
uint32_t __real_GOMP_transaction_stm_v1_write (struct newtm_context *, const void *, size_t, const void *, size_t);
uint32_t __wrap_GOMP_transaction_stm_v1_write (struct newtm_context *ctx, const void *object,
                                             size_t offset, const void *in, size_t size)
{
  if (++writes == 2) return NEWTM_ABI_INVALID;
  return __real_GOMP_transaction_stm_v1_write (ctx, object, offset, in, size);
}
uint32_t __real_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *, size_t);
uint32_t __wrap_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *slot, size_t count)
{
  assert (mode == 2 && ++cleanups == 1 && count == 1 && slot == owner && *slot);
  return __real_GOMP_transaction_stm_v1_after_failure (slot, count);
}
void __wrap_gomp_fatal (const char *, ...) __attribute__((noreturn,format(printf,1,2)));
void __wrap_gomp_fatal (const char *format, ...)
{
  assert (owner && !*owner && releases == 1 && begins == 1);
  assert (first == 5 && second == 7 && cleanups == (mode == 2));
  assert (strstr (format, mode == 1 ? "failed without context" : "invalid attempt"));
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  exit (0);
}
int main (int argc, char **argv)
{
  assert (argc == 2);
  mode = (unsigned)atoi (argv[1]);
#pragma omp transaction(first, second)
  { first = 8; second = 9; }
  abort ();
}
