/* A newly inserted READ has no source operation, even in a previously pure clone. */
#include <assert.h>
static int shared_value;
static int pure (int n) { return n + 1; }
int main (void)
{
  int result = 0;
#pragma omp transaction(result)
  { result = pure (3); }
  assert (result == 4 && shared_value == 0);
  return 0;
}
