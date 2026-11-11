/* Prepared calls and nested captures must use each outlined activation. */
#include <assert.h>
static int counter;
static int bump (int *p) { return ++*p; }
static int *advance (int *p, int n) { return p + n; }
static void nested (void)
{
#pragma omp transaction(counter)
  { counter += 1; }
}
#ifdef __cplusplus
template<class T> static int captured (T &value) { return bump (&value) + 2; }
#else
static int captured (int *p)
{
  int extra = 2;
  int leaf (void) { return bump (p) + extra; }
  int middle (void) { return leaf (); }
  return middle ();
}
#endif
int main (void)
{
  int sum = 0;
#pragma omp parallel num_threads(4) reduction(+:sum)
  {
    for (int n = 0; n < 32; ++n)
      {
        int local[3] = { 1, 2, 3 }, index = 0, result = -91;
        int *pointer = local;
#pragma omp transaction(counter, local, index, result, pointer)
        {
          int scratch = 3;
          int *q = advance (pointer, index++);
          nested ();
#ifdef __cplusplus
          result = captured (*q) + bump (&scratch);
#else
          result = captured (q) + bump (&scratch);
#endif
          pointer = advance (q, 1);
        }
        assert (local[0] == 2 && local[1] == 2 && local[2] == 3);
        assert (index == 1 && result == 8 && pointer == local + 1);
        sum += result + index;
      }
  }
  assert (counter == 128 && sum == 1152);
  int ordinary = 1;
#ifdef __cplusplus
  assert (captured (ordinary) == 4 && ordinary == 2);
#else
  assert (captured (&ordinary) == 4 && ordinary == 2);
#endif
  return 0;
}
