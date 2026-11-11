/* A locally ready root must not certify a rejected transitive callee. */
#include "newtm-abi.h"
#include <assert.h>
static int global;
static const int folded = 5;
static int unavailable (void) { return folded + 1; }
static int bridge (void) { return unavailable (); }
#ifdef __cplusplus
extern "C"
#endif
uint32_t probe_call_body (struct newtm_context *, const struct newtm_clone_binding_v1 *);
static uint32_t run (struct newtm_context *ctx)
{
#pragma omp transaction(global, folded)
  { global = bridge (); }
  global = -91;
  return probe_call_body (ctx, 0);
}
int main (void)
{
  struct newtm_context *slot = 0;
  uint32_t outer = 0;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0 && outer);
  assert (run (slot) == NEWTM_ABI_INVALID && global == -91);
  assert (GOMP_transaction_stm_v1_status (slot) == NEWTM_ABI_INVALID);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot);
  assert (global == -91);
  return 0;
}
