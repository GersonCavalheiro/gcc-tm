/* Static identities must survive remapping independently of matching guards. */
#include <assert.h>
static int shared_value, other_value;
static void update (int count)
{
  for (int i = 0; i < count; ++i) shared_value += i + 1;
}
int main (void)
{
#pragma omp transaction(shared_value)
  { update (2); }
  assert (shared_value == 3 && other_value == 0);
  return 0;
}
