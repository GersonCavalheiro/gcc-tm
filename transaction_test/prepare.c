/* Preparation regression: generated reads/writes, explicit external control. */
#include "newtm-abi.h"
#include <assert.h>
#include <stdint.h>
static int global;
static const int folded_value = 5;
#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t probe_body (struct newtm_context *, const struct newtm_clone_binding_v1 *);
#ifdef __cplusplus
}
#endif

static uint32_t run (struct newtm_context **owner, unsigned storage)
{
  int captured = 4;
  /* The original path remains valid and is reset before the probe. */
#pragma omp transaction(global, captured)
  { int scratch = 7; global = captured + scratch; }
  global = 0;
  captured = 6;
  struct newtm_clone_binding_v1 binding = {
    { &captured, sizeof captured, 0, NEWTM_ABI_SCALAR }, storage
  };
  uint32_t status = probe_body (*owner, &binding);
  if (*owner)
    {
      if (!status) assert (GOMP_transaction_stm_v1_end (owner) == 0);
      else assert (GOMP_transaction_stm_v1_discard (owner) == 0);
    }
  assert (captured == 6);
  return status;
}

static uint32_t nested (struct newtm_context *ctx)
{
#pragma omp transaction(global)
  { int a[2] = { 2, 3 }; int index = 0;
#pragma omp transaction(global)
    { global = a[index++]; }
  }
  global = 0;
  return probe_body (ctx, 0);
}

static uint32_t refused (struct newtm_context *ctx)
{
#pragma omp transaction(global, folded_value)
  { global = folded_value + 2; }
  global = 0;
  return probe_body (ctx, 0);
}

static void two_regions (void)
{
#pragma omp transaction(global)
  { global = 1; }
#pragma omp transaction(global)
  { global = 2; }
}

static void parallel_captures (void)
{
  int sum = 0;
#pragma omp parallel num_threads(2) reduction(+:sum)
  {
    int a[2] = { 4, 5 }, index = 0;
#pragma omp transaction(a, index)
    { a[index++] = 7; }
    a[0] = 4; index = 0;
    struct newtm_clone_binding_v1 bindings[2] = {
      { { a, sizeof a, 0, NEWTM_ABI_AGGREGATE }, NEWTM_STORAGE_REGISTERED },
      { { &index, sizeof index, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED }
    };
    struct newtm_context *slot = 0;
    uint32_t outer = 0;
    assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0 && outer);
    assert (probe_body (slot, bindings) == 0);
    assert (a[0] == 4 && a[1] == 5 && index == 0);
    int staged = 0;
    assert (GOMP_transaction_stm_v1_read (slot, a, 0, &staged, sizeof staged) == 0 && staged == 7);
    assert (GOMP_transaction_stm_v1_read (slot, &index, 0, &staged, sizeof staged) == 0 && staged == 1);
    assert (GOMP_transaction_stm_v1_end (&slot) == 0 && !slot);
    assert (a[0] == 7 && index == 1);
    sum += a[0] + index;
  }
  assert (sum == 16);
}

static void pointer_capture (void)
{
  int object[2] = { 4, 5 };
  int *pointer = object;
  int result = 0;
#pragma omp transaction(object, pointer, result)
  { result = *pointer; pointer = object + 1; result += *pointer; }
  pointer = object; result = 0;
  struct newtm_clone_binding_v1 bindings[4] = {
    { { object, sizeof object, 0, NEWTM_ABI_AGGREGATE }, NEWTM_STORAGE_REGISTERED },
    { { &pointer, sizeof pointer, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED },
    { { object, sizeof object, 0, NEWTM_ABI_AGGREGATE }, NEWTM_STORAGE_REGISTERED },
    { { &result, sizeof result, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED }
  };
  struct newtm_context *slot = 0;
  uint32_t outer = 0;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0 && outer);
  assert (probe_body (slot, bindings) == 0);
  assert (pointer == object && result == 0);
  int staged = 0; int *staged_pointer = 0;
  assert (GOMP_transaction_stm_v1_read (slot, &result, 0, &staged, sizeof staged) == 0 && staged == 9);
  assert (GOMP_transaction_stm_v1_read (slot, &pointer, 0, &staged_pointer, sizeof staged_pointer) == 0
          && staged_pointer == object + 1);
  assert (GOMP_transaction_stm_v1_end (&slot) == 0 && !slot);
  assert (pointer == object + 1 && result == 9);
}

__attribute__((optimize("wrapv"))) static void wrapping_capture (void)
{
  int value = __INT_MAX__;
#pragma omp transaction(global, value)
  { global = value + 1 < value; }
  global = 0;
  struct newtm_clone_binding_v1 binding = {
    { &value, sizeof value, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED
  };
  struct newtm_context *slot = 0;
  uint32_t outer = 0;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0 && outer);
  assert (probe_body (slot, &binding) == 0 && global == 0);
  int staged = 0;
  assert (GOMP_transaction_stm_v1_read (slot, &global, 0, &staged, sizeof staged) == 0 && staged == 1);
  assert (GOMP_transaction_stm_v1_end (&slot) == 0 && !slot);
  assert (global == 1);
}

int main (void)
{
  struct newtm_context *slot = 0;
  uint32_t outer = 0;
  two_regions ();
  parallel_captures ();
  pointer_capture ();
  wrapping_capture ();
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0 && outer);
  assert (run (&slot, NEWTM_STORAGE_REGISTERED) == 0 && global == 13);
  assert (!slot);
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (nested (slot) == 0 && global == 0);
  int staged = 0;
  assert (GOMP_transaction_stm_v1_read (slot, &global, 0, &staged, sizeof staged) == 0 && staged == 2);
  assert (GOMP_transaction_stm_v1_end (&slot) == 0 && !slot);
  assert (global == 2);
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (run (&slot, 999) == NEWTM_ABI_INVALID && global == 0);
  assert (!slot);
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (GOMP_transaction_stm_v1_fail (slot, NEWTM_ABI_CONFLICT) == NEWTM_ABI_CONFLICT);
  assert (run (&slot, NEWTM_STORAGE_REGISTERED) == NEWTM_ABI_CONFLICT && global == 0);
  assert (!slot);
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (refused (slot) == NEWTM_ABI_INVALID && global == 0);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot);
  assert (run (&slot, NEWTM_STORAGE_REGISTERED) == NEWTM_ABI_INVALID && global == 0);
  return 0;
}
