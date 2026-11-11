/* { dg-do compile } */
/* { dg-options "-fopenmp -fchecking=2 -fdump-tree-gimple" } */
static int *at (int *p, int n) { return p + n; }
static int *forward (int *p, int n) { return at (p, n); }
int test (void)
{
  int a[3] = {0}, i = 0;
  int *p = a;
  #pragma omp transaction(a,i,p)
  { p = forward (p, i++); *p = 1; }
  return a[0] + i;
}
/* { dg-final { scan-tree-dump "newtm_clone_binding_v1" "gimple" } } */
/* { dg-final { scan-tree-dump "newtm_result_binding" "gimple" } } */
/* { dg-final { scan-tree-dump "newtm_return_binding" "gimple" } } */
/* { dg-final { scan-tree-dump-not "__newtm_publish_result" "gimple" } } */
