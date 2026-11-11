/* { dg-do compile } */
/* { dg-options "-fopenmp -fchecking=2 -fdump-tree-gimple" } */
template<class T> inline T twice (T n) { return n + n; }
int test (int n)
{
  int result = 0;
  #pragma omp transaction(result,n)
  { result = twice (n); }
  return result;
}
/* { dg-final { scan-tree-dump "\\.newtm\\.v1" "gimple" } } */
/* { dg-final { scan-tree-dump "GOMP_transaction_stm_v1_status" "gimple" } } */
/* { dg-final { scan-tree-dump-not "__newtm_publish_result" "gimple" } } */
