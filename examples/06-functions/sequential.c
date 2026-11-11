/* Helper functions — version sequential. See index.html. */
#include <stdio.h>
#include <stdlib.h>
static int balance = 10000;
static void deposit(void)
{
  balance += 2500;
}
int main(void)
{
  {
    deposit();
  }
  if (balance != 12500) return EXIT_FAILURE;
  printf("RESULT balance=%d\n", balance);
  return EXIT_SUCCESS;
}
