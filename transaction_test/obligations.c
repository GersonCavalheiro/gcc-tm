#include <assert.h>
static int global;
static int bump (int *p) { return ++*p; }
static int read_const (const int *p) { return *p; }
static int *advance (int *p) { return p + 1; }
static int combine (int *p)
{ int scratch = 3; *p = bump (&scratch) + read_const (p); return *p; }
#ifndef __cplusplus
struct pair { int x, y; };
static void copy_pair (struct pair *d, const struct pair *s) { *d = *s; }
#endif
static void run (void)
{
  int array[2] = { 1, 2 }, result = -91;
  int *pointer = array;
#ifndef __cplusplus
  struct pair source = { 8, 9 }, destination = { 0, 0 };
#pragma omp transaction(array, result, pointer, global, source, destination)
#else
#pragma omp transaction(array, result, pointer, global)
#endif
  {
    global = combine (pointer);
    pointer = advance (pointer);
    result = read_const (pointer);
#ifndef __cplusplus
    copy_pair (&destination, &source);
#endif
  }
  assert (global == 5 && array[0] == 5 && result == 2 && pointer == array + 1);
#ifndef __cplusplus
  assert (destination.x == 8 && destination.y == 9);
#endif
}
#ifdef REFUSED_CASE
static const int folded = 5;
__attribute__((used)) static void refused (void)
{
#pragma omp transaction(global, folded)
  { global = folded + 1; }
}
#endif
int main (void) { run (); return 0; }
