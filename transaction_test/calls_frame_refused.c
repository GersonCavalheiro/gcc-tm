/* Explicit lexical environments are future work: refuse an incompatible frame. */
#include "newtm-abi.h"
#include <assert.h>
static int global;
uint32_t probe_call_body (struct newtm_context *, const struct newtm_clone_binding_v1 *);
static uint32_t run (struct newtm_context *ctx)
{
  int captured = 4;
  int leaf (void) { return captured; }
#pragma omp transaction(global, captured)
  { global = leaf (); }
  global = -91;
  struct newtm_clone_binding_v1 binding = {
    { &captured, sizeof captured, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED
  };
  return probe_call_body (ctx, &binding);
}
int main (void)
{
  struct newtm_context *slot = 0;
  uint32_t outer = 0;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0 && outer);
  assert (run (slot) == NEWTM_ABI_INVALID && global == -91);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot);
  return 0;
}
