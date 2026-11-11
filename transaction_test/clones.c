/* Prepared clone tests. The pragma only selects functions; it is not executed.
   Prepared reads and writes use STM; the external controller is test-owned. */
#include "newtm-abi.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int global;
static int later (int);
static int chain (int n) { return later(n) + 1; }
static int later (int n) { int local = n + 2; return local * 3; }
static void assign_global (void) { global = chain(3); }
static int factorial (int n) { return n <= 1 ? 1 : n * factorial(n - 1); }
static int *advance (int *p, int n) { return p + n; }
static int *forward (int *p, int n) { return advance(p, n); }
static int indirect (int *p) { int scratch[1] = {2}; return *p + scratch[0]; }
static int once (int *p, int n) { int i = n; int v = *advance(p, i++); return i + v; }
static int odd (int);
static int even (int n) { return n ? odd(n-1) : 1; }
static int odd (int n) { return n ? even(n-1) : 0; }
static int nested (void) {
  #pragma omp transaction(global)
  { }
  return 7;
}
static int private_caller (int n) { int value = n; return indirect(&value); }
static void empty (void) {}
static int fail_leaf (int n) { return n + 1; }
static int fail_chain (int n) { return fail_leaf(n) + 100; }
static int *fail_pointer (int *p) { return p; }
static int *pointer_chain (int *p) { return fail_pointer(p); }
#ifdef __cplusplus
static int by_reference (int &p) { return p + 3; }
template<class T> static T templated(T n) { return n + 5; }
#else
static int captured_private (int n) {
  int value=n; int*p=&value;
  int inner(int x) { return *p+x; }
  return inner(2);
}
static int captured_external (int*p) {
  int inner(int x) { return *p+x; }
  return inner(3);
}
static int captured_forward (int*p) {
  int leaf(void) { return *p+4; }
  int middle(void) { return leaf(); }
  return middle();
}
static int captured_write_only (int n) {
  int*p;
  int inner(void) { p=&n; return *p; }
  (void)&p; return inner();
}
struct pair { int a, b; };
static struct pair pair_make (int n) { struct pair p = { n, n+1 }; return p; }
static struct pair pair_chain (int n) { return pair_make(n); }
#endif
__attribute__((used,noinline)) static void select_clones (void)
{
  int a[4] = { 1,2,3,4 }, result = 0;
  int *p = a;
  #pragma omp transaction(a,p,result,global)
  {
    result = chain(2) + factorial(4) + private_caller(6) + even(4) + nested();
    result += once(a, 0);
    assign_global();
    p = forward(p, 1);
    result = indirect(p);
    result = fail_chain(result);
    p = pointer_chain(p);
    empty();
#ifdef __cplusplus
    result = by_reference(a[1]) + templated(7);
#else
    result += captured_private(3) + captured_external(a) + captured_forward(a);
    result += captured_write_only(12);
    struct pair value = pair_chain(8);
    result = value.a + value.b;
#endif
    global = result;
  }
}
#ifdef __cplusplus
extern "C" {
#endif
#define DECLARE(name,...) extern uint32_t invoke_##name(struct newtm_context *, const struct newtm_clone_binding_v1 *, __VA_ARGS__)
DECLARE(chain, int, int *);
DECLARE(factorial, int, int *);
DECLARE(even, int, int *);
DECLARE(once, int *, int, int *);
DECLARE(nested, int *);
DECLARE(private_caller, int, int *);
DECLARE(forward, int *, int, int **, struct newtm_clone_binding_v1 *);
DECLARE(indirect, int *, int *);
DECLARE(fail_chain, int, int *);
DECLARE(pointer_chain, int *, int **, struct newtm_clone_binding_v1 *);
extern uint32_t invoke_empty(struct newtm_context *, const struct newtm_clone_binding_v1 *);
#ifdef __cplusplus
DECLARE(by_reference, int *, int *);
DECLARE(templated, int, int *);
}
#else
DECLARE(pair_chain, int, struct pair *);
DECLARE(captured_private, int, int *);
DECLARE(captured_external, int *, int *);
DECLARE(captured_forward, int *, int *);
DECLARE(captured_write_only, int, int *);
#endif
#ifndef CAUSE
#define CAUSE 0
#endif
#ifndef POINTER_CAUSE
#define POINTER_CAUSE 0
#endif
int main(void)
{
  assert(chain(4) == 19 && factorial(6) == 720 && private_caller(9) == 11);
  struct newtm_context *slot = 0;
  uint32_t outer = 0;
  assert(GOMP_transaction_stm_v1_begin(&slot, &outer) == NEWTM_ABI_OK && outer);
  int result = -91;
  assert(invoke_chain(slot, 0, 4, &result) == 0 && result == 19);
  assert(invoke_factorial(slot, 0, 6, &result) == 0 && result == 720);
  assert(invoke_even(slot, 0, 8, &result) == 0 && result == 1);
  assert(invoke_even(slot, 0, 7, &result) == 0 && result == 0);
  assert(invoke_nested(slot, 0, &result) == 0 && result == 7);
  assert(invoke_private_caller(slot, 0, 9, &result) == 0 && result == 11);
  assert(invoke_empty(slot, 0) == 0);
#ifdef __cplusplus
  assert(invoke_templated(slot, 0, 7, &result) == 0 && result == 12);
#else
  struct pair pair = { -91, -91 };
  assert(invoke_pair_chain(slot, 0, 5, &pair) == 0 && pair.a == 5 && pair.b == 6);
  assert(invoke_captured_private(slot, 0, 5, &result) == 0 && result == 7);
  assert(invoke_captured_write_only(slot, 0, 12, &result) == 0 && result == 12);
#endif
  int array[4] = { 3,4,5,6 };
  assert(GOMP_transaction_stm_v1_register(slot, array, sizeof array, NEWTM_ABI_AGGREGATE) == 0);
  struct newtm_clone_binding_v1 input = { { array, sizeof array, sizeof(int), NEWTM_ABI_AGGREGATE }, NEWTM_STORAGE_REGISTERED };
  struct newtm_clone_binding_v1 output;
  memset(&output, 0xa5, sizeof output);
  int *p = 0;
  assert(invoke_forward(slot, &input, array+1, 2, &p, &output) == 0);
  assert(p == array+3 && output.identity.object == array);
  assert(output.identity.object_size == sizeof array && output.identity.offset == 3*sizeof(int));
  assert(output.identity.kind == NEWTM_ABI_AGGREGATE && output.storage == NEWTM_STORAGE_REGISTERED);
  assert(invoke_indirect(slot, &input, array+1, &result) == 0 && result == 6);
  assert(invoke_once(slot, &input, array+1, 1, &result) == 0 && result == 7);
#ifdef __cplusplus
  assert(invoke_by_reference(slot, &input, array+1, &result) == 0 && result == 7);
#else
  assert(invoke_captured_external(slot, &input, array+1, &result) == 0 && result == 7);
  assert(invoke_captured_forward(slot, &input, array+1, &result) == 0 && result == 8);
#endif
  /* PRIVATE follows the same identity through two calls, without registration. */
  int local[4] = { 1,2,3,4 };
  input.identity.object = local; input.identity.offset = 0; input.storage = NEWTM_STORAGE_PRIVATE;
  assert(invoke_forward(slot, &input, local, 1, &p, &output) == 0);
  assert(p == local+1 && output.identity.object == local && output.storage == NEWTM_STORAGE_PRIVATE);
  assert(GOMP_transaction_stm_v1_end(&slot) == 0 && !slot);

  assert(GOMP_transaction_stm_v1_begin(&slot, &outer) == 0);
  result = -91;
  uint32_t status = invoke_fail_chain(slot, 0, 2, &result);
  if (CAUSE) {
    assert(status == CAUSE && result == -91);
    assert(GOMP_transaction_stm_v1_status(slot) == CAUSE);
    assert(GOMP_transaction_stm_v1_discard(&slot) == 0 && !slot);
  } else {
    assert(status == 0 && result == 103);
    assert(GOMP_transaction_stm_v1_end(&slot) == 0 && !slot);
  }
  /* Pointer value AND its entire result binding are preserved on failure. */
  assert(GOMP_transaction_stm_v1_begin(&slot, &outer) == 0);
  assert(GOMP_transaction_stm_v1_register(slot, local, sizeof local, NEWTM_ABI_AGGREGATE) == 0);
  input.storage = NEWTM_STORAGE_REGISTERED;
  p = local+3;
  memset(&output, 0xa5, sizeof output);
  unsigned char saved[sizeof output]; memcpy(saved, &output, sizeof output);
  status = invoke_pointer_chain(slot, &input, local, &p, &output);
  if (POINTER_CAUSE) {
    assert(status == POINTER_CAUSE && p == local+3);
    assert(!memcmp(saved, &output, sizeof output));
    assert(GOMP_transaction_stm_v1_status(slot) == POINTER_CAUSE);
    assert(GOMP_transaction_stm_v1_discard(&slot) == 0 && !slot);
  } else {
    assert(status == 0 && p == local && output.identity.object == local);
    assert(output.storage == NEWTM_STORAGE_REGISTERED);
    assert(GOMP_transaction_stm_v1_end(&slot) == 0 && !slot);
  }
  /* An unknown storage tag never authorizes a read of common memory. */
  assert(GOMP_transaction_stm_v1_begin(&slot, &outer) == 0);
  input.storage = 999; result = -91;
  assert(invoke_indirect(slot, &input, local, &result) == NEWTM_ABI_INVALID);
  assert(result == -91);
  assert(GOMP_transaction_stm_v1_discard(&slot) == 0 && !slot);

  /* A condemned context must not publish even a pure scalar result. */
  assert(GOMP_transaction_stm_v1_begin(&slot, &outer) == 0);
  assert(GOMP_transaction_stm_v1_fail(slot, NEWTM_ABI_CONFLICT) == NEWTM_ABI_CONFLICT);
  result = -91;
  assert(invoke_chain(slot, 0, 2, &result) == NEWTM_ABI_CONFLICT && result == -91);
  assert(GOMP_transaction_stm_v1_after_failure(&slot, 1) == NEWTM_ABI_CONFLICT && !slot);
  result = -91;
  assert(invoke_chain(0, 0, 2, &result) == NEWTM_ABI_INVALID && result == -91);
  puts("prepared clone contracts passed");
  return 0;
}
