/* Registration in a prepared nested transaction, including a CFG back edge. */
#include <assert.h>
static int shared_value;
static int other_value;
static void update (int count)
{
  for (int i = 0; i < count; ++i)
    {
#pragma omp transaction(shared_value)
      { shared_value += 1; }
    }
}
int main (void)
{
#pragma omp transaction(shared_value)
  { update (2); }
  assert (shared_value == 2 && other_value == 0);
  return 0;
}
