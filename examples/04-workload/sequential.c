/* Read/write workload — version sequential. See index.html. */
#include <stdio.h>
#include <stdlib.h>
#include "input.h"
#include "timing.h"
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
  (void)threads;
  int actual_threads = 1;
  double started = monotonic_seconds();
  for (int i = 0; i < operations; ++i)
  {
    int writing = ((long long)(i + 1) * write_weight / weight_sum != (long long)i * write_weight / weight_sum);
    if (writing)
    {
      value += 1;
    }
    else
    {
      int observed = 0;
      {
        observed = value;
      }
      if (observed < 0 || observed > expected_writes) ++errors;
    }
  }
  double elapsed = monotonic_seconds() - started;
  if (errors || value != expected_writes) return EXIT_FAILURE;
  printf("RESULT operations=%d writes=%d reads=%d value=%d\n", operations, expected_writes, operations - expected_writes, value);
  printf("TIMING seconds=%.9f threads=%d\n", elapsed, actual_threads);
  return EXIT_SUCCESS;
}
