/* Private-view reads cannot touch a PROT_NONE original after staged writes. */
#include "newtm-abi.h"
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
static int protected_object[1024] __attribute__((aligned(4096)));
static int result = -91;
static unsigned protected_now, attempts, cleanups, injected;
#ifdef __cplusplus
extern "C" {
#endif
uint32_t __real_GOMP_transaction_stm_v1_read (struct newtm_context *, const void *, size_t, void *, size_t);
uint32_t __real_GOMP_transaction_stm_v1_end (newtm_slot_v1 *);
uint32_t __real_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *, size_t);
uint32_t __wrap_GOMP_transaction_stm_v1_read (struct newtm_context *ctx, const void *object, size_t offset, void *out, size_t size)
{
  assert (protected_now && result == -91);
  if (object == protected_object && !injected)
    { injected = 1; return NEWTM_ABI_CONFLICT; }
  return __real_GOMP_transaction_stm_v1_read (ctx, object, offset, out, size);
}
uint32_t __wrap_GOMP_transaction_stm_v1_end (newtm_slot_v1 *slot)
{
  ++attempts;
  assert (protected_now && result == -91 && cleanups == 1);
  assert (mprotect (protected_object, sizeof protected_object, PROT_READ | PROT_WRITE) == 0);
  protected_now = 0;
  assert (protected_object[0] == 5);
  return __real_GOMP_transaction_stm_v1_end (slot);
}
uint32_t __wrap_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *slot, size_t count)
{
  assert (protected_now && count == ++cleanups && count == 1);
  uint32_t status = __real_GOMP_transaction_stm_v1_after_failure (slot, count);
  assert (status == NEWTM_ABI_CONFLICT && !*slot && protected_now && result == -91);
  return status;
}
#ifdef __cplusplus
}
#endif
static int overwrite (int *p) { *p = 17; return *p; }
int main (void)
{
  assert (sysconf (_SC_PAGESIZE) == 4096);
  protected_object[0] = 5;
  assert (mprotect (protected_object, sizeof protected_object, PROT_NONE) == 0);
  protected_now = 1;
#pragma omp transaction(protected_object, result)
  {
    int *p = protected_object;
    result = overwrite (p);
    protected_object[0] = result + 1;
  }
  assert (protected_object[0] == 18 && result == 17 && attempts == 1 && cleanups == 1);
  return 0;
}
