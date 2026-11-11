/* Exercise only the public ABI, with ordinary source calls also replaced by
   the test plugin with compiler-built calls. No pragma STM activation. */
#include "newtm-abi.h"
#include <assert.h>
#include <limits.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#ifdef __cplusplus
#define ABI_ASSERT static_assert
#else
#define ABI_ASSERT _Static_assert
#endif

ABI_ASSERT (sizeof (newtm_status_v1) * CHAR_BIT == 32, "status width");
ABI_ASSERT (sizeof (((struct newtm_binding_v1 *) 0)->kind) * CHAR_BIT == 32,
                "kind width");
/* The plugin compares this frontend layout with the compiler-created type. */
__attribute__((used,noinline)) unsigned
abi_layout (struct newtm_binding_v1 *binding)
{
  return binding->kind;
}

static void
start (newtm_slot_v1 *slot)
{
  uint32_t outer = 99;
  assert (GOMP_transaction_stm_v1_begin (slot, &outer) == NEWTM_ABI_OK);
  assert (outer == 1 && *slot);
}

struct reader_input { void *object; newtm_slot_v1 foreign; };
/* Main retains a write reservation until this real competing thread exits. */
static void *
competing_reader (void *argument)
{
  struct reader_input *input = (struct reader_input *) argument;
  assert (GOMP_transaction_stm_v1_nest_begin (input->foreign) == NEWTM_ABI_INVALID);
  assert (GOMP_transaction_stm_v1_nest_end (input->foreign) == NEWTM_ABI_INVALID);
  newtm_slot_v1 slot = 0;
  int result = -91;
  start (&slot);
  assert (GOMP_transaction_stm_v1_nest_begin (slot) == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_register (slot, input->object, sizeof (int),
                                           NEWTM_ABI_SCALAR) == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_read (slot, input->object, 0, &result, sizeof result)
          == NEWTM_ABI_CONFLICT && result == -91);
  assert (GOMP_transaction_stm_v1_nest_end (slot) == NEWTM_ABI_CONFLICT);
  assert (GOMP_transaction_stm_v1_nest_begin (slot) == NEWTM_ABI_CONFLICT);
  assert (GOMP_transaction_stm_v1_after_failure (&slot, 1)
          == NEWTM_ABI_CONFLICT && !slot);
  return 0;
}

static void
fatal_case (void)
{
  newtm_slot_v1 slot = 0;
  int object = 9, value = 25;
  start (&slot);
  assert (GOMP_transaction_stm_v1_register (slot, &object, sizeof object,
                                           NEWTM_ABI_SCALAR) == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_write (slot, &object, 0, &value, sizeof value)
          == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_fail (slot, UINT32_MAX) == NEWTM_ABI_INVALID);
  (void) GOMP_transaction_stm_v1_after_failure (&slot, 1);
  _exit (99);
}

int
main (void)
{
  newtm_slot_v1 slot = 0, other = 0;
  uint32_t outer = 99;
  int object = 7, value = 23, result = -91;
  int array[3] = { 1, 2, 3 };
  assert (GOMP_transaction_stm_v1_begin (0, &outer) == NEWTM_ABI_INVALID);
  assert (outer == 0);
  assert (GOMP_transaction_stm_v1_begin (&slot, 0) == NEWTM_ABI_INVALID);
  assert (!slot);
  assert (GOMP_transaction_stm_v1_status (0) == NEWTM_ABI_INVALID);
  start (&slot);
  assert (GOMP_transaction_stm_v1_status (slot) == NEWTM_ABI_OK);
  newtm_slot_v1 copy = slot;
  assert (GOMP_transaction_stm_v1_end (&copy) == NEWTM_ABI_INVALID);
  assert (copy == slot);
  assert (GOMP_transaction_stm_v1_register (slot, &object, sizeof object,
                                           NEWTM_ABI_SCALAR) == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_begin (&slot, &outer) == NEWTM_ABI_OK && !outer);
  assert (GOMP_transaction_stm_v1_nest_begin (slot) == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_register (slot, &object, sizeof object,
                                           NEWTM_ABI_SCALAR) == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_write (slot, &object, 0, &value, sizeof value)
          == NEWTM_ABI_OK && object == 7);
  assert (GOMP_transaction_stm_v1_read (slot, &object, 0, &result, sizeof result)
          == NEWTM_ABI_OK && result == 23);
  assert (GOMP_transaction_stm_v1_nest_end (slot) == NEWTM_ABI_OK && slot && object == 7);
  assert (GOMP_transaction_stm_v1_end (&slot) == NEWTM_ABI_OK && slot && object == 7);
  assert (GOMP_transaction_stm_v1_end (&slot) == NEWTM_ABI_OK && !slot && object == 23);

  start (&slot);
  assert (GOMP_transaction_stm_v1_register (slot, array, sizeof array,
                                           NEWTM_ABI_AGGREGATE) == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_write (slot, array, sizeof (int), &value,
                                        sizeof value) == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_end (&slot) == NEWTM_ABI_OK && !slot);
  assert (array[0] == 1 && array[1] == 23 && array[2] == 3);

  start (&slot);
  assert (GOMP_transaction_stm_v1_register (slot, &object, sizeof object,
                                           NEWTM_ABI_SCALAR) == NEWTM_ABI_OK);
  value = 41;
  assert (GOMP_transaction_stm_v1_write (slot, &object, 0, &value, sizeof value)
          == NEWTM_ABI_OK);
  pthread_t reader;
  struct reader_input input = { &object, slot };
  assert (!pthread_create (&reader, 0, competing_reader, &input));
  assert (!pthread_join (reader, 0));
  start (&other);
  assert (GOMP_transaction_stm_v1_register (other, &object, sizeof object,
                                           NEWTM_ABI_SCALAR) == NEWTM_ABI_OK);
  result = -91;
  assert (GOMP_transaction_stm_v1_read (other, &object, 0, &result, sizeof result)
          == NEWTM_ABI_CONFLICT && result == -91);
  assert (GOMP_transaction_stm_v1_end (&other) == NEWTM_ABI_CONFLICT && other);
  assert (GOMP_transaction_stm_v1_after_failure (&other, 1)
          == NEWTM_ABI_CONFLICT && !other);
  assert (GOMP_transaction_stm_v1_discard (&slot) == NEWTM_ABI_OK && !slot);
  assert (object == 23 && value == 41);
  assert (GOMP_transaction_stm_v1_discard (&slot) == NEWTM_ABI_INVALID);

  start (&slot);
  assert (GOMP_transaction_stm_v1_register (slot, &object, sizeof object,
                                           NEWTM_ABI_SCALAR) == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_register (slot, array, sizeof array,
                                           NEWTM_ABI_AGGREGATE) == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_write (slot, &object, 0, &value, sizeof value)
          == NEWTM_ABI_OK);
  assert (GOMP_transaction_stm_v1_write (slot, array, 0, &value, sizeof value)
          == NEWTM_ABI_OK);
  result = -91;
  assert (GOMP_transaction_stm_v1_read (slot, &object, SIZE_MAX, &result,
                                       sizeof result) == NEWTM_ABI_INVALID);
  assert (result == -91 && object == 23);
  assert (GOMP_transaction_stm_v1_fail (slot, NEWTM_ABI_CONFLICT) == NEWTM_ABI_INVALID);
  assert (GOMP_transaction_stm_v1_end (&slot) == NEWTM_ABI_INVALID && slot);
  assert (object == 23 && array[0] == 1 && array[1] == 23 && array[2] == 3);
  assert (GOMP_transaction_stm_v1_discard (&slot) == NEWTM_ABI_OK && !slot);
  assert (object == 23 && array[0] == 1 && array[1] == 23 && array[2] == 3);

  start (&slot);
  assert (GOMP_transaction_stm_v1_register (slot, &object, sizeof object,
                                           UINT32_MAX) == NEWTM_ABI_INVALID);
  assert (GOMP_transaction_stm_v1_discard (&slot) == NEWTM_ABI_OK && !slot);
  start (&slot);
  assert (GOMP_transaction_stm_v1_fail (slot, NEWTM_ABI_OK) == NEWTM_ABI_INVALID);
  assert (GOMP_transaction_stm_v1_discard (&slot) == NEWTM_ABI_OK && !slot);

  start (&slot);
  assert (GOMP_transaction_stm_v1_nest_end (slot) == NEWTM_ABI_INVALID && slot);
  assert (GOMP_transaction_stm_v1_end (&slot) == NEWTM_ABI_INVALID && slot);
  assert (GOMP_transaction_stm_v1_discard (&slot) == NEWTM_ABI_OK && !slot);
  assert (GOMP_transaction_stm_v1_nest_begin (0) == NEWTM_ABI_INVALID);
  assert (GOMP_transaction_stm_v1_nest_end (0) == NEWTM_ABI_INVALID);

  pid_t child = fork ();
  assert (child >= 0);
  if (!child) fatal_case ();
  int status;
  assert (waitpid (child, &status, 0) == child);
  assert (WIFEXITED (status) && WEXITSTATUS (status) != 0 && WEXITSTATUS (status) != 99);
  puts ("ABI v1 public contracts passed");
  return 0;
}
