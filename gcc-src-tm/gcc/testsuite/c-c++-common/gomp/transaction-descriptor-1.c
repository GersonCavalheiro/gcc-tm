/* Internal descriptors prepare lowering; this still uses the trace bridge. */
/* { dg-do run } */
/* { dg-options "-fopenmp -O0 -fdump-tree-gimple" } */
int main (void)
{
  int a[3] = { 0 }, i = 0;
  short x = 2;
  int *p = &a[0];
#pragma omp transaction (a, i, x, p)
  {
    a[i++] = x;
    *(p + i) = 7;
  }
  return !(i == 1 && a[0] == 2 && a[1] == 7 && p == &a[0]);
}
/* { dg-final { scan-tree-dump "newtm-descriptor role=register.*origin=a class=1" "gimple" } } */
/* { dg-final { scan-tree-dump "newtm-descriptor role=register.*origin=x class=0" "gimple" } } */
/* { dg-final { scan-tree-dump "newtm-descriptor role=write.*state=registered" "gimple" } } */
/* { dg-final { scan-tree-dump "\\.MUL_OVERFLOW" "gimple" } } */
