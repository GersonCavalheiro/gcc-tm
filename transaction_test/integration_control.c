/* Branches and loops inside the attempt must replay with fresh private state. */
#include "newtm-abi.h"
#include <assert.h>
static int array[4] = { 1, 2, 3, 4 }, result = -91, sum;
static unsigned writes, cleanups;
#ifdef __cplusplus
extern "C" {
#endif
uint32_t __real_GOMP_transaction_stm_v1_write (struct newtm_context *, const void *, size_t, const void *, size_t);
uint32_t __real_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *, size_t);
static void originals (void)
{ assert (array[0] == 1 && array[1] == 2 && array[2] == 3 && array[3] == 4 && result == -91 && sum == 0); }
uint32_t __wrap_GOMP_transaction_stm_v1_write (struct newtm_context *ctx, const void *object, size_t offset, const void *in, size_t size)
{
  originals ();
  if (++writes == 2) return NEWTM_ABI_CONFLICT;
  return __real_GOMP_transaction_stm_v1_write (ctx, object, offset, in, size);
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
#ifdef __cplusplus
}
#endif
int main (void)
{
#pragma omp transaction(array, result, sum)
  {
    int scratch = 0;
    for (int i = 0; i < 4; ++i)
      {
        scratch += i;
        if (i & 1) array[i] = scratch;
        else array[i] += 2;
      }
    sum = scratch;
    result = array[0] + array[1] + array[2] + array[3];
  }
  assert (array[0] == 3 && array[1] == 1 && array[2] == 5 && array[3] == 6);
  assert (sum == 6 && result == 15 && cleanups == 1);
  return 0;
}
