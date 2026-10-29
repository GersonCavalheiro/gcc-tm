/* Exemplo STM com uma unica variavel compartilhada entre threads OpenMP.
   O corpo e preparado manualmente: todos os acessos transacionais passam pela
   API interna. Nao ha pragma transaction ou critical protegendo o contador.
   A ligacao usa libgomp.a local para acessar interfaces internas hidden. */
#include "newtm-context.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

/* Acessos e ciclo de vida usam enums distintos. Converter explicitamente
   permite ao callback devolver conflito ou erro permanente ao executor,
   sem depender de coincidencias entre os valores numericos dos enums. */
static enum newtm_lifecycle_status
access_status (enum newtm_access_status status)
{
  switch (status)
    {
    case NEWTM_ACCESS_OK: return NEWTM_LIFECYCLE_OK;
    case NEWTM_ACCESS_CONFLICT: return NEWTM_LIFECYCLE_CONFLICT;
    default: return NEWTM_LIFECYCLE_INVALID;
    }
}

static enum newtm_lifecycle_status
increment (struct newtm_context *ctx, void *argument)
{
  /* O contexto pertence ao executor e e emprestado a esta tentativa.
     O argumento identifica o objeto original; nao e um buffer privado. */
  unsigned long long *counter = argument;
  /* Registrar informa identidade, tamanho e classificacao escalar. Esta
     operacao nao le o contador nem adquire reserva sobre seu valor. */
  enum newtm_lifecycle_status status
    = newtm_register (ctx, counter, sizeof *counter, NEWTM_OBJECT_SCALAR);
  if (status != NEWTM_LIFECYCLE_OK) return status;

  /* O temporario e recriado a cada tentativa. Read obtem reserva de leitura
     e copia a visao privada para value. Se houver conflito, value nao pode ser
     consumido: o retorno imediato leva o executor ao descarte e retry. */
  unsigned long long value;
  enum newtm_access_status access
    = newtm_read (ctx, counter, 0, &value, sizeof value);
  if (access != NEWTM_ACCESS_OK) return access_status (access);
  /* O incremento altera apenas o temporario. Write solicita escrita exclusiva
     (promovendo a reserva de leitura se possivel) e atualiza o buffer privado.
     Outro leitor pode impedir a promocao e causar conflito. A memoria original
     so sera modificada na confirmacao executada por newtm_execute. */
  ++value;
  return access_status (newtm_write (ctx, counter, 0, &value, sizeof value));
}

int
main (void)
{
  unsigned long long counter = 0;
  unsigned int errors = 0;
  int threads = 0;
  const unsigned int repetitions = 100;
  /* A equipe e escolhida por OpenMP, por exemplo via OMP_NUM_THREADS.
     Desabilitar ajuste dinamico facilita conferir o tamanho efetivo da equipe.
     errors e privado por thread e somado pela reduction ao terminar a regiao. */
  omp_set_dynamic (0);
  #pragma omp parallel shared(counter, threads) reduction(+:errors)
  {
    /* Uma thread registra o tamanho da equipe; single tem barreira implicita. */
    #pragma omp single
    threads = omp_get_num_threads ();

    for (unsigned int i = 0; i < repetitions; ++i)
      /* Uma chamada representa um incremento confirmado, mesmo que precise
         de varias tentativas. O executor cria o contexto, invoca increment e,
         em OK, valida/publica e limpa antes de retornar sucesso. Em CONFLICT,
         descarta, espera progressivamente e repete increment desde o inicio.
         INVALID provoca limpeza e diagnostico fatal, sem repetir. Nao acessar
         counter diretamente enquanto outras threads executam transacoes. */
      if (newtm_execute (increment, &counter) != NEWTM_LIFECYCLE_OK)
        ++errors;
  }

  /* O fim da regiao paralela aguarda todas as threads. Agora e seguro ler o
     contador publicado normalmente: cada thread confirmou cem incrementos. */
  unsigned long long expected = (unsigned long long) threads * repetitions;
  /* Encerrar o proprietario da hash somente depois de todas as tentativas
     terminarem. O executor ja liberou reservas, referencias e contextos. */
  if (newtm_hash_owner_close () != NEWTM_HASH_OK || errors || counter != expected)
    {
      fprintf (stderr, "FALHA: contador=%llu esperado=%llu erros=%u\n",
               counter, expected, errors);
      return EXIT_FAILURE;
    }
  printf ("[teste] interno simples OK: threads=%d repeticoes=%u valor=%llu\n",
          threads, repetitions, counter);
  return EXIT_SUCCESS;
}
