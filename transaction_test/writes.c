/* Generated writes in dormant prepared functions; external control is explicit. */
#include "newtm-abi.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

static int call_target;
static int produce (int *p) { return ++*p; }
static void store_call (int *p) { int n = 0; *p = produce (&n); call_target = produce (&n); }
static void set (int *p, int n) { *p = n; }
static int increment (int *p) { return (*p)++; }
static int indexed (int *p, int i) { p[i++] += 3; return i; }
static void fill (int *p) { for (int i = 0; i < 3; ++i) p[i] = 10 + i; }
static int pair_update (int *a, int *b) { *a = 31; int old = (*b)++; *a += 2; return old; }
#ifdef __cplusplus
static int reference_update (int &p) { p += 4; return p; }
#else
struct pair { int x, y; };
static void copy_pair (struct pair *d, const struct pair *s) { *d = *s; }
static void field (struct pair *p, int n) { p->y = n; }
#endif
__attribute__((used)) static void select_writes (void)
{
  int a[3] = { 1, 2, 3 }, b = 7, result = 0;
#ifndef __cplusplus
  struct pair p = { 1, 2 }, q = { 3, 4 };
#pragma omp transaction(a, b, result, call_target, p, q)
#else
#pragma omp transaction(a, b, result, call_target)
#endif
  {
    set (a, 9); result = increment (&b); result += indexed (a, 0);
    fill (a); store_call (a); result += pair_update (a, &b);
#ifdef __cplusplus
    result += reference_update (b);
#else
    copy_pair (&p, &q); field (&p, 11);
#endif
  }
  (void)result;
}

#ifdef __cplusplus
extern "C" {
#endif
#define DECLARE(name, ...) extern uint32_t invoke_##name(struct newtm_context *, const struct newtm_clone_binding_v1 *, __VA_ARGS__)
DECLARE(set, int *, int);
DECLARE(increment, int *, int *);
DECLARE(indexed, int *, int, int *);
DECLARE(fill, int *);
DECLARE(store_call, int *);
DECLARE(pair_update, int *, int *, int *);
#ifdef __cplusplus
DECLARE(reference_update, int *, int *);
#else
DECLARE(copy_pair, struct pair *, const struct pair *);
DECLARE(field, struct pair *, int);
#endif
extern uint32_t __real_GOMP_transaction_stm_v1_read(struct newtm_context *, const void *, size_t, void *, size_t);
extern uint32_t __real_GOMP_transaction_stm_v1_write(struct newtm_context *, const void *, size_t, const void *, size_t);
static unsigned reads, writes, fail_at, failure;
uint32_t __wrap_GOMP_transaction_stm_v1_read(struct newtm_context *ctx, const void *object,
                                           size_t offset, void *out, size_t size)
{
  ++reads;
  return __real_GOMP_transaction_stm_v1_read(ctx, object, offset, out, size);
}
uint32_t __wrap_GOMP_transaction_stm_v1_write(struct newtm_context *ctx, const void *object,
                                            size_t offset, const void *source, size_t size)
{
  ++writes;
  if (fail_at && writes == fail_at) return failure;
  return __real_GOMP_transaction_stm_v1_write(ctx, object, offset, source, size);
}
#ifdef __cplusplus
}
#endif
static struct newtm_clone_binding_v1 binding (void *object, size_t size, uint32_t kind)
{
  struct newtm_clone_binding_v1 b = { { object, size, 0, kind }, NEWTM_STORAGE_REGISTERED };
  return b;
}
static void peek (struct newtm_context *ctx, const void *object, size_t offset, void *out, size_t size)
{ assert (__real_GOMP_transaction_stm_v1_read(ctx, object, offset, out, size) == 0); }
static void begin (struct newtm_context **slot)
{ uint32_t outer = 0; assert (GOMP_transaction_stm_v1_begin(slot, &outer) == 0 && outer); }
static void reg (struct newtm_context *ctx, void *object, size_t size, uint32_t kind)
{ assert (GOMP_transaction_stm_v1_register(ctx, object, size, kind) == 0); }

struct conflict_args {
  int *object;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int ready, finish;
};
static void *competitor (void *arg)
{
  struct conflict_args *a = (struct conflict_args *)arg;
  struct newtm_context *slot = 0; int value = 0;
  begin (&slot); reg (slot, a->object, sizeof value, NEWTM_ABI_SCALAR);
  /* Hold a shared reservation: another read succeeds, promotion must fail. */
  peek (slot, a->object, 0, &value, sizeof value);
  assert (value == 7);
  pthread_mutex_lock (&a->mutex); a->ready = 1; pthread_cond_broadcast (&a->cond);
  while (!a->finish) pthread_cond_wait (&a->cond, &a->mutex);
  pthread_mutex_unlock (&a->mutex);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0);
  return 0;
}

int main (void)
{
  struct newtm_context *slot = 0;
  int object = 5, result = -91, staged = 0;
  struct newtm_clone_binding_v1 input = binding (&object, sizeof object, NEWTM_ABI_SCALAR);
  begin (&slot); reg (slot, &object, sizeof object, NEWTM_ABI_SCALAR);
  reads = writes = 0;
  assert (invoke_set (slot, &input, &object, 17) == 0 && writes == 1 && !reads && object == 5);
  peek (slot, &object, 0, &staged, sizeof staged); assert (staged == 17);
  assert (invoke_increment (slot, &input, &object, &result) == 0 && result == 17 && object == 5);
  peek (slot, &object, 0, &staged, sizeof staged); assert (staged == 18);
  assert (GOMP_transaction_stm_v1_end (&slot) == 0 && !slot && object == 18);

  begin (&slot); reg (slot, &object, sizeof object, NEWTM_ABI_SCALAR);
  reads = writes = 0;
  assert (invoke_set (slot, &input, &object, 99) == 0 && object == 18);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && object == 18);

  /* Caller-owned attempt-private objects bypass the runtime on both paths. */
  begin (&slot);
  int local = 8;
  struct newtm_clone_binding_v1 private_input = binding (&local, sizeof local, NEWTM_ABI_SCALAR);
  private_input.storage = NEWTM_STORAGE_PRIVATE; reads = writes = 0;
  assert (invoke_set (slot, &private_input, &local, 20) == 0 && local == 20);
  assert (invoke_increment (slot, &private_input, &local, &result) == 0 && result == 20 && local == 21);
#ifdef __cplusplus
  assert (invoke_reference_update (slot, &private_input, &local, &result) == 0 && local == 25 && result == 25);
#endif
  assert (!reads && !writes);
  assert (GOMP_transaction_stm_v1_end (&slot) == 0);

  int array[3] = { 1, 2, 3 };
  input = binding (array, sizeof array, NEWTM_ABI_AGGREGATE);
  begin (&slot); reg (slot, array, sizeof array, NEWTM_ABI_AGGREGATE);
  reads = writes = 0;
  assert (invoke_indexed (slot, &input, array, 0, &result) == 0 && result == 1 && writes == 1 && reads == 1);
  assert (array[0] == 1);
  peek (slot, array, 0, &staged, sizeof staged); assert (staged == 4);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && array[0] == 1);
  begin (&slot); reg (slot, array, sizeof array, NEWTM_ABI_AGGREGATE);
  writes = 0;
  assert (invoke_fill (slot, &input, array) == 0 && writes == 3 && array[0] == 1 && array[2] == 3);
  int view[3]; peek (slot, array, 0, view, sizeof view);
  assert (view[0] == 10 && view[1] == 11 && view[2] == 12);
  assert (GOMP_transaction_stm_v1_end (&slot) == 0 && array[0] == 10 && array[2] == 12);

  object = 5; call_target = 6; input = binding (&object, sizeof object, NEWTM_ABI_SCALAR);
  begin (&slot); reg (slot, &object, sizeof object, NEWTM_ABI_SCALAR);
  reg (slot, &call_target, sizeof call_target, NEWTM_ABI_SCALAR); writes = 0;
  assert (invoke_store_call (slot, &input, &object) == 0 && writes == 2 && object == 5 && call_target == 6);
  peek (slot, &object, 0, &staged, sizeof staged); assert (staged == 1);
  peek (slot, &call_target, 0, &staged, sizeof staged); assert (staged == 2);
  assert (GOMP_transaction_stm_v1_end (&slot) == 0 && object == 1 && call_target == 2);
  object = 5; call_target = 6;
  begin (&slot); reg (slot, &object, sizeof object, 0); reg (slot, &call_target, sizeof call_target, 0);
  writes = 0; fail_at = 2; failure = NEWTM_ABI_CONFLICT;
  assert (invoke_store_call (slot, &input, &object) == NEWTM_ABI_CONFLICT);
  assert (writes == 2 && object == 5 && call_target == 6);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && object == 5 && call_target == 6);
  fail_at = 0; object = 1; call_target = 2;

  for (unsigned cause = 1; cause <= 3; ++cause) {
    int a = 1, b = 7;
    struct newtm_clone_binding_v1 inputs[2] = { binding (&a, sizeof a, 0), binding (&b, sizeof b, 0) };
    begin (&slot); reg (slot, &a, sizeof a, 0); reg (slot, &b, sizeof b, 0);
    writes = 0; fail_at = 2; failure = cause == 3 ? 77 : cause; result = -91;
    assert (invoke_pair_update (slot, inputs, &a, &b, &result) == (cause == 1 ? 1u : 2u));
    assert (writes == 2 && result == -91 && a == 1 && b == 7);
    assert (GOMP_transaction_stm_v1_status (slot) == (cause == 1 ? 1u : 2u));
    assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot && a == 1 && b == 7);
    fail_at = 0;
  }

  /* Failure in a read-modify-write never publishes its expression result. */
  begin (&slot); reg (slot, &object, sizeof object, 0);
  writes = 0; fail_at = 1; failure = NEWTM_ABI_CONFLICT; result = -91;
  assert (invoke_increment (slot, &input, &object, &result) == NEWTM_ABI_CONFLICT);
  assert (object == 1 && result == -91 && writes == 1);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0); fail_at = 0;

  begin (&slot); writes = 0;
  assert (invoke_set (slot, &input, &object, 77) == NEWTM_ABI_INVALID && writes == 1 && object == 1);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0);
  begin (&slot); writes = 0; input.storage = 999;
  assert (invoke_set (slot, &input, &object, 77) == NEWTM_ABI_INVALID && !writes && object == 1);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0); input.storage = 0;
  input = binding (array, sizeof array, 1);
  begin (&slot); writes = reads = 0; result = -91;
  assert (invoke_indexed (slot, &input, array, -1, &result) == NEWTM_ABI_INVALID);
  assert (!writes && !reads && result == -91 && array[0] == 10);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0);

  /* A pure store cannot read or publish to the original protected page. */
  long pagesize = sysconf (_SC_PAGESIZE);
  int *page = (int *)mmap (0, (size_t)pagesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert (page != MAP_FAILED); *page = 9; input = binding (page, sizeof *page, 0);
  for (int commit = 0; commit <= 1; ++commit) {
    begin (&slot); reg (slot, page, sizeof *page, 0);
    assert (!mprotect (page, (size_t)pagesize, PROT_NONE)); reads = writes = 0;
    assert (invoke_set (slot, &input, page, 91) == 0 && writes == 1 && !reads);
    peek (slot, page, 0, &staged, sizeof staged); assert (staged == 91);
    assert (!mprotect (page, (size_t)pagesize, PROT_READ | PROT_WRITE) && *page == 9);
    if (commit) assert (GOMP_transaction_stm_v1_end (&slot) == 0 && *page == 91);
    else assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && *page == 9);
  }
  assert (!munmap (page, (size_t)pagesize));

#ifndef __cplusplus
  struct pair a = { 1, 2 }, b = { 17, 19 }, pair_view;
  struct newtm_clone_binding_v1 pairs[2] = { binding (&a, sizeof a, 1), binding (&b, sizeof b, 1) };
  begin (&slot); reg (slot, &a, sizeof a, 1); reg (slot, &b, sizeof b, 1);
  reads = writes = 0; fail_at = 1; failure = NEWTM_ABI_CONFLICT;
  assert (invoke_copy_pair (slot, pairs, &a, &b) == NEWTM_ABI_CONFLICT);
  assert (reads == 1 && writes == 1 && a.x == 1 && a.y == 2 && b.y == 19);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0); fail_at = 0;
  begin (&slot); reg (slot, &a, sizeof a, 1); reg (slot, &b, sizeof b, 1); reads = writes = 0;
  assert (invoke_field (slot, pairs + 1, &b, 27) == 0 && b.y == 19); reads = writes = 0;
  assert (invoke_copy_pair (slot, pairs, &a, &b) == 0 && reads == 1 && writes == 1 && a.x == 1 && a.y == 2);
  peek (slot, &a, 0, &pair_view, sizeof pair_view); assert (pair_view.x == 17 && pair_view.y == 27);
  assert (invoke_field (slot, pairs, &a, 23) == 0);
  peek (slot, &a, 0, &pair_view, sizeof pair_view); assert (pair_view.x == 17 && pair_view.y == 23);
  assert (GOMP_transaction_stm_v1_end (&slot) == 0 && a.x == 17 && a.y == 23 && b.y == 27);
  pairs[1] = pairs[0]; begin (&slot); reg (slot, &a, sizeof a, 1);
  assert (invoke_field (slot, pairs, &a, 29) == 0);
  assert (invoke_copy_pair (slot, pairs, &a, &a) == 0);
  peek (slot, &a, 0, &pair_view, sizeof pair_view); assert (pair_view.x == 17 && pair_view.y == 29);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && a.x == 17 && a.y == 23);
#endif

  int first = 1, shared = 7;
  struct conflict_args args = { &shared, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0 };
  pthread_t thread; assert (!pthread_create (&thread, 0, competitor, &args));
  pthread_mutex_lock (&args.mutex);
  while (!args.ready) pthread_cond_wait (&args.cond, &args.mutex);
  pthread_mutex_unlock (&args.mutex);
  struct newtm_clone_binding_v1 inputs[2] = { binding (&first, sizeof first, 0), binding (&shared, sizeof shared, 0) };
  begin (&slot); reg (slot, &first, sizeof first, 0); reg (slot, &shared, sizeof shared, 0); writes = 0; result = -91;
  assert (invoke_pair_update (slot, inputs, &first, &shared, &result) == NEWTM_ABI_CONFLICT);
  assert (writes == 2 && first == 1 && shared == 7 && result == -91);
  assert (GOMP_transaction_stm_v1_after_failure (&slot, 1) == NEWTM_ABI_CONFLICT && !slot);
  pthread_mutex_lock (&args.mutex); args.finish = 1; pthread_cond_broadcast (&args.cond); pthread_mutex_unlock (&args.mutex);
  assert (!pthread_join (thread, 0));
  begin (&slot); reg (slot, &first, sizeof first, 0); reg (slot, &shared, sizeof shared, 0); writes = 0;
  assert (invoke_pair_update (slot, inputs, &first, &shared, &result) == 0 && result == 7 && writes == 3);
  assert (first == 1 && shared == 7);
  peek (slot, &first, 0, &staged, sizeof staged); assert (staged == 33);
  assert (GOMP_transaction_stm_v1_end (&slot) == 0 && first == 33 && shared == 8);
  assert (!pthread_mutex_destroy (&args.mutex) && !pthread_cond_destroy (&args.cond));
  return 0;
}
