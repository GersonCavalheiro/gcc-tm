/* { dg-do run } */
/* { dg-options "-fopenmp -O2 -fno-inline -fdump-tree-gimple -fdump-tree-lower" } */

int combine (int a, int b) { return a * 10 + b; }
int main (void)
{
  int a[2] = { 10, 0 }, i = 0, j = 2, r = 0;
  #pragma omp transaction (a, i, j, r)
  {
    a[i++] += 3;
    r = combine (i++, j++);
  }
  return !(a[0] == 13 && a[1] == 0 && i == 2 && j == 3 && r == 12);
}
/* { dg-final { scan-tree-dump "newtm-bind.*mode=load read-ref=a" "gimple" } } */
/* { dg-final { scan-tree-dump "newtm-bind.*mode=store write-ref=a" "gimple" } } */
/* { dg-final { scan-tree-dump "newtm-bind.*mode=call callee=combine" "gimple" } } */
/* { dg-final { scan-tree-dump-not "newtm-bind" "lower" } } */
/* { dg-final { scan-tree-dump-not "GOMP_transaction_stm|newtm_read|newtm_write" "gimple" } } */
