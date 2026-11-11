/* Concurrent counter — version sequential. See index.html. */
#include <stdio.h>
#include <stdlib.h>
#include "input.h"
#include "timing.h"
static int counter;
int main(int argc, char **argv)
{
  if (argc != 1 && argc != 3)
  {
    fprintf(stderr, "Usage: %s [threads operations]\n", argv[0]);
    return EXIT_FAILURE;
  }
  int threads = argc == 3 ? number(argv[1], 1, 8) : 4;
  int operations = argc == 3 ? number(argv[2], 1, 1000000) : 100;
  (void)threads;
  int actual_threads = 1;
  double started = monotonic_seconds();
  for (int i = 0; i < operations; ++i)
  {
    {
      counter += 1;
    }
  }
  double elapsed = monotonic_seconds() - started;
  if (counter != operations) return EXIT_FAILURE;
  printf("RESULT counter=%d operations=%d\n", counter, operations);
  printf("TIMING seconds=%.9f threads=%d\n", elapsed, actual_threads);
  return EXIT_SUCCESS;
}
