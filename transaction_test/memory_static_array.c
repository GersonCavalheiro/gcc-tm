/* The current offset must be in the static array even after transformation. */
#include <assert.h>
static int values[4] = { 1, 2, 3, 4 };
static int element (int index) { return values[index]; }
int main (void)
{
  int result = 0;
#pragma omp transaction(values, result)
  { result = element (2); }
  assert (result == 3);
  return 0;
}
