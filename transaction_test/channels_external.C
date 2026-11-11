/* Exceptions around a transaction do not extend its prepared subset. */
#include <assert.h>
static int value, observed;
__attribute__((noinline)) static void ordinary (bool fail)
{ if (fail) throw 17; }
int main (void)
{
  int array[2] = { 3, 4 }, index = 0;
  try { ordinary (true); }
  catch (int n) { assert (n == 17); value = n; }
#pragma omp transaction(value, observed, array, index)
  { value += array[index++]; observed = value; }
  try { ordinary (true); }
  catch (int n) { assert (n == 17 && observed == 20); }
  assert (index == 1 && value == 20 && array[0] == 3);
  return 0;
}
