/* { dg-do compile } */
/* { dg-options "-fopenmp -fchecking=2 -fdump-tree-gimple" } */
static int private_list (int n)
{
  int value[1] = { n };
  return value[0];
}
void select_private (void)
{
  int result;
#pragma omp transaction(result)
  { result = private_list (3); }
}
/* { dg-final { scan-tree-dump-not "__builtin_GOMP_transaction_stm_v1_read" "gimple" } } */
