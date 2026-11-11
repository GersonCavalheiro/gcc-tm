/* Explicit ABI controller — version gnu_tm. See index.html. */
#include <stdio.h>
#include <stdlib.h>
static int balance = 10000;
int main(void)
{
  __transaction_atomic
  {
    balance += 2500;
  }
  if (balance != 12500) return EXIT_FAILURE;
  printf("RESULT balance=%d\n", balance);
  return EXIT_SUCCESS;
}
