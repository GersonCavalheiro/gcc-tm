/* Current owning protocol, a nested borrower, and multiple lexical attempts. */
#include <assert.h>
static int total;
static int nested (int *p)
{
#pragma omp transaction(total)
  { ++total; }
  return ++*p;
}
int main (void)
{
  int values[2] = { 3, 5 }, index = 0, result = -1;
  for (int i = 0; i < 2; ++i)
    {
#pragma omp transaction(values, index, result, total)
      { result = nested (&values[index++]); }
    }
#pragma omp transaction(result)
  { result += 2; }
  assert (values[0] == 4 && values[1] == 6 && index == 2);
  assert (result == 8 && total == 2);
  return 0;
}
