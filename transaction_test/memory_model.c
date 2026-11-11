/* Dynamic identities, distinct slots and private objects through real clones. */
#include <assert.h>
struct pair { int first, second; };
static int left[4] = { 3, 5, 7, 9 }, right[4] = { 2, 4, 6, 8 };
static int unrelated[4] __attribute__((used));
static int result = -91;
static struct pair shared_pair = { 1, 2 };
static int transfer (int *p, int *q, unsigned index)
{
  p[index] += q[0];
  return p[index];
}
static int *step (int *p, unsigned index) { return p + index; }
static void dynamic_registration_root (void)
{
  int array[2] = { 1, 2 }, observed = -1;
#pragma omp transaction(array, observed)
  { array[1] += array[0]; observed = array[1]; }
  assert (array[0] == 1 && array[1] == 3 && observed == 3);
}
static int private_pair (void)
{
  int a[2] = { 10, 20 }, b[2] = { 1, 2 };
  int *p = step (a, 1);
  return transfer (p, b, 0) + a[0];
}
static int private_zero (void)
{
  int array[4] = { 0 };
  return transfer (step (array, 2), array, 0) + array[3];
}
static double real_value (void)
{
  double array[2] = { 1.5, 2.5 };
  return array[0] + array[1] - 2.0;
}
static int conditional_index (int choose)
{
  unsigned index;
  if (choose) index = 0;
  else index = 1;
  return left[index];
}
static int switch_index (int choose)
{
  unsigned index;
  switch (choose)
    {
    case 0: index = 0; break;
    case 1: index = 1; break;
    default: index = 2; break;
    }
  return left[index];
}
int main (void)
{
#ifndef __cplusplus
#pragma omp transaction(left, right, result, shared_pair)
#else
#pragma omp transaction(left, right, result)
#endif
  {
    result = transfer (left, right, 2);
    result += private_pair ();
    result += private_zero ();
    result += (int) real_value () - 2;
    result += conditional_index (1) - 3;
    result += switch_index (0) - 3;
#ifndef __cplusplus
    shared_pair.second += result;
#endif
  }
  assert (left[2] == 9 && left[0] == 3 && right[0] == 2 && result == 40);
#ifndef __cplusplus
  assert (shared_pair.first == 1 && shared_pair.second == 42);
#endif
  dynamic_registration_root ();
  return 0;
}
