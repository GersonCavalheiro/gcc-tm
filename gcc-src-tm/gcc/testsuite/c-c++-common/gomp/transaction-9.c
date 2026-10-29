/* { dg-do run } */
/* { dg-options "-fopenmp" } */
/* { dg-prune-output ".*newtm.* (Inicio|Fim) do tratamento das variaveis da transacao" } */

#define TRANSACTION_NAMES alpha, beta, gamma

int
main (void)
{
  int result = 0;
  int single = 0, alpha = 0, beta = 0, gamma = 0, delta = 0, epsilon = 0;
  #pragma omp transaction (single, result)
  { result++; }
  #pragma omp transaction (TRANSACTION_NAMES, result)
  {
    #pragma omp transaction (alpha, alpha)
    { result += 2; }
  }
  _Pragma ("omp transaction (delta, epsilon, result)")
  { result += 4; }
  #pragma omp transaction
  { int private_value = 8; (void) private_value; }
  if (result != 7)
    __builtin_abort ();
  return 0;
}
