/* { dg-do run } */
/* { dg-options "-fopenmp -O2" } */

int
main (void)
{
  int value = 3;
  int newtm_nested (int n)
  {
    value += n;
    return value;
  }
  int result = 0;
  #pragma omp transaction (value, result)
  { result = newtm_nested (4); }
  if (result != 7)
    __builtin_abort ();
  return 0;
}
