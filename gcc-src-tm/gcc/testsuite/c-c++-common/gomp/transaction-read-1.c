/* { dg-do compile } */
/* { dg-options "-fopenmp -fchecking=2 -fdump-tree-gimple" } */
static int load_twice (const int *p)
{
  int first = *p;
  int second = *p;
  return first + second;
}
void select_reads (void)
{
  int a[2] = { 1, 2 }, result;
#pragma omp transaction(a, result)
  { result = load_twice (a); }
}
/* { dg-final { scan-tree-dump-times "__builtin_GOMP_transaction_stm_v1_read \\(" 2 "gimple" } } */
/* { dg-final { scan-tree-dump-not "__newtm_publish_result" "gimple" } } */
