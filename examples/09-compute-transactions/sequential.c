/* Long transactions: reserve a shared scalar before substantial private work.
   A candidate for eager-conflict detection, not a demonstrated performance win. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "input.h"
#include "timing.h"
static uint32_t state = 1;
/* Unsigned arithmetic has defined wraparound; every iteration affects output. */
static uint32_t mix(uint32_t value, unsigned steps)
{
  for (unsigned i = 0; i < steps; ++i) {
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    value += UINT32_C(0x9e3779b9);
  }
  return value;
}
static void update(unsigned steps)
{
  {
    /* Copy the preexisting input once into an attempt-private temporary. */
    unsigned work = steps;
    state ^= 1u;
    uint32_t value = state;
    value = mix(value, work);
    state = value;
  }
}
int main(int argc, char **argv)
{
  if (argc != 1 && argc != 4) {
    fprintf(stderr, "Usage: %s [threads transactions compute_steps]\n", argv[0]);
    return EXIT_FAILURE;
  }
  int threads = argc == 4 ? number(argv[1], 1, 8) : 4;
  int transactions = argc == 4 ? number(argv[2], 1, 10000) : 128;
  unsigned steps = argc == 4 ? (unsigned)number(argv[3], 1, 1000000) : 4096;
  /* This bound keeps validation and accidental invocations tractable. */
  if ((unsigned long long)transactions * steps > 100000000ULL) {
    fputs("transactions * compute_steps must not exceed 100000000.\n", stderr);
    return EXIT_FAILURE;
  }
  int actual_threads = 1;
  double started = monotonic_seconds();
  (void)threads;
  for (int i = 0; i < transactions; ++i)
    update(steps);
  double elapsed = monotonic_seconds() - started;
  /* Independent serial recurrence, outside the measured interval. */
  uint32_t expected = 1;
  for (int i = 0; i < transactions; ++i)
    expected = mix(expected ^ 1u, steps);
  if (state != expected) return EXIT_FAILURE;
  printf("RESULT transactions=%d compute_steps=%u state=%u\n", transactions, steps, (unsigned)state);
  printf("TIMING seconds=%.9f threads=%d\n", elapsed, actual_threads);
  return EXIT_SUCCESS;
}
