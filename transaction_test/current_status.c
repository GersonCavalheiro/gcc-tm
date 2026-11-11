/* Native status gates, independent value/binding channels and nesting. */
#include <assert.h>
static int shared[3] = { 3, 5, 7 }, other[2] = { 2, 4 }, observed, counter;
static int *advance (int *p, unsigned n) { return p + n; }
static int *relay (int *p, int *q, unsigned n)
{
  int *value = advance (p, n);
  *value += *q;
  return value;
}
static int nested (int *p)
{
#pragma omp transaction(counter)
  { ++counter; }
  return ++*p;
}
static int scalar (int value) { return value + 1; }
static int pure_read (int *p) { return *p; }
int main (void)
{
  int local[2] = { 4, 8 }, index = 0;
#pragma omp transaction(shared, other, observed, counter, local, index)
  {
    int private_array[2] = { 1, 2 };
    observed = *relay (shared, other, 1);
    observed += nested (private_array);
    observed += scalar (2);
    observed += pure_read (other) - 2;
    observed += local[index++] - 4;
  }
  assert (observed == 12 && shared[1] == 7 && shared[0] == 3);
  assert (counter == 1 && index == 1 && local[0] == 4);
  return 0;
}
