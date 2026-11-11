/* { dg-do compile } */
/* { dg-options "-fopenmp -fdump-tree-omplower" } */
/* { dg-prune-output ".*newtm.*" } */

template<typename T>
void trace (T &object)
{
  #pragma omp transaction (object)
  { ++object; }
}

void instantiate (int &x) { trace (x); }

/* The reference is evaluated into a temporary before registration. */
/* { dg-final { scan-tree-dump {object\.[0-9]+_[0-9]+ = object;} "omplower" } } */
/* { dg-final { scan-tree-dump-times {GOMP_transaction_register \(object\.[0-9]+_[0-9]+,} 1 "omplower" } } */
