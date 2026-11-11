/* Section 9: exact side effects, derived store and aggregate copy. */
#include <assert.h>
static int touch (int *p) { ++*p; return *p; }
static int callee (int value) { return value + 7; }
int main (void)
{
  int a[2] = { 10, 20 }, i = 0, result = -1;
#pragma omp transaction(a, i, result)
  {
    int local_privado = 2;
    a[i++] += touch (&local_privado);
    result = local_privado;
  }
  assert (i == 1 && a[0] == 13 && a[1] == 20 && result == 3);
  int source = 5, destination = -1;
  int *destino = &destination;
#pragma omp transaction(source, destination, destino)
  { *destino = callee (source); }
  assert (source == 5 && destination == 12);
#ifndef __cplusplus
  struct pair { int x, y; } original = { 8, 9 }, copy = { -1, -2 };
  struct pair *origem = &original, *target = &copy;
#pragma omp transaction(original, copy, origem, target)
  { *target = *origem; }
  assert (original.x == 8 && original.y == 9 && copy.x == 8 && copy.y == 9);
#endif
  return 0;
}
