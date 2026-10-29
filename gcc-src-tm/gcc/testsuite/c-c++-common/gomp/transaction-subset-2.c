/* { dg-do run } */
/* { dg-options "-fopenmp -O2 -fdump-tree-gimple" } */

int next (int);
int main (void)
{
  int a[2] = { 0, 0 }, i = 0;
  #pragma omp transaction (a, i)
  { a[i++] = next (4); a[i++] = a[0] + 1; }
  return !(i == 2 && a[0] == 5 && a[1] == 6);
}
int next (int n) { return n + 1; }
/* { dg-final { scan-tree-dump "canonical-plan:.*kind=1" "gimple" } } */
/* { dg-final { scan-tree-dump "prepare\\(next\\)" "gimple" } } */
/* { dg-final { scan-tree-dump-not "GOMP_transaction_stm|newtm_read|newtm_write" "gimple" } } */
