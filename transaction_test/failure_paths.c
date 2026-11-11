/* Balanced nesting and void prepared helpers exercise the complete inspector. */
#include <assert.h>
static int counter;
static void increment (int *p) { *p += 1; }
static int nested (int *p, int n)
{
  for (int i = 0; i < n; ++i)
    {
#pragma omp transaction(counter)
      { counter += 1; }
      increment (p);
    }
  return *p;
}
int main (void)
{
  for (int n = 0; n < 5; ++n)
    {
      int value = 3, result = -1;
      counter = 0;
#pragma omp transaction(value, result, n)
      { result = nested (&value, n); }
      assert (value == 3 + n && result == value && counter == n);
    }
  return 0;
}
