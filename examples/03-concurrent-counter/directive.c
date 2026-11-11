/* Concurrent counter — version directive. See index.html. */
#include <stdio.h>
#include <stdlib.h>
#include "input.h"
#include "timing.h"
#include <omp.h>
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
  omp_set_dynamic(0);
  int actual_threads = 0;
  double started = monotonic_seconds();
#pragma omp parallel num_threads(threads)
  {
#pragma omp single
    actual_threads = omp_get_num_threads();
#pragma omp for
  for (int i = 0; i < operations; ++i)
  {
#pragma omp transaction (counter)
    {
      counter += 1;
    }
  }
  }
  double elapsed = monotonic_seconds() - started;
  if (actual_threads != threads) { fputs("Requested team size was not granted.\n", stderr); return EXIT_FAILURE; }
  if (counter != operations) return EXIT_FAILURE;
  printf("RESULT counter=%d operations=%d\n", counter, operations);
  printf("TIMING seconds=%.9f threads=%d\n", elapsed, actual_threads);
  return EXIT_SUCCESS;
}
