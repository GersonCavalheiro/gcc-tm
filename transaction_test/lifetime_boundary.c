/* Corrupt a child's value after its verifier; exercise the caller's ABI check. */
#include "newtm-abi.h"
#include <assert.h>
#include <string.h>
static int *advance (int *p, int n) { return p + n; }
static int *forward (int *p, int n) { return advance (p, n); }
#ifdef __cplusplus
static int reference_step (int &r, int mode)
{
  if (mode)
    {
      #pragma omp transaction(r)
      { ++r; }
    }
  return r;
}
#endif
__attribute__((used)) static void select_clone (void)
{
  int array[3] = { 1, 2, 3 }, *p = array, result = 0, element = 1;
  #pragma omp transaction(array, p, result, element)
  {
    p = forward (p, 1);
#ifdef __cplusplus
    result = reference_step (element, 0) + reference_step (element, 1);
#else
    result = *p + element;
#endif
  }
  (void) result;
}
#ifdef __cplusplus
extern "C"
#endif
uint32_t invoke_forward (struct newtm_context *, const struct newtm_clone_binding_v1 *,
                         int *, int, int **, struct newtm_clone_binding_v1 *);
#ifdef __cplusplus
extern "C" uint32_t invoke_reference_step (struct newtm_context *, const struct newtm_clone_binding_v1 *, int *, int, int *);
#endif
#ifndef CORRUPT
#define CORRUPT 0
#endif
int main (void)
{
  int array[3] = { 1, 2, 3 }, sentinel, *result = &sentinel;
  struct newtm_clone_binding_v1 input = {
    { array, sizeof array, 0, NEWTM_ABI_AGGREGATE }, NEWTM_STORAGE_REGISTERED
  }, output, saved;
  memset (&output, 0xa5, sizeof output); memcpy (&saved, &output, sizeof saved);
  newtm_slot_v1 slot = 0;
  uint32_t outer = 0;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0 && outer);
  assert (GOMP_transaction_stm_v1_register (slot, array, sizeof array, NEWTM_ABI_AGGREGATE) == 0);
  uint32_t status = invoke_forward (slot, &input, array, 1, &result, &output);
  if (CORRUPT)
    {
      assert (status == NEWTM_ABI_INVALID && result == &sentinel);
      assert (!memcmp (&output, &saved, sizeof output));
      assert (GOMP_transaction_stm_v1_status (slot) == NEWTM_ABI_INVALID);
      assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot);
    }
  else
    {
      assert (status == 0 && result == array + 1);
      assert (output.identity.object == array && output.identity.object_size == sizeof array);
      assert (output.identity.offset == sizeof (int) && output.storage == NEWTM_STORAGE_REGISTERED);
#ifdef __cplusplus
      int value = -1;
      int element = 1;
      struct newtm_clone_binding_v1 ref = { { &element, sizeof element, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED };
      assert (GOMP_transaction_stm_v1_register (slot, &element, sizeof element, NEWTM_ABI_SCALAR) == 0);
      assert (invoke_reference_step (slot, &ref, &element, 0, &value) == 0 && value == 1);
      assert (invoke_reference_step (slot, &ref, &element, 1, &value) == 0 && value == 2);
      assert (element == 1);
#endif
      assert (GOMP_transaction_stm_v1_end (&slot) == 0 && !slot);
#ifdef __cplusplus
      assert (element == 2);
#endif
    }
  return 0;
}
