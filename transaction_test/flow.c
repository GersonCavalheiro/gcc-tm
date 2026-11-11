/* Real loops, switches, early returns and lifetime cleanups in prepared code. */
#include <assert.h>
static int step (int *p, int n)
{
  int scratch[3] = { 1, 2, 3 }, sum = 0;
  for (int i = 0; i < n; ++i)
    {
      if (i == 1) continue;
      switch (i % 3)
        {
        case 0: sum += *p; break;
        case 1: sum += scratch[1]; break;
        default: sum += scratch[2]; break;
        }
      if (sum > 17) return sum;
    }
  *p += sum;
  return *p;
}
int main (void)
{
  for (int n = 0; n <= 9; ++n)
    {
      int object = 2, reference = 2, result = -91;
      int expected = step (&reference, n);
#pragma omp transaction(object, result, n)
      { result = step (&object, n); }
      assert (object == reference && result == expected);
    }
  return 0;
}
