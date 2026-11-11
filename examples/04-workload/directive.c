/* Read/write workload — version directive. See index.html. */
#include <stdio.h>
#include <stdlib.h>
#include "input.h"
#include "timing.h"
#include <omp.h>
static int value;
int main(int argc, char **argv)
{
  if (argc != 1 && argc != 5)
  {
    fprintf(stderr, "Usage: %s [threads operations write_weight read_weight]\n", argv[0]);
    return EXIT_FAILURE;
  }
  int threads = argc == 5 ? number(argv[1], 1, 8) : 4;
  int operations = argc == 5 ? number(argv[2], 1, 1000000) : 400;
  int write_weight = argc == 5 ? number(argv[3], 0, 1000) : 1;
  int read_weight = argc == 5 ? number(argv[4], 0, 1000) : 9;
  int weight_sum = write_weight + read_weight;
  if (!weight_sum)
  {
    fputs("The weights must not both be zero.\n", stderr);
    return EXIT_FAILURE;
  }
  int expected_writes = (int)((long long)operations * write_weight / weight_sum);
  int errors = 0;
  omp_set_dynamic(0);
  int actual_threads = 0;
  double started = monotonic_seconds();
#pragma omp parallel num_threads(threads)
  {
#pragma omp single
    actual_threads = omp_get_num_threads();
#pragma omp for reduction(+:errors)
  for (int i = 0; i < operations; ++i)
  {
    int writing = ((long long)(i + 1) * write_weight / weight_sum != (long long)i * write_weight / weight_sum);
    if (writing)
#pragma omp transaction (value)
    {
      value += 1;
    }
    else
    {
      int observed = 0;
#pragma omp transaction (value, observed)
      {
        observed = value;
      }
      if (observed < 0 || observed > expected_writes) ++errors;
    }
  }
  }
  double elapsed = monotonic_seconds() - started;
  if (actual_threads != threads) { fputs("Requested team size was not granted.\n", stderr); return EXIT_FAILURE; }
  if (errors || value != expected_writes) return EXIT_FAILURE;
  printf("RESULT operations=%d writes=%d reads=%d value=%d\n", operations, expected_writes, operations - expected_writes, value);
  printf("TIMING seconds=%.9f threads=%d\n", elapsed, actual_threads);
  return EXIT_SUCCESS;
}
