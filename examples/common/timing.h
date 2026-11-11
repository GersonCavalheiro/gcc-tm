#ifndef EXAMPLES_TIMING_H
#define EXAMPLES_TIMING_H
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
/* Monotonic wall time: team startup, transaction work and final barrier included. */
static inline double monotonic_seconds(void)
{
  struct timespec t;
  if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
    perror("clock_gettime");
    exit(EXIT_FAILURE);
  }
  return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
#endif
