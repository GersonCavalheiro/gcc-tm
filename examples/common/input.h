#ifndef EXAMPLES_INPUT_H
#define EXAMPLES_INPUT_H
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
static inline int number(const char *text, int minimum, int maximum)
{
  char *end; errno = 0;
  long n = strtol(text, &end, 10);
  if (errno || end == text || *end || n < minimum || n > maximum) {
    fprintf(stderr, "Invalid argument: %s (range %d..%d)\n", text, minimum, maximum);
    exit(EXIT_FAILURE);
  }
  return (int)n;
}
#endif
