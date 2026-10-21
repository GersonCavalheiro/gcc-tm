#include <limits.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define N 1000

int
main (void)
{
  int vetor[N];
  for (int i = 0; i < N; ++i)
    vetor[i] = (i * 37 + 19) % 997;

  /* Empate proposital: deve vencer a primeira posicao.  */
  vetor[N / 2] = vetor[N - 1] = 2000;
  int esperado = vetor[0];
  int posicao_esperada = 0;
  for (int i = 1; i < N; ++i)
    if (vetor[i] > esperado)
      {
	esperado = vetor[i];
	posicao_esperada = i;
      }

  int maior = INT_MIN;
  int posicao = N;
  int threads = 0;

  #pragma omp parallel shared(vetor, maior, posicao, threads)
  {
    int maior_local = INT_MIN;
    int posicao_local = N;

    #pragma omp single
    threads = omp_get_num_threads ();

    #pragma omp for schedule(static)
    for (int i = 0; i < N; ++i)
      if (vetor[i] > maior_local
	  || (vetor[i] == maior_local && i < posicao_local))
	{
	  maior_local = vetor[i];
	  posicao_local = i;
	}

    /* transaction ainda nao oferece isolamento.  O critical externo garante
       a sincronizacao deste exemplo na etapa atual de rastreamento.  */
    #pragma omp critical (transaction_exemplo_resultado)
    {
      #pragma omp transaction (maior, posicao)
      {
	if (maior_local > maior
	    || (maior_local == maior && posicao_local < posicao))
	  {
	    maior = maior_local;
	    posicao = posicao_local;
	  }
      }
    }
  }

  if (maior != esperado || posicao != posicao_esperada)
    {
      fprintf (stderr, "FALHA: obtido (%d, %d), esperado (%d, %d)\n",
	       maior, posicao, esperado, posicao_esperada);
      return EXIT_FAILURE;
    }
  printf ("[teste] maximo OK: valor=%d posicao=%d threads=%d\n",
	  maior, posicao, threads);
  return EXIT_SUCCESS;
}
