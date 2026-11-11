#include <assert.h>
__attribute__((used, noinline)) static int unprepared_dependency (int n)
{ return n + 17; }
static int increment (int n) { return n + 1; }
int main (void)
{
  int value = 4;
#pragma omp transaction(value)
  { value = increment (value); }
  assert (value == 5);
  return 0;
}
