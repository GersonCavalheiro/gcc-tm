#include <assert.h>
struct pair { int x, y; };
static int shared = 5, result;
static int transfer (int *p, int *q) { *p += *q; return *p; }
static int relay (int *p) { return *p + 1; }
static void copy_pair (struct pair *destino, struct pair *origem)
{ *destino = *origem; }
int main (void)
{
  struct pair source = { 8, 9 }, destination = { 0, 0 };
#ifndef __cplusplus
#pragma omp transaction(shared, result, source, destination)
#else
#pragma omp transaction(shared, result)
#endif
  {
    int local_privado = 2;
    result = transfer (&local_privado, &shared);
    result += relay (&shared);
#ifndef __cplusplus
    copy_pair (&destination, &source);
#endif
  }
  assert (shared == 5 && result == 13);
#ifndef __cplusplus
  assert (destination.x == 8 && destination.y == 9);
#endif
  return 0;
}
