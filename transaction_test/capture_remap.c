/* Same-typed shared captures must retain distinct origins after outlining. */
#include <assert.h>
int main (void)
{
  int left = 1, right = 2;
#pragma omp parallel num_threads(1) shared(left, right)
  {
#pragma omp transaction(left, right)
    { left += 1; right += 2; }
  }
  assert (left == 2 && right == 4);
  return 0;
}
