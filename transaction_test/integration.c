/* Fault each operation of the real pragma, including nested prepared calls. */
#include "newtm-abi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

enum operation { BEGIN = 1, REGISTER, READ, WRITE, STATUS, NEST_BEGIN, NEST_END, END };
static int array[3] = { 10, 20, 30 }, other = 7, index_value, result = -91;
static int **observed_pointer;
static unsigned target, cause, attempts, cleanups, events, failed;
static unsigned trace[512];
static newtm_slot_v1 *owner;
static void originals (void)
{
  assert (array[0] == 10 && array[1] == 20 && array[2] == 30);
  assert (other == 7 && index_value == 0 && result == -91 && *observed_pointer == array);
}
static int event (enum operation operation)
{
  originals ();
  assert (!failed && events < sizeof trace / sizeof trace[0]);
  trace[events++] = operation;
  if (target && events == target && attempts <= 2)
    { failed = 1; return 1; }
  return 0;
}
static void print_trace (const char *kind)
{
  printf ("TRACE %s", kind);
  for (unsigned i = 0; i < events; ++i) printf (" %u", trace[i]);
  putchar ('\n');
}
#ifdef __cplusplus
extern "C" {
#endif
uint32_t __real_GOMP_transaction_stm_v1_begin (newtm_slot_v1 *, uint32_t *);
uint32_t __real_GOMP_transaction_stm_v1_register (struct newtm_context *, const void *, size_t, uint32_t);
uint32_t __real_GOMP_transaction_stm_v1_read (struct newtm_context *, const void *, size_t, void *, size_t);
uint32_t __real_GOMP_transaction_stm_v1_write (struct newtm_context *, const void *, size_t, const void *, size_t);
uint32_t __real_GOMP_transaction_stm_v1_status (struct newtm_context *);
uint32_t __real_GOMP_transaction_stm_v1_nest_begin (struct newtm_context *);
uint32_t __real_GOMP_transaction_stm_v1_nest_end (struct newtm_context *);
uint32_t __real_GOMP_transaction_stm_v1_end (newtm_slot_v1 *);
uint32_t __real_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *, size_t);
uint32_t __wrap_GOMP_transaction_stm_v1_begin (newtm_slot_v1 *slot, uint32_t *outer)
{
  assert (!*slot && (!owner || owner == slot));
  owner = slot; ++attempts; events = failed = 0;
  if (event (BEGIN)) { *outer = 0; return cause; }
  return __real_GOMP_transaction_stm_v1_begin (slot, outer);
}
uint32_t __wrap_GOMP_transaction_stm_v1_register (struct newtm_context *ctx, const void *object, size_t size, uint32_t kind)
{
  if (event (REGISTER)) return cause;
  return __real_GOMP_transaction_stm_v1_register (ctx, object, size, kind);
}
uint32_t __wrap_GOMP_transaction_stm_v1_read (struct newtm_context *ctx, const void *object, size_t offset, void *out, size_t size)
{
  if (event (READ)) return cause;
  return __real_GOMP_transaction_stm_v1_read (ctx, object, offset, out, size);
}
uint32_t __wrap_GOMP_transaction_stm_v1_write (struct newtm_context *ctx, const void *object, size_t offset, const void *in, size_t size)
{
  if (event (WRITE)) return cause;
  return __real_GOMP_transaction_stm_v1_write (ctx, object, offset, in, size);
}
uint32_t __wrap_GOMP_transaction_stm_v1_status (struct newtm_context *ctx)
{
  if (event (STATUS)) return cause;
  return __real_GOMP_transaction_stm_v1_status (ctx);
}
uint32_t __wrap_GOMP_transaction_stm_v1_nest_begin (struct newtm_context *ctx)
{
  if (event (NEST_BEGIN)) return cause;
  return __real_GOMP_transaction_stm_v1_nest_begin (ctx);
}
uint32_t __wrap_GOMP_transaction_stm_v1_nest_end (struct newtm_context *ctx)
{
  if (event (NEST_END)) return cause;
  return __real_GOMP_transaction_stm_v1_nest_end (ctx);
}
uint32_t __wrap_GOMP_transaction_stm_v1_end (newtm_slot_v1 *slot)
{
  assert (slot == owner);
  if (event (END)) return cause;
  uint32_t status = __real_GOMP_transaction_stm_v1_end (slot);
  assert (status == 0 && !*slot);
  return status;
}
uint32_t __wrap_GOMP_transaction_stm_v1_after_failure (newtm_slot_v1 *slot, size_t count)
{
  originals ();
  assert (slot == owner && *slot && failed && events == target);
  assert (count == ++cleanups && count == attempts);
  print_trace ("discard");
  uint32_t status = __real_GOMP_transaction_stm_v1_after_failure (slot, count);
  assert (status == NEWTM_ABI_CONFLICT && !*slot);
  originals ();
  return status;
}
#ifdef __cplusplus
}
#endif

static int produce (int *p) { return (*p)++; }
static int *advance (int *p, int n) { return p + n; }
static int *forward (int *p, int n) { return advance (p, n); }
static int *mutate (int *p) { ++*p; return forward (p, 1); }
static void nested (void)
{
#pragma omp transaction(other)
  { other += 2; }
}
static int *execute (void)
{
  int *pointer = array;
  observed_pointer = &pointer;
#pragma omp transaction(array, other, index_value, result, pointer)
  {
    int scratch = 4;
    int *q = forward (pointer, index_value++);
    *q += produce (&scratch);
    int *r = mutate (q);
    result = *r + scratch;
    nested ();
    pointer = r;
  }
  return pointer;
}
int main (int argc, char **argv)
{
  if (argc > 1) { assert (argc == 3); target = (unsigned)atoi (argv[1]); cause = (unsigned)atoi (argv[2]); }
  int *pointer = execute ();
  assert (array[0] == 15 && array[1] == 20 && array[2] == 30);
  assert (index_value == 1 && other == 9 && result == 25 && pointer == array + 1);
  assert (attempts == (target ? 3u : 1u) && cleanups == (target ? 2u : 0u));
  print_trace ("commit");
  return 0;
}
