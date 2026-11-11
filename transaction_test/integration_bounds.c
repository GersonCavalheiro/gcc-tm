/* A real bounds failure must discard a staged prefix before terminating. */
#include "newtm-abi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static int array[2] = { 5, 7 }, result = -91;
static unsigned cleanups;
#ifdef __cplusplus
extern "C" {
#endif
uint32_t __real_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *, size_t);
uint32_t __real_GOMP_transaction_stm_v1_read (struct newtm_context *, const void *, size_t, void *, size_t);
uint32_t __wrap_GOMP_transaction_stm_v1_read (struct newtm_context *ctx, const void *object, size_t offset, void *out, size_t size)
{
  /* The invalid array access must be refused before reaching the runtime. */
  if (object == array) assert (offset <= sizeof array && size <= sizeof array - offset);
  return __real_GOMP_transaction_stm_v1_read (ctx, object, offset, out, size);
}
uint32_t __wrap_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *slot, size_t count)
{
  assert (array[0] == 5 && array[1] == 7 && result == -91);
  assert (++cleanups == 1 && count == 1 && *slot);
  assert (GOMP_transaction_stm_v1_status (*slot) == NEWTM_ABI_INVALID);
  puts ("BOUNDS invalid before publication");
  return __real_GOMP_transaction_stm_v1_after_failure (slot, count);
}
#ifdef __cplusplus
}
#endif
int main (int argc, char **argv)
{
  assert (argc == 2);
  long long index = strtoll (argv[1], 0, 10);
#pragma omp transaction(array, result, index)
  { array[0] = 9; result = array[index]; }
  assert (!cleanups && array[0] == 9 && array[1] == 7);
  assert (index == 0 ? result == 9 : result == 7);
  return 0;
}
