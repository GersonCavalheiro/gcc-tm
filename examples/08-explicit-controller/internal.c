/* Exported ABI: an explicit controller, unlike the private helpers. */
#include "newtm-abi.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int main(void)
{
  int balance = 10000;
  size_t conflicts = 0;
  for (;;)
  {
    newtm_slot_v1 slot = 0;
    uint32_t outermost = 0;
    newtm_status_v1 s = GOMP_transaction_stm_v1_begin(&slot, &outermost);
    if (s == NEWTM_ABI_OK && outermost != 1)         s = GOMP_transaction_stm_v1_fail(slot, NEWTM_ABI_INVALID);
    if (s == NEWTM_ABI_OK)         s = GOMP_transaction_stm_v1_register(slot, &balance, sizeof balance,                                             NEWTM_ABI_SCALAR);
    int value = 0;
    if (s == NEWTM_ABI_OK)         s = GOMP_transaction_stm_v1_read(slot, &balance, 0, &value, sizeof value);
    if (s == NEWTM_ABI_OK)
    {
      value += 2500;
      s = GOMP_transaction_stm_v1_write(slot, &balance, 0, &value, sizeof value);
    }
    if (s == NEWTM_ABI_OK)         s = GOMP_transaction_stm_v1_end(&slot);
    if (s == NEWTM_ABI_OK)
    {
      assert(slot == 0);
      break;
    }
    if (!slot)         GOMP_transaction_stm_v1_fatal(s);
    s = GOMP_transaction_stm_v1_fail(slot, s);
    if (s == NEWTM_ABI_CONFLICT && conflicts != SIZE_MAX)         ++conflicts;
    s = GOMP_transaction_stm_v1_after_failure(&slot,                                                conflicts ? conflicts : 1);
    if (s != NEWTM_ABI_CONFLICT)         GOMP_transaction_stm_v1_fatal(s);
  }
  if (balance != 12500) return 1;
  printf("RESULT balance=%d\n", balance);
  return 0;
}
