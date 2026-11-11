#include <assert.h>
static void execute (int &value)
{
#pragma omp transaction(value)
  { value += 2; }
}
int main ()
{
  int value = 3;
  execute (value);
  assert (value == 5);
}
