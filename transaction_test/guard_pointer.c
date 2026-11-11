/* Test a clone without dereferencing the synthetic extreme address. */
#include "newtm-abi.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static int *advance (int *p, int n) { return p + n; }
__attribute__((used)) static void select_clone (void)
{
  int array[2] = { 0, 0 }, *p = array;
  #pragma omp transaction(array, p)
  { p = advance(p, 1); }
}
#ifdef __cplusplus
extern "C"
#endif
uint32_t invoke_advance (struct newtm_context *, const struct newtm_clone_binding_v1 *,
                         int *, int, int **, struct newtm_clone_binding_v1 *);

int main (void)
{
  newtm_slot_v1 slot = 0;
  uint32_t outer = 0;
  int sentinel, *result = &sentinel;
  struct newtm_clone_binding_v1 output, saved;
  memset (&output, 0xa5, sizeof output);
  memcpy (&saved, &output, sizeof saved);
  /* Last byte is representable, but the one-past address would wrap. */
  int *extreme = (int *) (UINTPTR_MAX - (sizeof (int) - 1));
  struct newtm_clone_binding_v1 input = {
    { extreme, sizeof (int), 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED
  };
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == NEWTM_ABI_OK && outer);
  assert (invoke_advance (slot, &input, extreme, 1, &result, &output) == NEWTM_ABI_INVALID);
  assert (result == &sentinel && !memcmp (&output, &saved, sizeof output));
  assert (GOMP_transaction_stm_v1_status (slot) == NEWTM_ABI_INVALID);
  assert (GOMP_transaction_stm_v1_discard (&slot) == NEWTM_ABI_OK && !slot);

  int array[2] = { 4, 5 };
  input.identity.object = array;
  input.identity.object_size = sizeof array;
  input.identity.kind = NEWTM_ABI_AGGREGATE;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == NEWTM_ABI_OK && outer);
  assert (invoke_advance (slot, &input, array, 2, &result, &output) == NEWTM_ABI_OK);
  assert (result == array + 2 && output.identity.object == array);
  assert (output.identity.offset == sizeof array);
  assert (GOMP_transaction_stm_v1_end (&slot) == NEWTM_ABI_OK && !slot);
  return 0;
}
