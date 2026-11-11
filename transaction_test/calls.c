/* Prepared calls with real STM accesses. The external controller is test-owned. */
#include "newtm-abi.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static int nested_counter;
static int *advance (int *p, int n) { return p + n; }
static int *forward (int *p, int n) { return advance (p, n); }
static int bump (int *p) { return ++*p; }
static int nested_bump (int *p)
{
#pragma omp transaction(nested_counter)
  { nested_counter += 1; }
  return bump (p);
}
static int private_chain (void)
{
  int local[2] = { 2, 3 };
  int *q = forward (local, 1);
  return bump (q) + local[0];
}
static int recurse (int n)
{ return n ? recurse (n - 1) + 1 : 0; }
static int *mutate_return (int *p) { bump (p); return forward (p, 1); }
#ifdef __cplusplus
template<class T> static T reference_chain (T &p) { return bump (&p); }
#else
static int captured_chain (int *p)
{
  int local = 3;
  int leaf (void) { *p += local; return *p; }
  int middle (void) { return leaf (); }
  return middle ();
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t probe_call_body (struct newtm_context *, const struct newtm_clone_binding_v1 *);
extern uint32_t invoke_private_chain (struct newtm_context *, const struct newtm_clone_binding_v1 *, int *);
extern uint32_t invoke_recurse (struct newtm_context *, const struct newtm_clone_binding_v1 *, int, int *);
extern uint32_t invoke_mutate_return (struct newtm_context *, const struct newtm_clone_binding_v1 *, int *, int **, struct newtm_clone_binding_v1 *);
#ifdef __cplusplus
extern uint32_t invoke_reference_chain (struct newtm_context *, const struct newtm_clone_binding_v1 *, int *, int *);
#else
extern uint32_t invoke_captured_chain (struct newtm_context *, const struct newtm_clone_binding_v1 *, int *, int *);
#endif
extern uint32_t __real_GOMP_transaction_stm_v1_write (struct newtm_context *, const void *, size_t, const void *, size_t);
static unsigned writes, fail_at, cause;
uint32_t __wrap_GOMP_transaction_stm_v1_write (struct newtm_context *ctx, const void *object,
                                             size_t offset, const void *in, size_t size)
{
  if (++writes == fail_at) return cause;
  return __real_GOMP_transaction_stm_v1_write (ctx, object, offset, in, size);
}
#ifdef __cplusplus
}
#endif

static void begin (struct newtm_context **slot)
{
  uint32_t outer = 0;
  assert (GOMP_transaction_stm_v1_begin (slot, &outer) == 0 && outer);
  writes = fail_at = cause = 0;
}
static int peek (struct newtm_context *ctx, const void *object, size_t offset)
{
  int value = -99;
  assert (GOMP_transaction_stm_v1_read (ctx, object, offset, &value, sizeof value) == 0);
  return value;
}

static void root (unsigned failure, int commit)
{
  int array[3] = { 10, 20, 30 };
  int *p = array;
  int index = 1;
  int result = -91;
#pragma omp transaction(array, p, index, result)
  {
    int scratch = 2;
    int *q = forward (p, index++);
    result = nested_bump (q) + bump (&scratch);
    p = forward (q, 1);
  }
  array[0] = 10; array[1] = 20; array[2] = 30;
  p = array; index = 1; result = -91; nested_counter = 0;
  struct newtm_clone_binding_v1 bindings[] = {
    { { array, sizeof array, 0, NEWTM_ABI_AGGREGATE }, NEWTM_STORAGE_REGISTERED },
    { { &p, sizeof p, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED },
    { { array, sizeof array, 0, NEWTM_ABI_AGGREGATE }, NEWTM_STORAGE_REGISTERED },
    { { &index, sizeof index, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED },
    { { &result, sizeof result, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED }
  };
  struct newtm_context *slot = 0;
  begin (&slot);
  /* index++ is the first write; failure in the nested callee follows it. */
  fail_at = failure ? 3 : 0; cause = failure;
  uint32_t status = probe_call_body (slot, bindings);
  assert (array[1] == 20 && p == array && index == 1 && result == -91 && nested_counter == 0);
  if (failure)
    {
      assert (status == (failure == NEWTM_ABI_CONFLICT ? NEWTM_ABI_CONFLICT : NEWTM_ABI_INVALID));
      assert (writes == 3 && GOMP_transaction_stm_v1_status (slot) == status);
      assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot);
    }
  else
    {
      assert (status == 0);
      assert (peek (slot, array, sizeof (int)) == 21);
      assert (peek (slot, &index, 0) == 2 && peek (slot, &result, 0) == 24);
      int *staged = 0;
      assert (GOMP_transaction_stm_v1_read (slot, &p, 0, &staged, sizeof staged) == 0 && staged == array + 2);
      assert (peek (slot, &nested_counter, 0) == 1);
      if (commit)
        {
          assert (GOMP_transaction_stm_v1_end (&slot) == 0 && !slot);
          assert (array[1] == 21 && p == array + 2 && index == 2 && result == 24 && nested_counter == 1);
        }
      else assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot);
    }
  if (!commit || failure)
    assert (array[1] == 20 && p == array && index == 1 && result == -91 && nested_counter == 0);
}

__attribute__((used)) static void select_extra (void)
{
  int a[2] = { 1, 2 }, result = 0; int *p = a;
#pragma omp transaction(a, p, result)
  {
    result = private_chain () + recurse (2);
    p = mutate_return (p);
#ifdef __cplusplus
    result += reference_chain (a[1]);
#else
    result += captured_chain (a);
#endif
  }
  (void)result;
}

int main (void)
{
  root (0, 1); root (0, 0);
  root (NEWTM_ABI_CONFLICT, 0); root (NEWTM_ABI_INVALID, 0); root (77, 0);
  root (0, 1); /* A fresh explicit attempt runs the complete body again. */
  struct newtm_context *slot = 0;
  begin (&slot);
  int result = -91;
  assert (invoke_private_chain (slot, 0, &result) == 0 && result == 6 && writes == 0);
  int a[2] = { 4, 5 };
  struct newtm_clone_binding_v1 binding = { { a, sizeof a, 0, NEWTM_ABI_AGGREGATE }, NEWTM_STORAGE_REGISTERED };
  assert (GOMP_transaction_stm_v1_register (slot, a, sizeof a, NEWTM_ABI_AGGREGATE) == 0);
  assert (invoke_recurse (slot, 0, 5, &result) == 0 && result == 5 && a[0] == 4);
#ifdef __cplusplus
  assert (invoke_reference_chain (slot, &binding, a, &result) == 0 && result == 5);
#else
  assert (invoke_captured_chain (slot, &binding, a, &result) == 0 && result == 7);
#endif
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot && a[0] == 4);

  for (unsigned failure = 0; failure < 3; ++failure)
    {
      begin (&slot);
      assert (GOMP_transaction_stm_v1_register (slot, a, sizeof a, NEWTM_ABI_AGGREGATE) == 0);
      int *out = a;
      struct newtm_clone_binding_v1 output;
      memset (&output, 0xa5, sizeof output);
      unsigned char saved[sizeof output]; memcpy (saved, &output, sizeof saved);
      fail_at = failure ? 1 : 0; cause = failure;
      uint32_t status = invoke_mutate_return (slot, &binding, a, &out, &output);
      assert (a[0] == 4);
      if (failure)
        assert (status == failure && out == a && !memcmp (saved, &output, sizeof saved));
      else
        {
          assert (status == 0 && out == a + 1 && output.identity.object == a);
          assert (output.identity.offset == sizeof (int) && output.identity.object_size == sizeof a
                  && output.identity.kind == NEWTM_ABI_AGGREGATE && output.storage == NEWTM_STORAGE_REGISTERED);
          assert (peek (slot, a, 0) == 5);
        }
      assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot);
    }
  puts ("prepared call integration passed");
  return 0;
}
