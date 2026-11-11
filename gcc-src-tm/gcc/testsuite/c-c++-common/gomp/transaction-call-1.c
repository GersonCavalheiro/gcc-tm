/* { dg-do compile } */
/* { dg-options "-fopenmp -fdump-tree-gimple" } */
static int *next (int *p) { return p + 1; }
static int update (int *p) { return ++*next (p); }
void calls (void)
{
  int a[2] = { 1, 2 }, result;
#pragma omp transaction(a, result)
  { result = update (a); }
}
/* Prepared pointer results feed per-activation bindings and checked accesses. */
/* { dg-final { scan-tree-dump "newtm_call_bindings" "gimple" } } */
/* { dg-final { scan-tree-dump "newtm_return_binding" "gimple" } } */
/* { dg-final { scan-tree-dump "__builtin_GOMP_transaction_stm_v1_read" "gimple" } } */
/* { dg-final { scan-tree-dump "__builtin_GOMP_transaction_stm_v1_write" "gimple" } } */
/* { dg-final { scan-tree-dump-not "__newtm_publish_result" "gimple" } } */
