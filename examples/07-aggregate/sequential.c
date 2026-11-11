/* Agregado e ranges — version sequential. See index.html. */
#include <stdio.h>
#include <stdlib.h>
struct accounts
{
  int source;
  int destination;
};
static struct accounts accounts =
{
  10000, 2000
};
int main(void)
{
  {
    accounts.source -= 2500;
    accounts.destination += 2500;
  }
  if (accounts.source != 7500 || accounts.destination != 4500 || accounts.source + accounts.destination != 12000) return EXIT_FAILURE;
  printf("RESULT source=%d destination=%d total=%d\n", accounts.source, accounts.destination, accounts.source + accounts.destination);
  return EXIT_SUCCESS;
}
