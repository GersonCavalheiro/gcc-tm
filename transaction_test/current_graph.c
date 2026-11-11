/* Two roots share a recursive SCC; a third root is independent. */
#include <assert.h>
unsigned unrelated __attribute__((used));
static int other (int);
static int recurse (int n) { return n ? other (n - 1) + 1 : 0; }
static int other (int n) { return n ? recurse (n - 1) + 1 : 0; }
static int separate (void)
{
  int value = 0;
#pragma omp transaction(value)
  { ++value; }
  return value;
}
int main (void)
{
  int a = -1, b = -1, n = 5;
#pragma omp transaction(a, n)
  { a = recurse (n); }
#pragma omp transaction(b, n)
  { b = recurse (n); }
  assert (a == 5 && b == 5 && separate () == 1 && !unrelated);
  return 0;
}
