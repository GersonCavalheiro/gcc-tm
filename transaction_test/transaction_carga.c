/* Parameterized concurrent workload for the experimental transaction bridge.
   Current transaction regions do not isolate memory: protect the shared data
   with an external critical section, including registration/release.
   Ratios classify transactions; a write transaction is a read-modify-write. */
#include <limits.h>
#include <omp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  /* The remainder product fits in 64 bits because period <= UINT32_MAX.
     Quotient/remainder avoids overflowing repetitions * writes_weight. */
  unsigned long long writes_per_thread
    = (repetitions / period) * writes_weight
      + ((repetitions % period) * writes_weight) / period;
  unsigned long long expected_writes = threads_arg * writes_per_thread;
  unsigned long long expected_reads
    = threads_arg * (repetitions - writes_per_thread);
  unsigned long long writes = 0, reads = 0, errors = 0;
  struct
  {
    unsigned long long value;
    unsigned long long inverse;
  } state = { 0, ULLONG_MAX };

  omp_set_dynamic (0);
  #pragma omp parallel num_threads(requested) shared(state, actual) \
    reduction(+: writes, reads, errors)
  {
    #pragma omp single
    actual = omp_get_num_threads ();

    /* Deterministic weighted schedule, interleaving operations without a
       random generator or multiplication of potentially large loop indices.
       credit stays below period; each thread performs floor(N * W / (W+R))
       write transactions, and the rest are read transactions. */
    unsigned long long credit = 0;
    if (actual == requested)
      for (unsigned long long i = 0; i < repetitions; ++i)
	{
	  bool writing = credit >= reads_weight;
	  if (writing)
	    credit -= reads_weight;
	  else
	    credit += writes_weight;
	  unsigned long long observed, inverse;

	  #pragma omp critical (transaction_carga_state)
	  {
	    #pragma omp transaction (state, writing, observed, inverse)
	    {
	      observed = state.value;
	      inverse = state.inverse;
	      if (writing)
		{
		  state.value = observed + 1;
		  state.inverse = ~state.value;
		}
	    }
	  }
	  if (inverse != ~observed)
	    ++errors;
	  if (writing)
	    ++writes;
	  else
	    ++reads;
	}
  }

  if (actual != requested)
    {
      fprintf (stderr, "FALHA: threads solicitadas=%d, lancadas=%d; "
	       "verifique os limites do runtime/ambiente\n", requested, actual);
      return EXIT_FAILURE;
    }
  if (errors != 0 || writes != expected_writes || reads != expected_reads
      || state.value != expected_writes || state.inverse != ~state.value)
    {
      fprintf (stderr, "FALHA: escritas=%llu/%llu leituras=%llu/%llu "
	       "valor=%llu erros=%llu\n", writes, expected_writes,
	       reads, expected_reads, state.value, errors);
      return EXIT_FAILURE;
    }
  printf ("[teste] carga OK: threads=%d relacao=%llu:%llu repeticoes=%llu "
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
