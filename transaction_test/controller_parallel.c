/* Generated control and explicit captures must survive OpenMP outlining. */
#include <assert.h>
static int counter;
int main (void)
{
  int sum = 0;
#pragma omp parallel num_threads(4) reduction(+:sum)
  {
    for (int n = 0; n < 40; ++n)
      {
        int local[2] = { 1, 2 }, index = 0;
#pragma omp transaction(counter, local, index)
        { int scratch = 2; counter += 1; local[index++] += scratch; }
        assert (index == 1 && local[0] == 3 && local[1] == 2);
        sum += local[0] + index;
      }
  }
  assert (counter == 160 && sum == 640);
  return 0;
}
