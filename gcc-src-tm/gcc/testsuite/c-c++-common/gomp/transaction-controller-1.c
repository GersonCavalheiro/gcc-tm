/* { dg-do compile } */
/* { dg-options "-fopenmp -fopenmp-transaction-stm -fdump-tree-gimple" } */
int controller (void)
{
  int value = 0;
#pragma omp transaction(value)
  { value += 1; }
  return value;
}
/* { dg-final { scan-tree-dump "__builtin_GOMP_transaction_stm_v1_begin" "gimple" } } */
/* { dg-final { scan-tree-dump "__builtin_GOMP_transaction_stm_v1_end" "gimple" } } */
/* { dg-final { scan-tree-dump "__builtin_GOMP_transaction_stm_v1_after_failure" "gimple" } } */
/* { dg-final { scan-tree-dump "__builtin_GOMP_transaction_stm_v1_fatal" "gimple" } } */
/* { dg-final { scan-tree-dump-not "__builtin_GOMP_transaction_begin \\(" "gimple" } } */
