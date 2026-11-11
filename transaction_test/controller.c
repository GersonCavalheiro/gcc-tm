/* Execute the real pragma. Wrappers observe/inject ABI failures only. */
#include "newtm-abi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int a = 5, b = 7;
static unsigned begins, cleanups, writes, commits, mode, injected;
static newtm_slot_v1 *owner;
#ifdef __cplusplus
extern "C" {
#endif
uint32_t __real_GOMP_transaction_stm_v1_begin (newtm_slot_v1 *, uint32_t *);
uint32_t __real_GOMP_transaction_stm_v1_write (struct newtm_context *, const void *, size_t, const void *, size_t);
uint32_t __real_GOMP_transaction_stm_v1_end (newtm_slot_v1 *);
uint32_t __real_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *, size_t);
uint32_t __wrap_GOMP_transaction_stm_v1_begin (newtm_slot_v1 *slot, uint32_t *outer)
{
  assert (!*slot);
  if (owner) assert (owner == slot);
  owner = slot; ++begins;
  if (mode == 3) { *outer = 0; return NEWTM_ABI_INVALID; }
  return __real_GOMP_transaction_stm_v1_begin (slot, outer);
}
uint32_t __wrap_GOMP_transaction_stm_v1_write (struct newtm_context *ctx, const void *object,
                                             size_t offset, const void *in, size_t size)
{
  ++writes;
  if (!injected && writes == 2 && mode && mode != 2 && mode != 3)
    {
      injected = 1;
      assert (a == 5 && b == 7);
      return mode == 4 ? NEWTM_ABI_INVALID : mode == 5 ? 77 : NEWTM_ABI_CONFLICT;
    }
  return __real_GOMP_transaction_stm_v1_write (ctx, object, offset, in, size);
}
uint32_t __wrap_GOMP_transaction_stm_v1_end (newtm_slot_v1 *slot)
{
  ++commits;
  assert (slot == owner && a == 5 && b == 7);
  if (mode == 2 && !injected) { injected = 1; return NEWTM_ABI_CONFLICT; }
  uint32_t status = __real_GOMP_transaction_stm_v1_end (slot);
  if (!status) assert (!*slot);
  return status;
}
uint32_t __wrap_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *slot, size_t conflicts)
{
  assert (slot == owner && *slot && conflicts == ++cleanups);
  assert (a == 5 && b == 7);
  uint32_t status = __real_GOMP_transaction_stm_v1_after_failure (slot, conflicts);
  assert (status == NEWTM_ABI_CONFLICT && !*slot);
  assert (a == 5 && b == 7);
  return status;
}
#ifdef __cplusplus
}
#endif

static int produce (int *p) { return (*p)++; }
static void nested (void)
{
#pragma omp transaction(b)
  { b += 2; }
}
static void execute (void)
{
  int local[2] = { 10, 20 };
  int *p = local;
  int index = 0;
  int result = -91;
#pragma omp transaction(a, b, local, p, index, result)
  {
    int scratch = 3;
    a += produce (&scratch);
    nested ();
    local[index++] += scratch;
    p = local + 1;
    result = *p + scratch;
  }
  assert (a == 8 && b == 9 && local[0] == 14 && local[1] == 20);
  assert (index == 1 && p == local + 1 && result == 24);
}
int main (int argc, char **argv)
{
  if (argc > 1) mode = (unsigned)atoi (argv[1]);
  execute ();
  assert (begins == (mode == 1 || mode == 2 ? 2u : 1u));
  assert (cleanups == (mode == 1 || mode == 2 ? 1u : 0u));
  puts ("real pragma controller passed");
  return 0;
}
