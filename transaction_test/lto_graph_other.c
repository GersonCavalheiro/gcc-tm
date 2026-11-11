/* An independent root from another object, with colliding local source ids. */
static int sequential (int n) { return n + 1; }
__attribute__((noinline)) int ordinary (int n) { volatile int value = n; return value; }
int other_unit (void)
{
  int value = 0;
#pragma omp transaction(value)
  { value = sequential (value); }
  return value + ordinary (0);
}
