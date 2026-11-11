/* { dg-do compile } */
/* { dg-options "-fopenmp -fchecking=2 -fdump-tree-gimple" } */
static int local_write (int n)
{
  int a[2] = { n, 0 };
  a[1] = n + 1;
  return a[0] + a[1];
}
void select_private_write (void)
{
  int result;
#pragma omp transaction(result)
  { result = local_write (3); }
}
/* Only the external result store uses STM. */
/* { dg-final { scan-tree-dump-times "__builtin_GOMP_transaction_stm_v1_write \\(" 1 "gimple" } } */
