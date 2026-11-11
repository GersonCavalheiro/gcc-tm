#include <assert.h>
int shared_values[2] = { 3, 5 }, result;
unsigned unrelated __attribute__((used)) = 9;
int unrelated_array[2] __attribute__((used));
static int foreign (int x) __attribute__((used));
static int foreign (int x) { return x + 1; }
static int step (int *p) { return ++*p; }
static int relay (int *p) { return step (p); }
int main (void)
{
  ++unrelated; /* Ordinary code outside the controller remains ordinary. */
#pragma omp transaction(shared_values, result)
  { result = relay (shared_values); }
  ++unrelated;
  assert (shared_values[0] == 4 && shared_values[1] == 5 && result == 4);
  assert (unrelated == 11);
  return 0;
}
