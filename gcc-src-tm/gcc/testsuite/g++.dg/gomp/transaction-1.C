/* { dg-do run } */
/* { dg-options "-fopenmp -O2" } */

template <int N>
int
newtm_template ()
{
  int value = 0;
  #pragma omp transaction (value)
  {
    int local = 11;
    value += local;
    #pragma omp transaction
    {
      value += N;
    }
  }
  return value;
}

int
main ()
{
  if (newtm_template<5> () != 16)
    __builtin_abort ();
  return 0;
}
