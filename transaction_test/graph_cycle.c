/* Mutual recursion must be certified as a whole. */
#include <assert.h>
static int other (int);
static int recurse (int n) { return n ? other (n - 1) + 1 : 0; }
static int other (int n) { return n ? recurse (n - 1) + 1 : 0; }
int main (void)
{
  int result = -1, n = 5;
#pragma omp transaction(result, n)
  { result = recurse (n); }
  assert (result == n);
  return 0;
}
