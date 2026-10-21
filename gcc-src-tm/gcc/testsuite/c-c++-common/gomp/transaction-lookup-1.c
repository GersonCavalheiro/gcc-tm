/* { dg-do compile } */
/* { dg-options "-fopenmp" } */

int value;
void
lookup (int parameter)
{
  double value;
  #pragma omp transaction (value, parameter)
  {}
  {
    int expired;
    (void) expired;
  }
  #pragma omp transaction (expired) /* { dg-error "is not declared|was not declared|has not been declared" } */
  {}
  #pragma omp transaction (later) /* { dg-error "is not declared|was not declared|has not been declared" } */
  { int later; (void) later; }
  #pragma omp transaction (value)
  {}
}
