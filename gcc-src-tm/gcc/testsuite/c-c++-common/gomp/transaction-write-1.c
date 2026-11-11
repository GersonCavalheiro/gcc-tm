/* { dg-do compile } */
/* { dg-options "-fopenmp -fchecking=2 -fdump-tree-gimple" } */
static void store (int *p, int n) { *p = n; }
void select_writes (void)
{
  int value = 0;
#pragma omp transaction(value)
  { store (&value, 7); }
}
/* { dg-final { scan-tree-dump-times "__builtin_GOMP_transaction_stm_v1_write \\(" 1 "gimple" } } */
/* { dg-final { scan-tree-dump-not "__newtm_publish_result" "gimple" } } */
