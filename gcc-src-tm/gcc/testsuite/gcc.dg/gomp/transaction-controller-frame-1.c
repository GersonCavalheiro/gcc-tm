/* { dg-do compile } */
/* { dg-options "-fopenmp -fopenmp-transaction-stm" } */
static int global;
void controller (void)
{
  int value = 4;
  int leaf (void) { return value; }
#pragma omp transaction(global, value) /* { dg-error "prepared nested call requires a different lexical frame" } */
  { global = leaf (); }
}
