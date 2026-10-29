/* Equivalente STM de transaction_carga.c, usando diretamente a API interna.
   Recebe THREADS ESCRITAS:LEITURAS REPETICOES e mantem inverse == ~value.
   Corpos preparados manualmente usam reservas, visao privada, commit e retry
   externo, sem critical. Compilar/ligar pelo alvo exemplos-internos. */
#include "newtm-context.h"
#include <stddef.h>
#include <limits.h>
#include <omp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Os dois campos formam um unico objeto registrado e uma unidade de conflito.
   Uma transacao leitora deve observar o par coerente, nunca metade de uma
   atualizacao concorrente. Os intervalos privados continuam separados por campo. */
struct shared_state
{
  unsigned long long value;
  unsigned long long inverse;
};

/* Entrada de controle emprestada, estavel durante toda a chamada ao executor.
   Retry conserva state/writing; nao ha saidas especulativas neste descritor.
   Os resultados transacionais sao publicados apenas no objeto registrado. */
struct operation
{
  struct shared_state *state;
  bool writing;
};

/* Traduzir entre os enums distintos de acesso e ciclo de vida. O callback
   propaga a primeira falha antes de consumir resultados ou continuar o corpo. */
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
operate (struct newtm_context *ctx, void *argument)
{
  /* Cada invocacao e uma tentativa completa, com contexto novo fornecido pelo
     executor. O callback nao confirma nem descarta esse contexto por conta propria. */
  const struct operation *op = argument;
  /* Registrar o agregado inteiro uma vez. Kind nao e inferido pelo tamanho;
     registro nao carrega campos nem obtem reservas de leitura/escrita. */
  enum newtm_lifecycle_status status
    = newtm_register (ctx, op->state, sizeof *op->state, NEWTM_OBJECT_AGGREGATE);
  if (status != NEWTM_LIFECYCLE_OK) return status;

  /* offsetof fornece deslocamentos em bytes a partir do objeto canonico.
     Cada read valida a faixa e copia apenas o campo pedido para um temporario.
     A primeira leitura obtem reserva compartilhada sobre o objeto; a segunda
     reutiliza essa reserva. As listas de intervalos marcam os bytes validos,
     sem exigir leitura de padding ou do restante do agregado. */
  unsigned long long observed, inverse;
  enum newtm_access_status access
    = newtm_read (ctx, op->state, offsetof (struct shared_state, value),
                  &observed, sizeof observed);
  if (access != NEWTM_ACCESS_OK) return access_status (access);
  access = newtm_read (ctx, op->state, offsetof (struct shared_state, inverse),
                       &inverse, sizeof inverse);
  if (access != NEWTM_ACCESS_OK) return access_status (access);
  if (inverse != ~observed)
    /* Par incoerente e erro permanente, nao conflito recuperavel. Marcar INVALID
       permite ao executor limpar e so depois emitir o diagnostico fatal.
       Nao fazer E/S nem incrementar contador externo dentro da tentativa. */
    return newtm_fail (ctx, NEWTM_ACCESS_INVALID);

  if (op->writing)
    {
      /* Calcular em temporarios nao altera o estado compartilhado. A primeira
         escrita tenta promover a reserva para exclusiva; leitores concorrentes
         podem causar conflito e reinicio de todo o callback. */
      ++observed;
      inverse = ~observed;
      access = newtm_write (ctx, op->state, offsetof (struct shared_state, value),
                            &observed, sizeof observed);
      if (access != NEWTM_ACCESS_OK) return access_status (access);
      /* A primeira escrita ja esta no buffer privado. Se esta segunda operacao
         falhar, o retorno imediato leva ao descarte de ambas, sem publicar um
         par parcial. Em sucesso, os dois intervalos ficam marcados modificados. */
      access = newtm_write (ctx, op->state, offsetof (struct shared_state, inverse),
                            &inverse, sizeof inverse);
      if (access != NEWTM_ACCESS_OK) return access_status (access);
    }
  /* OK entrega a tentativa ao executor: ele valida antes de PUBLISHING,
     publica so intervalos modificados e conserva todas as reservas ate terminar
     as copias. Depois limpa todos os recursos. Uma tentativa somente de leitura
     nao publica dados; apenas libera seus recursos ao concluir. */
  return NEWTM_LIFECYCLE_OK;
}

/* Converter um intervalo de caracteres decimal, rejeitando sinais, caracteres
   extras e overflow antes de multiplicar/acumular. out so e escrito em sucesso. */
static bool
decimal (const char *begin, const char *end, unsigned long long *out)
{
  if (begin == end)
    return false;
  unsigned long long value = 0;
  for (const char *p = begin; p != end; ++p)
    {
      if (*p < '0' || *p > '9')
	return false;
      unsigned int digit = (unsigned int) (*p - '0');
      if (value > (ULLONG_MAX - digit) / 10)
	return false;
      value = value * 10 + digit;
    }
  *out = value;
  return true;
}

int
main (int argc, char **argv)
{
  unsigned long long threads_arg, writes_weight, reads_weight, repetitions;
  if (argc != 4)
    goto usage;
  /* Validar parametros antes de iniciar threads/transacoes. A soma dos pesos
     cabe em uint32_t, e o total de operacoes cabe em unsigned long long. */
  const char *colon = strchr (argv[2], ':');
  if (!decimal (argv[1], argv[1] + strlen (argv[1]), &threads_arg)
      || threads_arg == 0 || threads_arg > INT_MAX
      || colon == NULL
      || !decimal (argv[2], colon, &writes_weight)
      || !decimal (colon + 1, argv[2] + strlen (argv[2]), &reads_weight)
      || writes_weight > UINT32_MAX
      || reads_weight > UINT32_MAX - writes_weight
      || writes_weight + reads_weight == 0
      || !decimal (argv[3], argv[3] + strlen (argv[3]), &repetitions)
      || repetitions == 0 || repetitions > ULLONG_MAX / threads_arg)
    goto usage;

  int requested = (int) threads_arg;
  int actual = 0;
  unsigned long long period = writes_weight + reads_weight;
  /* Calcular floor(REPETICOES * W / (W+R)) sem multiplicar dois valores que
     poderiam transbordar. O produto dos restos cabe em 64 bits, pois ambos
     sao menores que period <= UINT32_MAX. Cada escrita confirmada soma um. */
  unsigned long long writes_per_thread
    = (repetitions / period) * writes_weight
      + ((repetitions % period) * writes_weight) / period;
  unsigned long long expected_writes = threads_arg * writes_per_thread;
  unsigned long long expected_reads
    = threads_arg * (repetitions - writes_per_thread);
  unsigned long long writes = 0, reads = 0, errors = 0;
  /* Complemento de zero: o par ja comeca satisfazendo a invariancia. */
  struct shared_state state = { 0, ULLONG_MAX };

  /* As reducoes contam operacoes concluidas por thread, fora das tentativas.
     Somente state e manipulado transacionalmente pelos callbacks concorrentes. */
  omp_set_dynamic (0);
  #pragma omp parallel num_threads(requested) shared(state, actual) \
    reduction(+: writes, reads, errors)
  {
    /* single publica actual para a equipe pela barreira implicita. Se OpenMP
       nao fornecer a equipe solicitada, nenhuma thread inicia a carga. */
    #pragma omp single
    actual = omp_get_num_threads ();

    /* Escala ponderada deterministica, igual a transaction_carga.c. credit
       fica abaixo de period e escolhe leituras/escritas sem gerador aleatorio.
       Cada thread faz floor(N * W / (W+R)) escritas nas N operacoes. A escolha
       ocorre fora do callback: conflitos repetem a mesma operacao, sem mudar
       a proporcao nem avancar o credito de novo. */
    unsigned long long credit = 0;
    if (actual == requested)
      for (unsigned long long i = 0; i < repetitions; ++i)
	{
	  bool writing = credit >= reads_weight;
	  if (writing)
	    credit -= reads_weight;
	  else
	    credit += writes_weight;
	  struct operation op = { &state, writing };
          /* newtm_execute administra contexto, commit, limpeza e retry completo.
             op permanece vivo e inalterado ate a chamada terminar. Contar apenas
             depois do sucesso evita incluir tentativas descartadas na carga. */
          if (newtm_execute (operate, &op) != NEWTM_LIFECYCLE_OK)
            { ++errors; continue; }
	  if (writing)
	    ++writes;
	  else
	    ++reads;
	}
  }

  /* A barreira final de parallel garante que nao ha transacoes em andamento.
     Fechar o proprietario da hash depois da liberacao de todas as referencias;
     agora tambem e seguro inspecionar os campos originais sem newtm_read. */
  if (newtm_hash_owner_close () != NEWTM_HASH_OK)
    return EXIT_FAILURE;
  if (actual != requested)
    {
      fprintf (stderr, "FALHA: threads solicitadas=%d, lancadas=%d; "
	       "verifique os limites do runtime/ambiente\n", requested, actual);
      return EXIT_FAILURE;
    }
  /* Verificar totais e o par publicado. Cada escrita confirmada incrementa
     value uma vez, independentemente de quantos retries foram necessarios. */
  if (errors != 0 || writes != expected_writes || reads != expected_reads
      || state.value != expected_writes || state.inverse != ~state.value)
    {
      fprintf (stderr, "FALHA: escritas=%llu/%llu leituras=%llu/%llu "
	       "valor=%llu erros=%llu\n", writes, expected_writes,
	       reads, expected_reads, state.value, errors);
      return EXIT_FAILURE;
    }
  printf ("[teste] interno carga OK: threads=%d relacao=%llu:%llu repeticoes=%llu "
	  "transacoes=%llu escritas=%llu leituras=%llu valor=%llu\n",
	  actual, writes_weight, reads_weight, repetitions,
	  threads_arg * repetitions, writes, reads, state.value);
  return EXIT_SUCCESS;

usage:
  fprintf (stderr, "Uso: %s THREADS ESCRITAS:LEITURAS REPETICOES\n"
	   "THREADS e REPETICOES devem ser positivos; pesos e sua soma "
	   "devem caber em uint32_t, com soma positiva.\n"
	   "O total de transacoes deve caber em unsigned long long.\n",
	   argv[0]);
  return EXIT_FAILURE;
}
