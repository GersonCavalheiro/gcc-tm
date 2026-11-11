/* Aggregate call results, aliases and staged source data through the pragma. */
#include "newtm-abi.h"
#include <assert.h>
struct pair { int x, y; };
static struct pair source = { 1, 2 }, destination = { 3, 4 };
static int result = -91;
static unsigned writes, injected, cleanups;
uint32_t __real_GOMP_transaction_stm_v1_write (struct newtm_context *, const void *, size_t, const void *, size_t);
uint32_t __real_GOMP_transaction_stm_v1_end (newtm_slot_v1 *);
uint32_t __real_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *, size_t);
static void originals (void)
{ assert (source.x == 1 && source.y == 2 && destination.x == 3 && destination.y == 4 && result == -91); }
uint32_t __wrap_GOMP_transaction_stm_v1_write (struct newtm_context *ctx, const void *object, size_t offset, const void *in, size_t size)
{
  originals ();
  if (++writes == 2 && !injected) { injected = 1; return NEWTM_ABI_CONFLICT; }
  return __real_GOMP_transaction_stm_v1_write (ctx, object, offset, in, size);
}
uint32_t __wrap_GOMP_transaction_stm_v1_end (newtm_slot_v1 *slot)
{
  originals ();
  assert (cleanups == 1);
  return __real_GOMP_transaction_stm_v1_end (slot);
}
uint32_t __wrap_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *slot, size_t count)
{
  originals ();
  assert (count == ++cleanups && count == 1);
  uint32_t status = __real_GOMP_transaction_stm_v1_after_failure (slot, count);
  originals ();
  assert (status == NEWTM_ABI_CONFLICT && !*slot);
  return status;
}
static struct pair copy (const struct pair *p) { return *p; }
int main (void)
{
#pragma omp transaction(source, destination, result)
  {
    source.x = 9;
    destination = copy (&source);
    source.y = 7;
    destination.y += source.y;
    destination = copy (&destination);
    result = destination.x + destination.y;
  }
  assert (source.x == 9 && source.y == 7 && destination.x == 9 && destination.y == 9 && result == 18);
  return 0;
}
