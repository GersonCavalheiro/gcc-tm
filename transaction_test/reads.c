/* Stage 5.2: generated reads, manually staged writes and external cleanup. */
#include "newtm-abi.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int scalar (const int *p) { return *p; }
static int indexed (const int *p, int n) { int v = p[n++]; return v + n; }
static int two (const int *p) { int a = p[0]; return a + p[1]; }
static int nested_read (const int *p) { return two (p) + 5; }
static int loop_read (const int *p) { int sum = 0; for (int i = 0; i < 2; ++i) sum += p[i]; return sum; }
#define TWO_READS(P, A, B) A = *(P); B = *(P)
static int repeated (const int *p) { int a = *p; int b = *p; return a + b; }
static int macro_reads (const int *p) { int a, b; TWO_READS(p, a, b); return a + b; }
#ifdef __cplusplus
static int private_ref (int &r) {
#pragma omp transaction(r)
  { ++r; }
  return r;
}
#endif
#ifndef __cplusplus
struct pair { int x, y; };
static struct pair pair_read (const struct pair *p) { return *p; }
static void copy_pair (struct pair *d, const struct pair *s) { *d = *s; }
#endif
__attribute__((used)) static void select_reads (void)
{
  int a[3] = { 1, 2, 3 }, result = 0;
#ifdef __cplusplus
  int element = 3;
#endif
#ifndef __cplusplus
  struct pair p = { 1, 2 }, q = { 0, 0 };
#pragma omp transaction(a, result, p, q)
#else
#pragma omp transaction(a, result, element)
#endif
  {
    result = scalar (a) + indexed (a, 0) + two (a) + nested_read (a) + loop_read (a)
      + repeated (a) + macro_reads (a);
#ifdef __cplusplus
    result += private_ref (element);
#endif
#ifndef __cplusplus
    q = pair_read (&p); copy_pair (&q, &p);
#endif
  }
  (void)result;
#ifndef __cplusplus
  (void)q;
#endif
}

#ifdef __cplusplus
extern "C" {
#endif
#define DECLARE(name, ...) extern uint32_t invoke_##name(struct newtm_context *, const struct newtm_clone_binding_v1 *, __VA_ARGS__)
DECLARE(scalar, const int *, int *);
DECLARE(indexed, const int *, int, int *);
DECLARE(two, const int *, int *);
DECLARE(nested_read, const int *, int *);
DECLARE(loop_read, const int *, int *);
DECLARE(repeated, const int *, int *);
DECLARE(macro_reads, const int *, int *);
#ifdef __cplusplus
DECLARE(private_ref, int *, int *);
#endif
#ifndef __cplusplus
DECLARE(pair_read, const struct pair *, struct pair *);
#endif
extern uint32_t probe_read_body(struct newtm_context *, const struct newtm_clone_binding_v1 *);
extern uint32_t __real_GOMP_transaction_stm_v1_read(struct newtm_context *, const void *, size_t, void *, size_t);
static unsigned read_calls, fail_at, failure;
uint32_t __wrap_GOMP_transaction_stm_v1_read(struct newtm_context *ctx, const void *object,
                                           size_t offset, void *out, size_t size)
{
  ++read_calls;
  if (fail_at && read_calls == fail_at) return failure;
  return __real_GOMP_transaction_stm_v1_read(ctx, object, offset, out, size);
}
#ifdef __cplusplus
}
#endif

static int objects_a[2] = { 11, 12 }, objects_b[2] = { 31, 32 }, observed;
static void captured_pointer (int mismatch)
{
  int *p = objects_a;
#pragma omp transaction(p, objects_a, objects_b, observed)
  { observed = *p; }
  observed = -91;
  struct newtm_context *slot = 0; uint32_t outer = 0;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (GOMP_transaction_stm_v1_register (slot, &p, sizeof p, NEWTM_ABI_SCALAR) == 0);
  int *staged = objects_b;
  assert (GOMP_transaction_stm_v1_write (slot, &p, 0, &staged, sizeof staged) == 0);
  struct newtm_clone_binding_v1 bindings[2] = {
    { { &p, sizeof p, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_REGISTERED },
    { { mismatch ? objects_a : objects_b, sizeof objects_b, 0, NEWTM_ABI_AGGREGATE }, NEWTM_STORAGE_REGISTERED }
  };
  read_calls = 0;
  uint32_t status = probe_read_body (slot, bindings);
  assert (p == objects_a);
  if (mismatch) assert (status == NEWTM_ABI_INVALID && observed == -91 && read_calls == 1);
  else {
    assert (status == 0 && observed == -91 && read_calls == 2);
    int staged_result = 0;
    assert (GOMP_transaction_stm_v1_read (slot, &observed, 0, &staged_result, sizeof staged_result) == 0
            && staged_result == 31);
  }
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot);
  assert (p == objects_a && observed == -91);
}

struct conflict_args {
  int *object;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int ready, finish;
};
static void *competitor (void *arg)
{
  struct conflict_args *a = (struct conflict_args *)arg;
  struct newtm_context *slot = 0; uint32_t outer = 0; int value = 55;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (GOMP_transaction_stm_v1_register (slot, a->object, sizeof(int), NEWTM_ABI_SCALAR) == 0);
  assert (GOMP_transaction_stm_v1_write (slot, a->object, 0, &value, sizeof value) == 0);
  pthread_mutex_lock (&a->mutex); a->ready = 1; pthread_cond_broadcast (&a->cond);
  while (!a->finish) pthread_cond_wait (&a->cond, &a->mutex);
  pthread_mutex_unlock (&a->mutex);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0);
  return 0;
}

int main (void)
{
  int a[3] = { 1, 2, 3 }, result = -91, value = 71;
  struct newtm_context *slot = 0; uint32_t outer = 0;
  struct newtm_clone_binding_v1 binding = { { a, sizeof a, 0, NEWTM_ABI_AGGREGATE }, NEWTM_STORAGE_REGISTERED };
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (GOMP_transaction_stm_v1_register (slot, a, sizeof a, NEWTM_ABI_AGGREGATE) == 0);
  assert (GOMP_transaction_stm_v1_write (slot, a, 0, &value, sizeof value) == 0);
  read_calls = 0;
  assert (invoke_scalar (slot, &binding, a, &result) == 0 && result == 71 && a[0] == 1 && read_calls == 1);
  assert (invoke_indexed (slot, &binding, a, 0, &result) == 0 && result == 72);
  assert (invoke_two (slot, &binding, a, &result) == 0 && result == 73);
  assert (invoke_nested_read (slot, &binding, a, &result) == 0 && result == 78);
  assert (invoke_loop_read (slot, &binding, a, &result) == 0 && result == 73);
  read_calls = 0;
  assert (invoke_repeated (slot, &binding, a, &result) == 0 && result == 142 && read_calls == 2);
  read_calls = 0;
  assert (invoke_macro_reads (slot, &binding, a, &result) == 0 && result == 142 && read_calls == 2);
  assert (GOMP_transaction_stm_v1_end (&slot) == 0 && a[0] == 71);

  /* A proven private branch needs no registration or runtime read. */
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  int private_array[3] = { 71, 2, 3 };
  struct newtm_clone_binding_v1 local_binding = binding;
  local_binding.identity.object = private_array; local_binding.storage = NEWTM_STORAGE_PRIVATE;
  read_calls = 0;
  assert (invoke_scalar (slot, &local_binding, private_array, &result) == 0 && result == 71 && !read_calls);
#ifdef __cplusplus
  int private_value = 7;
  struct newtm_clone_binding_v1 private_binding = {
    { &private_value, sizeof private_value, 0, NEWTM_ABI_SCALAR }, NEWTM_STORAGE_PRIVATE
  };
  assert (invoke_private_ref (slot, &private_binding, &private_value, &result) == 0);
  assert (private_value == 8 && result == 8 && !read_calls);
#endif
  assert (GOMP_transaction_stm_v1_end (&slot) == 0);

  for (unsigned cause = 1; cause <= 3; ++cause) {
    assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
    assert (GOMP_transaction_stm_v1_register (slot, a, sizeof a, NEWTM_ABI_AGGREGATE) == 0);
    read_calls = 0; fail_at = 2; failure = cause == 3 ? 77 : cause; result = -91;
    assert (invoke_nested_read (slot, &binding, a, &result) == (cause == 1 ? 1u : 2u));
    assert (result == -91 && read_calls == 2 && a[0] == 71);
    assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && !slot);
    fail_at = 0;
  }

  /* Bounds and missing registration fail before an output is consumed. */
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  read_calls = 0; result = -91;
  assert (invoke_indexed (slot, &binding, a, -1, &result) == NEWTM_ABI_INVALID);
  assert (!read_calls && result == -91);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0);
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (invoke_scalar (slot, &binding, a, &result) == NEWTM_ABI_INVALID && result == -91);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0);

  /* The original page becomes unreadable after staging a complete value. */
  long pagesize = sysconf (_SC_PAGESIZE);
  int *page = (int *)mmap (0, (size_t)pagesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert (page != MAP_FAILED); *page = 9;
  binding.identity.object = page; binding.identity.object_size = sizeof *page; binding.identity.kind = NEWTM_ABI_SCALAR;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (GOMP_transaction_stm_v1_register (slot, page, sizeof *page, NEWTM_ABI_SCALAR) == 0);
  assert (GOMP_transaction_stm_v1_write (slot, page, 0, &value, sizeof value) == 0);
  assert (!mprotect (page, (size_t)pagesize, PROT_NONE));
  assert (invoke_scalar (slot, &binding, page, &result) == 0 && result == value);
  assert (!mprotect (page, (size_t)pagesize, PROT_READ | PROT_WRITE));
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0 && *page == 9);
  assert (!munmap (page, (size_t)pagesize));

#ifndef __cplusplus
  struct pair original = { 2, 3 }, staged = { 17, 19 }, out = { -91, -92 };
  binding.identity.object = &original; binding.identity.object_size = sizeof original; binding.identity.kind = NEWTM_ABI_AGGREGATE;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (GOMP_transaction_stm_v1_register (slot, &original, sizeof original, NEWTM_ABI_AGGREGATE) == 0);
  assert (GOMP_transaction_stm_v1_write (slot, &original, 0, &staged, sizeof staged) == 0);
  assert (invoke_pair_read (slot, &binding, &original, &out) == 0 && out.x == 17 && out.y == 19);
  assert (original.x == 2 && original.y == 3);
  fail_at = 1; read_calls = 0; failure = NEWTM_ABI_CONFLICT; out.x = -91; out.y = -92;
  assert (invoke_pair_read (slot, &binding, &original, &out) == NEWTM_ABI_CONFLICT && out.x == -91 && out.y == -92);
  fail_at = 0;
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0);
#endif

  captured_pointer (0); captured_pointer (1);

  int shared = 7;
  struct conflict_args args = { &shared, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0 };
  pthread_t thread;
  assert (!pthread_create (&thread, 0, competitor, &args));
  pthread_mutex_lock (&args.mutex);
  while (!args.ready) pthread_cond_wait (&args.cond, &args.mutex);
  pthread_mutex_unlock (&args.mutex);
  binding.identity.object = &shared; binding.identity.object_size = sizeof shared; binding.identity.kind = NEWTM_ABI_SCALAR;
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == 0);
  assert (GOMP_transaction_stm_v1_register (slot, &shared, sizeof shared, NEWTM_ABI_SCALAR) == 0);
  result = -91;
  assert (invoke_scalar (slot, &binding, &shared, &result) == NEWTM_ABI_CONFLICT && result == -91 && shared == 7);
  assert (GOMP_transaction_stm_v1_discard (&slot) == 0);
  pthread_mutex_lock (&args.mutex); args.finish = 1; pthread_cond_broadcast (&args.cond); pthread_mutex_unlock (&args.mutex);
  assert (!pthread_join (thread, 0));
  assert (!pthread_mutex_destroy (&args.mutex) && !pthread_cond_destroy (&args.cond));
  return 0;
}
