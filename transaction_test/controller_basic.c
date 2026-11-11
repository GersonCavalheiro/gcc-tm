#include <assert.h>
static int *forward (int *p) { return p + 1; }
static int bump (int *p) { return ++*p; }
#ifndef __cplusplus
static int captured (int *p)
{
  int local = 3;
  int leaf (void) { return *p + local; }
  int middle (void) { return leaf (); }
  return middle ();
}
#else
template<class T> static T by_reference (T &p) { return ++p; }
#endif
int main (void)
{
  int a[2] = { 4, 5 }, result = -91;
#pragma omp transaction(a, result)
  {
    int *p = forward (a);
    result = bump (p);
#ifndef __cplusplus
    result += captured (a);
#else
    result += by_reference (a[0]) + 2;
#endif
  }
  assert (result == 13 && a[1] == 6);
#pragma omp transaction(result)
  { }
#pragma omp transaction(result)
  { result += 2; }
  assert (result == 15);
  return 0;
}
