/* Transfer — version directive. See index.html. */
#include <stdio.h>
#include <stdlib.h>
static int source = 10000, destination = 2000;
int main(void)
{
#pragma omp transaction (source, destination)
  {
    if (source >= 2500)
    {
      source -= 2500;
      destination += 2500;
    }
  }
  if (source != 7500 || destination != 4500 || source + destination != 12000) return EXIT_FAILURE;
  printf("RESULT source=%d destination=%d total=%d\n", source, destination, source + destination);
  return EXIT_SUCCESS;
}
