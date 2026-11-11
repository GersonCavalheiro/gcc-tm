/* Incoming slots, aliased program data and paired pointer results. */
#include <assert.h>
static int left[3] = { 3, 5, 7 }, right[2] = { 2, 4 }, observed;
static int *shift (int *p, unsigned n) { return p + n; }
static int *relay (int *p, int *q, unsigned n)
{
  int *value = shift (p, n);
  *value += *q;
  return value;
}
static int scalar (int value) { return value + 1; }
static void empty (void) { }
int main (void)
{
  int local[2] = { 4, 8 }, index = 0;
#pragma omp transaction(left, right, observed, local, index)
  {
    int *p = relay (left, right, 1);
    observed = *p;
    observed += *relay (left, left, 0);
    observed += scalar (2);
    observed += local[index++] - 4;
    empty ();
  }
  assert (left[0] == 6 && left[1] == 7 && right[0] == 2 && observed == 16);
  assert (index == 1 && local[0] == 4);
  return 0;
}
