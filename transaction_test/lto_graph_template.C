#include <assert.h>
template<class T> T owner (T value)
{
#pragma omp transaction(value)
  { ++value; }
  return value;
}
__attribute__((noinline)) int ordinary (int n) { volatile int x = n; return x; }
int main () { assert (owner (2) == 3 && ordinary (0) == 0); }
