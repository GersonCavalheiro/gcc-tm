/* Helper functions — version gnu_tm. See index.html. */
#include <stdio.h>
#include <stdlib.h>
static int balance = 10000;
static void deposit(void) __attribute__((transaction_safe));
static void deposit(void)
{
  balance += 2500;
}
int main(void)
{
  __transaction_atomic
  {
    deposit();
  }
  if (balance != 12500) return EXIT_FAILURE;
  printf("RESULT balance=%d\n", balance);
  return EXIT_SUCCESS;
}
