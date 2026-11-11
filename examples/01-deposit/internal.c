/* Deposit — version internal. See index.html. */
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
  value += 2500;
  WRITE(balance, value);
  return NEWTM_LIFECYCLE_OK;
}
int main(void)
{
  execute_body(body, NULL);
  if (balance != 12500) return EXIT_FAILURE;
  printf("RESULT balance=%d\n", balance);
  return EXIT_SUCCESS;
}
