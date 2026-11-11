/* { dg-do compile } */
/* { dg-options "-fopenmp -fchecking=2 -fdump-tree-gimple" } */
/* Prepared functions are emitted; the pragma still uses the tracing bridge. */
static int plus_one (int n) { return n + 1; }
static int chain (int n) { return plus_one (n) + 1; }
int test (int n)
{
  int result = 0;
  #pragma omp transaction(result,n)
  { result = chain (n); }
  return result;
}
/* { dg-final { scan-tree-dump "\\.newtm\\.v1" "gimple" } } */
/* { dg-final { scan-tree-dump "GOMP_transaction_stm_v1_status" "gimple" } } */
/* { dg-final { scan-tree-dump "GOMP_transaction_begin" "gimple" } } */
/* { dg-final { scan-tree-dump-not "__newtm_publish_result" "gimple" } } */
