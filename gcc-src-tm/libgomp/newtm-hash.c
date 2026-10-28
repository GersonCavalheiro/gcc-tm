/* Copyright (C) 2026 Free Software Foundation, Inc.

   This file is part of the GNU Offloading and Multi Processing Library
   (libgomp).

   Libgomp is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

/* Hash storage for the experimental transaction runtime.
   Table lifetime, entry acquisition/release, immutable queries and metadata
   visits and consistent counting are operational; there is no STM yet.  */

#include "libgomp.h"
#include "newtm-hash.h"
#include "newtm-context.h"
#include <stdio.h>
#include <stdlib.h>

/* Physical lifetime and collision linkage only.  Policy data remain opaque;
   transaction contexts, reader identities and private buffers belong to the
   separate transaction layer, not to the hash implementation.  */
struct newtm_hash_entry
{
  const void *address;
  size_t size;
  size_t references;
  size_t bucket_index;
  void *policy_data;
  struct newtm_hash_entry *next;
};

struct newtm_hash_bucket
{
  gomp_mutex_t mutex;
  struct newtm_hash_entry *entries;
  size_t entry_count;
};

struct newtm_hash_table
{
  struct newtm_hash_bucket *buckets;
  size_t bucket_count;
  newtm_hash_policy_destroy_fn destroy_policy;
};

/* Address-to-bucket algorithm, shared by acquire and lookup:
   Convert ADDRESS to uintptr_t without discarding low alignment bits. Select
   the MurmurHash3 finalizer by UINTPTR_MAX, not sizeof (unsigned long):
   - UINTPTR_MAX == UINT32_MAX: unsigned 32-bit x; x ^= x >> 16;
     x *= 0x85ebca6b; x ^= x >> 13; x *= 0xc2b2ae35; x ^= x >> 16.
   - UINTPTR_MAX == UINT64_MAX: unsigned 64-bit x; x ^= x >> 33;
     x *= 0xff51afd7ed558ccd; x ^= x >> 33;
     x *= 0xc4ceb9fe1a85ec53; x ^= x >> 33.
   Products wrap at the selected unsigned width; use appropriately typed
   constants and shifts strictly smaller than that width. Finalizers from
   https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp .
   Compute x % bucket_count without narrowing x before the reduction. Any
   positive capacity whose allocation is representable is admissible; do not
   substitute a bit mask or require a power of two. Equality still compares
   exact addresses, not mixed values. Store the resulting immutable index in
   new entries for release/visit. No seed, mutable state or object reads.
   Widths other than 32/64 have no selected implementation: the operational
   implementation must diagnose unsupported widths at build time, rather
   than silently truncate; wider-width support requires an explicit extension.
   This mapping does not read the registered object.  */

static size_t
newtm_hash_bucket_index (const void *address, size_t bucket_count)
{
  uintptr_t x = (uintptr_t) address;
#if UINTPTR_MAX == UINT32_MAX
  x ^= x >> 16;
  x *= UINT32_C (0x85ebca6b);
  x ^= x >> 13;
  x *= UINT32_C (0xc2b2ae35);
  x ^= x >> 16;
#elif UINTPTR_MAX == UINT64_MAX
  x ^= x >> 33;
  x *= UINT64_C (0xff51afd7ed558ccd);
  x ^= x >> 33;
  x *= UINT64_C (0xc4ceb9fe1a85ec53);
  x ^= x >> 33;
#else
#error "newtm hash requires a 32-bit or 64-bit uintptr_t"
#endif
  return x % bucket_count;
}

/* Logging constraint for the proposed operational algorithms:
   Never call printf/fflush or other diagnostic I/O while holding any bucket
   mutex, including from visit callbacks. The same restriction applies while
   count holds multiple buckets. Keep the printf/fflush debug messages.
   Lazy bridge initialization must also release its global mutex before any
   creation log or fatal diagnostic. Raw allocation under a runtime lock must
   report failure to its caller without I/O; release the lock before gomp_fatal.
   Capture only the required data while protected and emit before locking
   or after unlocking; never dereference a released entry for logging, expose mutable
   metadata without protection or infer concurrent operation order from print
   order. If tracing at the bridge, likewise hold no bucket mutex during I/O.
   Update output-dependent tests and documentation in the same change as any
   message change. Verify actual operations, reference lifetime and callbacks;
   message counts alone are not proof of correct hash behavior.  */

/* Common functional pre/postconditions:
   TABLE/ENTRY arguments must denote live matching objects; destruction must
   not overlap any other operation, including immutable-field queries.
   Retained references keep entries alive, not the registered program objects.
   OUT, when nonnull, must designate writable exclusive storage, not overlapping
   table, entries or metadata. Clearing OUT does not release its old contents;
   the caller must preserve any previously owned reference separately.
   Returnable failures cause no partial insertion or reference increment by
   this call; other threads may still change the table. No normal return leaves
   a bucket mutex held. NULL checks do not validate dangling pointers.
   These are hash lifetime/storage contracts, not transactional reservations
   or synchronization of the registered objects' values.  */

/* Allocate a table without logging or fatal diagnostics.  The caller validates
   a positive, representable capacity and handles NULL only after releasing any
   initialization lock.  This internal failure is never a recoverable hash API
   result.  Do not publish a partially initialized table.  */
static struct newtm_hash_table *
newtm_hash_allocate_table (size_t bucket_count,
			   newtm_hash_policy_destroy_fn destroy_policy)
{
  struct newtm_hash_table *table = malloc (sizeof (*table));
  if (table == NULL)
    return NULL;
  table->buckets = calloc (bucket_count, sizeof (*table->buckets));
  if (table->buckets == NULL)
    {
      free (table);
      return NULL;
    }
  table->bucket_count = bucket_count;
  table->destroy_policy = destroy_policy;
  for (size_t i = 0; i < bucket_count; ++i)
    gomp_mutex_init (&table->buckets[i].mutex);
  return table;
}

/* Share the existing diagnostic format with lazy bridge initialization, which
   must print only after releasing newtm_trace_lock.  */
static void
newtm_hash_log_create (size_t bucket_count,
		       newtm_hash_policy_destroy_fn destroy_policy,
		       struct newtm_hash_table **out)
{
  printf ("[newtm] newtm_hash_create: bucket_count=%zu, destroy_policy=%s, "
	  "out=%p\n", bucket_count,
	  destroy_policy != NULL ? "nonnull" : "null", (void *) out);
  fflush (stdout);
}

/* Create an empty table with BUCKET_COUNT buckets and one mutex per bucket.
   DESTROY_POLICY owns nonnull entry metadata; it may be null only when all
   metadata remain null.  Store the new table in *OUT.  Return OK, INVALID
   for zero capacity/null OUT, or OVERFLOW for an unrepresentable allocation.
   On failure clear *OUT when OUT is valid. Allocation failure is fatal.
   Allocation uses a shared nonlogging malloc/calloc helper; the bridge calls
   that helper while initializing and emits the existing creation log only
   after publishing the complete table and releasing its global mutex.
   The public wrapper logs without acquiring that mutex. No recoverable OOM
   status is introduced; fatal diagnostics occur outside runtime locks.

   Algoritmo implementado:
   1. Emitir o log pelo helper newtm_hash_log_create (printf e fflush).
      Limpar *OUT quando valido; rejeitar OUT nulo ou BUCKET_COUNT zero com
      INVALID. Verificar BUCKET_COUNT > SIZE_MAX / sizeof (bucket) e retornar
      OVERFLOW antes de alocar, mantendo *OUT nulo.
   2. Chamar o helper de alocacao sem E/S: malloc da tabela, calloc dos
      buckets, armazenamento da capacidade/destrutor e inicializacao de todos
      os mutexes. Se malloc falhar, o helper retorna NULL; se calloc falhar,
      libera a tabela parcial antes de retornar NULL. Listas e contadores
      dos buckets ficam zerados por calloc.
   3. Se o helper retornar NULL, chamar gomp_fatal; esta primitiva nao adquire
      locks. Caso contrario, publicar a tabela completa em *OUT e retornar OK.
      A ponte usa diretamente o helper e controla sua publicacao/log no
      protocolo do proprietario; nao chama este wrapper sob o lock global.  */
enum newtm_hash_status
newtm_hash_create (size_t bucket_count,
                   newtm_hash_policy_destroy_fn destroy_policy,
                   struct newtm_hash_table **out)
{
  newtm_hash_log_create (bucket_count, destroy_policy, out);
  if (out)
    *out = NULL;
  if (out == NULL || bucket_count == 0)
    return NEWTM_HASH_INVALID;
  if (bucket_count > SIZE_MAX / sizeof (struct newtm_hash_bucket))
    return NEWTM_HASH_OVERFLOW;

  struct newtm_hash_table *table
    = newtm_hash_allocate_table (bucket_count, destroy_policy);
  if (table == NULL)
    gomp_fatal ("Out of memory allocating transaction hash table");

  *out = table;
  return NEWTM_HASH_OK;
}

/* With externally established quiescence, destroy TABLE and its mutexes
   if empty. The caller must prevent new operations and wait for all active
   operations, visits and policy destructors to finish, keeping this exclusion
   through the return. Idle retained references are allowed, but their entries
   cause BUSY; they must not be used concurrently with this call. A zero count
   neither establishes quiescence nor proves that destructors have finished.
   Return INVALID for null TABLE, BUSY if any entry remains, or OK on success.
   BUSY must leave the complete table intact; success invalidates TABLE.

   Algoritmo implementado:
   1. Emitir printf/fflush e retornar INVALID se TABLE for nula. Para tabela
      nao nula, assumir a quiescencia externa como pre-condicao, sem tenta-la
      estabelecer ou verifica-la. Esta primitiva nao adquire mutexes.
   2. Percorrer os buckets em ordem crescente. Ao encontrar entries nao nulo
      OU entry_count diferente de zero, retornar BUSY imediatamente, sem
      alterar tabela, referencias ou destruir qualquer recurso.
   3. Somente se todos estiverem vazios, percorrer novamente em ordem
      crescente destruindo cada mutex; liberar buckets, liberar TABLE e
      retornar OK. Nenhum acesso posterior a TABLE sera permitido.
      Count zero nao substitui a quiescencia: na tabela global, cabe ao
      proprietario fechar admissao e drenar participacoes antes desta chamada.  */
enum newtm_hash_status
newtm_hash_destroy (struct newtm_hash_table *table)
{
  printf ("[newtm] newtm_hash_destroy: table=%p\n", (void *) table);
  fflush (stdout);
  if (table == NULL)
    return NEWTM_HASH_INVALID;

  /* External quiescence permits inspection without locking.  Complete this
     pass before destroying any mutex, so BUSY leaves the whole table intact.
     Do not infer quiescence from empty buckets or from newtm_hash_count.  */
  for (size_t i = 0; i < table->bucket_count; ++i)
    if (table->buckets[i].entries != NULL
	|| table->buckets[i].entry_count != 0)
      return NEWTM_HASH_BUSY;

  for (size_t i = 0; i < table->bucket_count; ++i)
    gomp_mutex_destroy (&table->buckets[i].mutex);
  free (table->buckets);
  free (table);
  return NEWTM_HASH_OK;
}

/* Find or insert ADDRESS with SIZE bytes in TABLE under its bucket mutex.
   Return a retained entry in *OUT, creating it with null metadata if absent.
   Return OK, INVALID for null arguments/zero SIZE, SIZE_MISMATCH for an
   existing key with another size, or OVERFLOW for counters/allocation size.
   Clear *OUT on failure when valid; make no partial changes.  Allocation
   failure is fatal. Use the shared uintptr_t/fmix32/fmix64 bucket algorithm
   specified above. Do not read or reserve the program object.
   Initial implementation allocates a missing entry with malloc under the
   bucket mutex. On allocation failure, unlock before gomp_fatal, keeping OUT
   null and the table unchanged; OOM remains fatal, not a returned status.
   Once operational correctness is established, measured allocator contention
   may motivate allocation outside the lock. Such an optimization must recheck
   the key under lock, dispose of unused candidates and preserve uniqueness,
   size checks, overflow handling, reference ownership and failure atomicity.
   It is a future option, not a requirement of the initial implementation.

   Algoritmo implementado:
   1. Emitir printf/fflush antes de adquirir o mutex. Limpar *OUT quando
      valido; rejeitar TABLE, ADDRESS ou OUT nulos e SIZE zero com INVALID.
   2. Calcular o indice por uintptr_t, fmix32/fmix64 e modulo bucket_count,
      conforme o algoritmo comum. Inicializar status=OK, bloquear o bucket
      e percorrer sua lista comparando enderecos exatos.
   3. Se encontrar, verificar primeiro SIZE: divergencia define SIZE_MISMATCH;
      senao, references==SIZE_MAX define OVERFLOW; senao, incrementar a
      referencia e preencher *OUT. Desbloquear e retornar esse status, sem
      examinar o contador de entradas ou alocar no caminho de chave existente.
   4. Se ausente e entry_count==SIZE_MAX, definir OVERFLOW sem inserir.
      Caso contrario, fazer malloc da entrada ainda sob o mutex. Se falhar,
      desbloquear e chamar gomp_fatal, mantendo *OUT nulo e sem alteracoes
      parciais causadas por esta chamada. Na entrada nova, definir address,
      size, references=1, bucket_index, policy_data=NULL e next=entries;
      publicar na cabeca da lista, incrementar entry_count e preencher *OUT.
   5. Desbloquear e retornar OK ou OVERFLOW. Cada sucesso entrega uma
      referencia a consumir com release. Otimizar a alocacao fora do lock
      continua sendo possibilidade futura, nao comportamento implementado.  */
enum newtm_hash_status
newtm_hash_acquire (struct newtm_hash_table *table, const void *address,
                    size_t size, struct newtm_hash_entry **out)
{
  printf ("[newtm] newtm_hash_acquire: table=%p, address=%p, size=%zu, "
	  "out=%p\n", (void *) table, (void *) address, size, (void *) out);
  fflush (stdout);
  if (out)
    *out = NULL;
  if (table == NULL || address == NULL || size == 0 || out == NULL)
    return NEWTM_HASH_INVALID;

  size_t index = newtm_hash_bucket_index (address, table->bucket_count);
  struct newtm_hash_bucket *bucket = &table->buckets[index];
  enum newtm_hash_status status = NEWTM_HASH_OK;
  gomp_mutex_lock (&bucket->mutex);
  for (struct newtm_hash_entry *entry = bucket->entries; entry;
       entry = entry->next)
    if (entry->address == address)
      {
	if (entry->size != size)
	  status = NEWTM_HASH_SIZE_MISMATCH;
	else if (entry->references == SIZE_MAX)
	  status = NEWTM_HASH_OVERFLOW;
	else
	  {
	    ++entry->references;
	    *out = entry;
	  }
	gomp_mutex_unlock (&bucket->mutex);
	return status;
      }

  if (bucket->entry_count == SIZE_MAX)
    status = NEWTM_HASH_OVERFLOW;
  else
    {
      struct newtm_hash_entry *entry = malloc (sizeof (*entry));
      if (entry == NULL)
	{
	  gomp_mutex_unlock (&bucket->mutex);
	  gomp_fatal ("Out of memory allocating %lu bytes",
		      (unsigned long) sizeof (*entry));
	}
      entry->address = address;
      entry->size = size;
      entry->references = 1;
      entry->bucket_index = index;
      entry->policy_data = NULL;
      entry->next = bucket->entries;
      bucket->entries = entry;
      ++bucket->entry_count;
      *out = entry;
    }
  gomp_mutex_unlock (&bucket->mutex);
  return status;
}

/* Look up ADDRESS without insertion and retain the entry under its bucket
   mutex.  Return OK, NOT_FOUND, INVALID for null arguments, or OVERFLOW for
   the reference counter.  Clear *OUT on failure when valid.  The caller must
   release a successful reference; it is not a transactional reservation.
   Use the same uintptr_t/fmix32/fmix64 reduction as acquire, without truncation.

   Algoritmo implementado:
   1. Emitir printf/fflush antes dos locks. Limpar *OUT quando valido e
      rejeitar TABLE, ADDRESS ou OUT nulos com INVALID.
   2. Calcular o mesmo indice uintptr_t/fmix32/fmix64/modulo de acquire;
      inicializar status=NOT_FOUND, bloquear o bucket e buscar endereco exato.
   3. Se encontrar, references==SIZE_MAX define OVERFLOW sem incremento;
      caso contrario, incrementar references, preencher *OUT e definir OK.
      Encerrar a busca. Se ausente, preservar NOT_FOUND e *OUT nulo.
   4. Desbloquear e retornar status. Nunca inserir nem validar tamanho;
      a busca e a retencao ocorrem na mesma secao critica, e cada sucesso
      entrega uma referencia que o chamador devera liberar.  */
enum newtm_hash_status
newtm_hash_lookup_acquire (struct newtm_hash_table *table,
                           const void *address,
                           struct newtm_hash_entry **out)
{
  printf ("[newtm] newtm_hash_lookup_acquire: table=%p, address=%p, "
	  "out=%p\n", (void *) table, (void *) address, (void *) out);
  fflush (stdout);
  if (out)
    *out = NULL;
  if (table == NULL || address == NULL || out == NULL)
    return NEWTM_HASH_INVALID;

  size_t index = newtm_hash_bucket_index (address, table->bucket_count);
  struct newtm_hash_bucket *bucket = &table->buckets[index];
  enum newtm_hash_status status = NEWTM_HASH_NOT_FOUND;
  gomp_mutex_lock (&bucket->mutex);
  for (struct newtm_hash_entry *entry = bucket->entries; entry;
       entry = entry->next)
    if (entry->address == address)
      {
	if (entry->references == SIZE_MAX)
	  status = NEWTM_HASH_OVERFLOW;
	else
	  {
	    ++entry->references;
	    *out = entry;
	    status = NEWTM_HASH_OK;
	  }
	break;
      }
  gomp_mutex_unlock (&bucket->mutex);
  return status;
}

/* Release one caller-owned reference to ENTRY in TABLE under its bucket lock.
   Require a live matching TABLE/ENTRY, a positive reference count and one
   caller-owned reference obtained by a successful acquire/lookup_acquire.
   Each successful acquisition requires one release; copying a pointer does
   not acquire a reference, and a failed acquisition requires no release.
   Withdraw only the participations/reservations sustained by the consumed
   reference before this call; other independently retained users may remain.
   Keep TABLE alive until this call fully returns, including the destructor
   and entry deallocation after unlock. Empty buckets do not prove quiescence.
   Decrement under the bucket mutex. At zero, unlink, unlock, destroy nonnull
   metadata exactly once and free the entry before returning. No participation
   or reservation may outlive the last reference sustaining it.
   A positive decrement only means this call does not destroy ENTRY; another
   thread may release the last reference and free it before this call returns.
   After release, access requires another independently retained reference.
   A new generation of the same address may exist during old metadata cleanup;
   its destructor must touch neither the program object nor the new generation.
   The callback must return normally, without reentry, exceptions or longjmp.
   Return no state and no held bucket lock. NULL, mismatched table, zero count,
   double release and dangling pointers are contract violations with no
   guaranteed diagnosis or recoverable result; never force removal while used.

   Algoritmo implementado:
   1. Assumir TABLE/ENTRY vivos e correspondentes, entrada encadeada,
      contador positivo e uma referencia do chamador; nao ha validacao desses
      argumentos. Participacoes/reservas sustentadas por essa referencia
      devem ter sido retiradas pelo chamador. TABLE deve permanecer viva
      ate concluir callback e free; o proprietario garante isso por lease.
   2. Emitir printf/fflush sem lock; obter o bucket pelo indice imutavel da
      entrada e bloquear seu mutex. Decrementar references; se nao zerar,
      desbloquear e retornar sem destruir nesta chamada. Outra thread pode
      consumir a ultima referencia antes deste retorno; uso posterior exige
      outra referencia retida independentemente.
   3. Se zerar, localizar ENTRY na lista percorrendo os ponteiros de ligacao
      a partir de entries; substituir sua ligacao por ENTRY->next e
      decrementar entry_count. Desbloquear o bucket.
   4. Se policy_data nao for nulo, invocar table->destroy_policy com esse
      ponteiro; liberar ENTRY e retornar. O codigo nao valida o destrutor:
      sua configuracao e retorno normal, sem reentrada/excecao/longjmp,
      sao obrigacoes do contrato. Ele atua apenas na geracao retirada,
      sem acessar a entrada, o objeto do programa ou outra geracao.
      Uma nova geracao pode surgir apos o desbloqueio. Nao acessar ENTRY
      depois de free nem usar ausencia de entradas como prova de quiescencia.  */
void
newtm_hash_release (struct newtm_hash_table *table,
                    struct newtm_hash_entry *entry)
{
  printf ("[newtm] newtm_hash_release: table=%p, entry=%p\n",
	  (void *) table, (void *) entry);
  fflush (stdout);
  struct newtm_hash_bucket *bucket = &table->buckets[entry->bucket_index];
  gomp_mutex_lock (&bucket->mutex);
  if (--entry->references != 0)
    {
      gomp_mutex_unlock (&bucket->mutex);
      return;
    }

  struct newtm_hash_entry **link = &bucket->entries;
  while (*link != entry)
    link = &(*link)->next;
  *link = entry->next;
  --bucket->entry_count;
  gomp_mutex_unlock (&bucket->mutex);

  if (entry->policy_data != NULL)
    table->destroy_policy (entry->policy_data);
  free (entry);
}

/* Return the immutable canonical program-object address stored in ENTRY.
   Require a valid retained reference; no lock or object dereference is needed.
   The returned address does not extend the lifetime of the program object.

   Algoritmo implementado:
   1. Assumir ENTRY valida e referencia retida durante toda a consulta;
      esta pre-condicao nao e verificada pela primitiva.
   2. Emitir printf/fflush e retornar ENTRY->address diretamente, sem lock.
      O campo e imutavel; nao ler o objeto, adquirir/consumir referencias
      nem estender sua vida util.  */
const void *
newtm_hash_entry_address (const struct newtm_hash_entry *entry)
{
  printf ("[newtm] newtm_hash_entry_address: entry=%p\n", (void *) entry);
  fflush (stdout);
  return entry->address;
}

/* Return ENTRY's immutable object size in bytes.  Require a valid retained
   reference; do not read the object or acquire a transactional reservation.

   Algoritmo implementado:
   1. Assumir ENTRY valida e referencia retida durante toda a consulta;
      esta pre-condicao nao e verificada pela primitiva.
   2. Emitir printf/fflush e retornar ENTRY->size diretamente, sem lock.
      O campo e imutavel; nao ler o objeto nem adquirir/consumir referencias.  */
size_t
newtm_hash_entry_size (const struct newtm_hash_entry *entry)
{
  printf ("[newtm] newtm_hash_entry_size: entry=%p\n", (void *) entry);
  fflush (stdout);
  return entry->size;
}

/* Call nonnull VISIT on the address, size and metadata slot of retained ENTRY
   under its bucket mutex in TABLE, forwarding ARGUMENT (which may be null).
   Require valid matching TABLE/ENTRY.  Return no value; results travel through
   ARGUMENT. The callback must return normally without consuming the retained
   reference, unlocking, reentering this table, waiting for a transaction,
   executing user code or acquiring locks with an inverted order. It must not
   retain the metadata slot address. Metadata ownership belongs to the entry;
   replacements must dispose of or transfer the old metadata correctly.
   Nonnull metadata require a configured policy destructor. Pointers copied
   out of the callback are not protected after unlock; any later use needs an
   explicit lifetime and synchronization protocol. Return with the reference
   preserved and mutex released; callback changes are not rolled back.

   Algoritmo implementado:
   1. Assumir TABLE/ENTRY correspondentes, referencia retida e VISIT nao nulo;
      nao ha verificacao desses argumentos ou do contrato do callback.
      Emitir printf/fflush antes de adquirir qualquer mutex.
   2. Obter o bucket por ENTRY->bucket_index e bloquear seu mutex.
   3. Chamar VISIT uma vez com ENTRY->address, ENTRY->size,
      &ENTRY->policy_data e ARGUMENT, que pode ser nulo. O callback deve
      retornar normalmente, sem espera por transacao, reentrada, E/S,
      excecao/longjmp ou inversao de locks; nao pode consumir a referencia,
      liberar o mutex ou guardar o endereco do slot. Propriedade e destino
      de metadados substituidos pertencem ao contrato do callback, sem
      validacao, destruicao automatica ou rollback pela primitiva. Metadados
      nao nulos exigem destroy_policy configurado. Ponteiros copiados para
      fora exigem protocolo proprio de vida util/sincronizacao depois.
   4. Apos retorno normal do callback, desbloquear e retornar, sem alterar
      referencias. Resultados via ARGUMENT e mudancas em policy_data ficam
      preservados; o codigo nao oferece cleanup para saida anormal do callback.  */
void
newtm_hash_entry_visit (struct newtm_hash_table *table,
                        struct newtm_hash_entry *entry,
                        newtm_hash_visit_fn visit, void *argument)
{
  printf ("[newtm] newtm_hash_entry_visit: table=%p, entry=%p, visit=%s, "
	  "argument=%p\n", (void *) table, (void *) entry,
	  visit != NULL ? "nonnull" : "null", argument);
  fflush (stdout);
  struct newtm_hash_bucket *bucket = &table->buckets[entry->bucket_index];
  gomp_mutex_lock (&bucket->mutex);
  visit (entry->address, entry->size, &entry->policy_data, argument);
  gomp_mutex_unlock (&bucket->mutex);
}

/* Return a consistent entry count, saturated at SIZE_MAX, for valid TABLE.
   Lock every bucket in ascending order, sum counts, unlock in reverse order.
   For diagnostics only; require no concurrent destruction and no bucket
   lock already held. Never call from a visit or destructor of this table.
   References and transactional participants are not counted. The snapshot may
   change after unlock; zero does not exclude new operations, retained users
   of the table or an unlinked entry whose policy destructor is still running.
   It is not authorization to destroy TABLE; preserve all state/references.

   Algoritmo implementado:
   1. Assumir TABLE valida, sem destruicao concorrente, chamada fora de
      visitas/destruidores e sem mutex de bucket retido; nao verificar essas
      pre-condicoes. Emitir printf/fflush antes de adquirir os locks.
   2. Bloquear todos os buckets em ordem crescente de indice.
   3. Inicializar count=0 e percorrer entry_count em ordem crescente.
      Antes de somar, comparar entry_count > SIZE_MAX - count; se verdadeiro,
      definir count=SIZE_MAX e encerrar a soma imediatamente, mantendo todos
      os locks adquiridos. Senao, adicionar entry_count e continuar.
   4. Desbloquear TODOS os buckets em ordem inversa, inclusive os posteriores
      ao ponto de saturacao, e retornar count. Nao alterar contadores ou
      referencias. A fotografia conta entradas, nao referencias, e pode
      mudar apos unlock. Zero nao comprova quiescencia nem fim dos destruidores;
      na tabela global, manter a lease durante toda a chamada.  */
size_t
newtm_hash_count (struct newtm_hash_table *table)
{
  printf ("[newtm] newtm_hash_count: table=%p\n", (void *) table);
  fflush (stdout);
  for (size_t i = 0; i < table->bucket_count; ++i)
    gomp_mutex_lock (&table->buckets[i].mutex);

  size_t count = 0;
  for (size_t i = 0; i < table->bucket_count; ++i)
    {
      size_t entries = table->buckets[i].entry_count;
      if (entries > SIZE_MAX - count)
	{
	  count = SIZE_MAX;
	  break;
	}
      count += entries;
    }

  for (size_t i = table->bucket_count; i != 0; --i)
    gomp_mutex_unlock (&table->buckets[i - 1].mutex);
  return count;
}


/* Registration bridge for generated code.  Each distinct object in a context
   owns a real hash reference through flat nesting.  This does not grant
   reservations or transactional storage.  Task suspension is unsupported.  */
struct newtm_trace_object
{
  void *address;
  size_t size;
  struct newtm_hash_entry *entry;
  struct newtm_trace_object *next;
};

struct newtm_trace_context
{
  struct newtm_trace_object *objects;
  struct newtm_context *attempt;
};

enum newtm_owner_state
{
  NEWTM_OWNER_OPEN,
  NEWTM_OWNER_CLOSING,
  NEWTM_OWNER_CLOSED
};

/* This control block outlives the table, including late rejected admissions.
   All fields below are protected by newtm_trace_lock. The lock is deliberately
   not destroyed at process finalization: rejected/waiting callers may still
   need it. Library unload still requires external exclusion of code users. */
static gomp_mutex_t newtm_trace_lock;
static bool newtm_trace_initialized;
static struct newtm_hash_table *newtm_trace_table;
static enum newtm_owner_state newtm_trace_state = NEWTM_OWNER_OPEN;
static size_t newtm_trace_users;
static bool newtm_trace_destroying;

static void __attribute__ ((constructor))
newtm_trace_initialize (void)
{
#if !GOMP_MUTEX_INIT_0
  gomp_mutex_init (&newtm_trace_lock);
#endif
}

enum newtm_hash_status
newtm_hash_owner_close (void)
{
  gomp_mutex_lock (&newtm_trace_lock);
  if (newtm_trace_state == NEWTM_OWNER_CLOSED)
    {
      gomp_mutex_unlock (&newtm_trace_lock);
      return NEWTM_HASH_OK;
    }
  newtm_trace_state = NEWTM_OWNER_CLOSING;
  if (newtm_trace_users != 0 || newtm_trace_destroying)
    {
      gomp_mutex_unlock (&newtm_trace_lock);
      return NEWTM_HASH_BUSY;
    }
  /* Close admission and designate a single coordinator before dropping the
     lock. No admitted user can now access this table, even between calls or
     during a metadata destructor. Other closers return BUSY, not double free. */
  newtm_trace_destroying = true;
  struct newtm_hash_table *table = newtm_trace_table;
  gomp_mutex_unlock (&newtm_trace_lock);

  enum newtm_hash_status status
    = table ? newtm_hash_destroy (table) : NEWTM_HASH_OK;

  gomp_mutex_lock (&newtm_trace_lock);
  if (status == NEWTM_HASH_OK)
    {
      newtm_trace_table = NULL;
      newtm_trace_initialized = false;
      newtm_trace_state = NEWTM_OWNER_CLOSED;
    }
  newtm_trace_destroying = false;
  gomp_mutex_unlock (&newtm_trace_lock);
  return status;
}

static void __attribute__ ((destructor))
newtm_trace_finalize (void)
{
  /* Never wait for a context owned by the exiting thread, force references
     away, or recursively call exit via gomp_fatal. If busy, leave table/control
     alive for process reclamation; explicit close can finish after users join.
     This is not an unload barrier for dlclose. */
  (void) newtm_hash_owner_close ();
}

/* Algorithm: under the admission gate reject closing/closed or overflow,
   lazily create a complete table with immutable newtm_policy_destroy, and
   count the participation before publishing the lease.  Unlock before create
   logs or fatal allocation diagnostics; the admitted lease covers the log.
   No metadata is allocated here and no partially built table is published. */
enum newtm_hash_status
newtm_hash_owner_acquire (struct newtm_hash_owner_lease *lease)
{
  if (lease == NULL || lease->active)
    return NEWTM_HASH_INVALID;
  lease->table = NULL;
  bool attempted_create = false;
  bool allocation_failed = false;
  gomp_mutex_lock (&newtm_trace_lock);
  if (newtm_trace_state != NEWTM_OWNER_OPEN)
    {
      enum newtm_hash_status status = newtm_trace_state == NEWTM_OWNER_CLOSING
	? NEWTM_HASH_BUSY : NEWTM_HASH_INVALID;
      gomp_mutex_unlock (&newtm_trace_lock);
      return status;
    }
  if (newtm_trace_users == SIZE_MAX)
    {
      gomp_mutex_unlock (&newtm_trace_lock);
      return NEWTM_HASH_OVERFLOW;
    }
  if (!newtm_trace_initialized)
    {
      attempted_create = true;
      /* The fixed default capacity is positive and representable.  Do not
	 call the public create wrapper here: its printf and fatal allocation
	 diagnostics would acquire stdio locks while holding this mutex.  */
      struct newtm_hash_table *table
	= newtm_hash_allocate_table (NEWTM_HASH_DEFAULT_BUCKETS,
                                     newtm_policy_destroy);
      if (table == NULL)
	allocation_failed = true;
      else
	{
	  newtm_trace_table = table;
	  newtm_trace_initialized = true;
	}
    }
  if (!allocation_failed)
    {
      ++newtm_trace_users;
      lease->table = newtm_trace_table;
      lease->active = true;
    }
  gomp_mutex_unlock (&newtm_trace_lock);
  if (attempted_create)
    newtm_hash_log_create (NEWTM_HASH_DEFAULT_BUCKETS, newtm_policy_destroy,
			   &newtm_trace_table);
  if (allocation_failed)
    gomp_fatal ("Out of memory allocating transaction hash table");
  return NEWTM_HASH_OK;
}

enum newtm_hash_status
newtm_hash_owner_release (struct newtm_hash_owner_lease *lease)
{
  if (lease == NULL || !lease->active)
    return NEWTM_HASH_INVALID;
  gomp_mutex_lock (&newtm_trace_lock);
  if (newtm_trace_users == 0 || lease->table != newtm_trace_table)
    {
      gomp_mutex_unlock (&newtm_trace_lock);
      return NEWTM_HASH_INVALID;
    }
  --newtm_trace_users;
  lease->table = NULL;
  lease->active = false;
  gomp_mutex_unlock (&newtm_trace_lock);
  return NEWTM_HASH_OK;
}

/* Algorithm: drain the bridge's legacy trace references while the context
   lease remains active, then discard the context, clear TLS and free the bridge.
   Used on normal trace exit and before permanent-error diagnostics.  No commit,
   user callback or mutex held across any release, logging or final fatal. */
static void
newtm_trace_discard (struct gomp_thread *thr, struct newtm_trace_context *ctx)
{
  while (ctx->objects)
    {
      struct newtm_trace_object *p = ctx->objects;
      ctx->objects = p->next;
      newtm_hash_release (ctx->attempt->lease.table, p->entry);
      free (p);
    }
  if (newtm_discard (&ctx->attempt) != NEWTM_LIFECYCLE_OK)
    gomp_fatal ("transaction context cleanup failed");
  thr->newtm_transaction_context = NULL;
  free (ctx);
}

/* Algorithm: outer tracing entry admits into a local lease, creates a stable
   bridge slot, and transfers that lease into the real context via begin.
   Nested entries reuse the context, including while owner admission is closing.
   Returning failures release any untransferred token before fatal diagnosis.
   This bridge still traces program values; it does not provide STM isolation. */
void
GOMP_transaction_begin (void)
{
  struct gomp_thread *thr = gomp_thread ();
  if (!thr)
    gomp_fatal ("transaction tracing requires an initialized runtime thread");
  struct newtm_trace_context *ctx = thr->newtm_transaction_context;
  bool outermost;
  if (!ctx)
    {
      ctx = gomp_malloc_cleared (sizeof (*ctx));
      struct newtm_hash_owner_lease lease = { 0 };
      enum newtm_hash_status status = newtm_hash_owner_acquire (&lease);
      if (status != NEWTM_HASH_OK)
        {
          free (ctx);
          gomp_fatal ("transaction admission failed (status %d)", status);
        }
      enum newtm_lifecycle_status entered
        = newtm_begin (&lease, &ctx->attempt, &outermost);
      if (entered != NEWTM_LIFECYCLE_OK)
        {
          if (newtm_hash_owner_release (&lease) != NEWTM_HASH_OK)
            gomp_fatal ("transaction admission cleanup failed");
          free (ctx);
          gomp_fatal ("transaction context creation failed (status %d)", entered);
        }
      thr->newtm_transaction_context = ctx;
    }
  else if (newtm_begin (NULL, &ctx->attempt, &outermost) != NEWTM_LIFECYCLE_OK)
    {
      newtm_trace_discard (thr, ctx);
      gomp_fatal ("transaction nested entry failed");
    }
}

/* Algorithm: validate an active trace context and object, deduplicate by exact
   address/size, acquire one physical hash reference and link a trace node.
   Returning structural errors drain bridge/context resources before fatal.
   No representation kind is inferred: this legacy bridge does not call the
   functional register stub, acquire reservations or copy program values. */
void
GOMP_transaction_register (void *address, size_t size)
{
  struct gomp_thread *thr = gomp_thread ();
  struct newtm_trace_context *ctx = thr ? thr->newtm_transaction_context : NULL;
  if (!ctx || !ctx->attempt || !ctx->attempt->depth)
    gomp_fatal ("transaction registration without an active region");
  if (address == NULL || size == 0)
    {
      newtm_trace_discard (thr, ctx);
      gomp_fatal ("transaction registration requires a nonnull object and size");
    }
  for (struct newtm_trace_object *p = ctx->objects; p; p = p->next)
    if (p->address == address)
      {
	if (p->size != size)
          {
            newtm_trace_discard (thr, ctx);
            gomp_fatal ("transaction tracing object size mismatch");
          }
	return;
      }
  struct newtm_trace_object *p = gomp_malloc (sizeof (*p));
  p->address = address;
  p->size = size;
  enum newtm_hash_status status
    = newtm_hash_acquire (ctx->attempt->lease.table, address, size, &p->entry);
  if (status != NEWTM_HASH_OK || p->entry == NULL)
    {
      free (p);
      newtm_trace_discard (thr, ctx);
      gomp_fatal ("transaction hash acquisition failed (status %d)", status);
    }
  p->next = ctx->objects;
  ctx->objects = p;
}

/* Algorithm: inner trace exit only reduces context depth.  On outer exit,
   drain bridge-owned trace references before discarding the context.  Discard
   consumes the transferred lease only after all context cleanup.  Detach TLS
   and free the stable bridge slot.  This is trace cleanup, never STM commit;
   the public ABI still lacks compiler-supplied representation classification. */
void
GOMP_transaction_end (void)
{
  struct gomp_thread *thr = gomp_thread ();
  if (!thr)
    gomp_fatal ("transaction tracing requires an initialized runtime thread");
  struct newtm_trace_context *ctx = thr->newtm_transaction_context;
  if (!ctx || !ctx->attempt || !ctx->attempt->depth)
    gomp_fatal ("transaction tracing exit without an active region");
  if (ctx->attempt->depth > 1)
    {
      if (newtm_end (&ctx->attempt) != NEWTM_LIFECYCLE_OK)
        {
          newtm_trace_discard (thr, ctx);
          gomp_fatal ("transaction nested exit failed");
        }
      return;
    }
  newtm_trace_discard (thr, ctx);
}
