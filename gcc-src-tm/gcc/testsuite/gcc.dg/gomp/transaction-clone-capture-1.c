/* { dg-do compile } */
/* { dg-options "-fopenmp -fchecking=2 -fdump-tree-gimple" } */
static int outer (int n)
{
  int value = n, *p = &value;
  int inner (int x) { return *p + x; }
  return inner (2);
}
int test (void)
{
  int result = 0;
  #pragma omp transaction(result)
  { result = outer (3); }
  return result;
}
/* { dg-final { scan-tree-dump "inner.*\\.newtm\\.v1" "gimple" } } */
/* { dg-final { scan-tree-dump "newtm_clone_binding_v1" "gimple" } } */
/* { dg-final { scan-tree-dump-not "__newtm_publish_result" "gimple" } } */
