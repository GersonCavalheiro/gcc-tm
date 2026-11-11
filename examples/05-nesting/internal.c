/* Flat nesting — version internal. See index.html. */
#include <stdio.h>
#include <stdlib.h>
#include "internal.h"
static int balance = 10000;
static enum newtm_lifecycle_status body(struct newtm_context *ctx, void *argument)
{
  (void)argument;
  REG(balance);
  int value;
  READ(balance, value);
  value += 1000;
  WRITE(balance, value);
  if (GOMP_transaction_stm_v1_nest_begin(ctx) != NEWTM_ABI_OK) return NEWTM_LIFECYCLE_INVALID;
  READ(balance, value);
  value += 1500;
  WRITE(balance, value);
  if (GOMP_transaction_stm_v1_nest_end(ctx) != NEWTM_ABI_OK) return NEWTM_LIFECYCLE_INVALID;
  return NEWTM_LIFECYCLE_OK;
}
int main(void)
{
  execute_body(body, NULL);
  if (balance != 12500) return EXIT_FAILURE;
  printf("RESULT balance=%d\n", balance);
  return EXIT_SUCCESS;
}
