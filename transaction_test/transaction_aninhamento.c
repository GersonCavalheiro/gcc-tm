#include <stdio.h>
#include <stdlib.h>

static int saldo = 10;
static int total = 0;

static void
atualizar (void)
{
  /* Aninhamento dinamico: esta funcao participa do contexto ja ativo.  */
  #pragma omp transaction (saldo, total)
  {
    saldo += 4;
    total += 5;
  }
}

int
main (void)
{
  #pragma omp transaction (saldo, saldo)
  {
    ++saldo;
    /* A lista interna acrescenta total; saldo ja esta registrado.  */
    #pragma omp transaction (saldo, total)
    {
      saldo += 2;
      total += 3;
      atualizar ();
    }
  }
  puts ("[teste] regiao externa encerrada");

  if (saldo != 17 || total != 8)
    return EXIT_FAILURE;
  puts ("[teste] aninhamento OK: saldo=17 total=8");
  return EXIT_SUCCESS;
}
