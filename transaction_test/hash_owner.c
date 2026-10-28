/* Test the real owner protocol in fresh processes (closure is irreversible).
   Private inspection/interposition is test-only; no production hooks. */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include "../gcc-src-tm/libgomp/libgomp.h"
static int owner_test_printf (const char *, ...);
#define printf owner_test_printf
#include "../gcc-src-tm/libgomp/newtm-hash.c"
#undef printf
#undef NDEBUG
#include <assert.h>

static int create_log_entered, destroy_log_entered, destroy_logs;
static int object;

static int
owner_test_printf (const char *format, ...)
{
  if (strstr (format, "[newtm] newtm_hash_create:") != NULL)
    __atomic_store_n (&create_log_entered, 1, __ATOMIC_RELEASE);
  if (strstr (format, "[newtm] newtm_hash_destroy:") != NULL)
    {
      __atomic_add_fetch (&destroy_logs, 1, __ATOMIC_RELAXED);
      __atomic_store_n (&destroy_log_entered, 1, __ATOMIC_RELEASE);
    }
  va_list args;
  va_start (args, format);
  int result = vprintf (format, args);
  va_end (args);
  return result;
}

static void
check_closed (void)
{
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  assert (newtm_hash_owner_close () == NEWTM_HASH_OK);
  struct newtm_hash_owner_lease late = { 0 };
  assert (newtm_hash_owner_acquire (&late) == NEWTM_HASH_INVALID);
  assert (!late.active && late.table == NULL);
  gomp_mutex_lock (&newtm_trace_lock);
  assert (newtm_trace_state == NEWTM_OWNER_CLOSED);
  assert (!newtm_trace_initialized && newtm_trace_table == NULL);
  assert (newtm_trace_users == 0 && !newtm_trace_destroying);
  gomp_mutex_unlock (&newtm_trace_lock);
}

static void
check_nested (void)
{
  GOMP_transaction_begin ();
  GOMP_transaction_register (&object, sizeof (object));
  GOMP_transaction_begin ();
  assert (newtm_trace_users == 1);
  assert (newtm_hash_owner_close () == NEWTM_HASH_BUSY);
  struct newtm_hash_owner_lease rejected = { 0 };
  assert (newtm_hash_owner_acquire (&rejected) == NEWTM_HASH_BUSY);
  assert (!rejected.active && rejected.table == NULL);
  /* Existing contexts may nest, register and drain while admission is closed. */
  GOMP_transaction_begin ();
  GOMP_transaction_register (&object, sizeof (object));
  GOMP_transaction_end ();
  GOMP_transaction_end ();
  assert (newtm_trace_users == 1);
  assert (newtm_hash_owner_close () == NEWTM_HASH_BUSY);
  GOMP_transaction_end ();
  assert (newtm_trace_users == 0);
  check_closed ();
}

static void *
close_worker (void *argument)
{
  enum newtm_hash_status *status = argument;
  *status = newtm_hash_owner_close ();
  return NULL;
}

static void
check_concurrent_close (void)
{
  struct newtm_hash_owner_lease lease = { 0 };
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_hash_owner_release (&lease) == NEWTM_HASH_OK);
  pthread_t thread;
  enum newtm_hash_status status = NEWTM_HASH_INVALID;
  flockfile (stdout);
  assert (pthread_create (&thread, NULL, close_worker, &status) == 0);
  while (!__atomic_load_n (&destroy_log_entered, __ATOMIC_ACQUIRE))
    sched_yield ();
  /* First closer is blocked in the destroy log, outside the owner mutex. */
  assert (newtm_hash_owner_close () == NEWTM_HASH_BUSY);
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_BUSY);
  funlockfile (stdout);
  assert (pthread_join (thread, NULL) == 0);
  assert (status == NEWTM_HASH_OK);
  check_closed ();
  assert (__atomic_load_n (&destroy_logs, __ATOMIC_RELAXED) == 1);
}

static void *
creation_worker (void *argument)
{
  (void) argument;
  struct newtm_hash_owner_lease lease = { 0 };
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_hash_owner_release (&lease) == NEWTM_HASH_OK);
  return NULL;
}

static void
check_pending_creation_log (void)
{
  pthread_t thread;
  flockfile (stdout);
  assert (pthread_create (&thread, NULL, creation_worker, NULL) == 0);
  while (!__atomic_load_n (&create_log_entered, __ATOMIC_ACQUIRE))
    sched_yield ();
  /* Admission must already protect the table while acquire waits to log. */
  assert (newtm_hash_owner_close () == NEWTM_HASH_BUSY);
  gomp_mutex_lock (&newtm_trace_lock);
  assert (newtm_trace_users == 1 && newtm_trace_table != NULL);
  gomp_mutex_unlock (&newtm_trace_lock);
  funlockfile (stdout);
  assert (pthread_join (thread, NULL) == 0);
  check_closed ();
}

static void
check_residual_entries (void)
{
  struct newtm_hash_owner_lease lease = { 0 };
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_hash_owner_release (&lease) == NEWTM_HASH_OK);
  /* Model an orphan only through quiescent, test-only owner setup. No valid
     client is allowed to abandon references when releasing its lease. */
  struct newtm_hash_entry orphan = {
    .address = &object, .size = sizeof (object), .references = 1
  };
  gomp_mutex_lock (&newtm_trace_lock);
  newtm_trace_state = NEWTM_OWNER_CLOSING;
  struct newtm_hash_table *table = newtm_trace_table;
  table->buckets[0].entries = &orphan;
  table->buckets[0].entry_count = 1;
  gomp_mutex_unlock (&newtm_trace_lock);
  assert (newtm_hash_owner_close () == NEWTM_HASH_BUSY);
  gomp_mutex_lock (&newtm_trace_lock);
  assert (newtm_trace_state == NEWTM_OWNER_CLOSING);
  assert (newtm_trace_table == table && !newtm_trace_destroying);
  assert (table->buckets[0].entries == &orphan && orphan.references == 1);
  table->buckets[0].entries = NULL;
  table->buckets[0].entry_count = 0;
  gomp_mutex_unlock (&newtm_trace_lock);
  check_closed ();
}

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
static bool destructor_entered, finish_destructor, worker_finished;
static struct newtm_hash_owner_lease cleaning_lease;
static struct newtm_hash_entry *cleaning_entry;

static void
install_metadata (const void *address, size_t size, void **slot, void *argument)
{
  assert (address != NULL && size != 0 && *slot == NULL);
  *slot = argument;
}

static void
gated_destructor (void *data)
{
  assert (pthread_mutex_lock (&gate) == 0);
  destructor_entered = true;
  assert (pthread_cond_signal (&condition) == 0);
  while (!finish_destructor)
    assert (pthread_cond_wait (&condition, &gate) == 0);
  assert (pthread_mutex_unlock (&gate) == 0);
  free (data);
}

static void *
cleanup_worker (void *argument)
{
  (void) argument;
  newtm_hash_release (cleaning_lease.table, cleaning_entry);
  assert (newtm_hash_owner_release (&cleaning_lease) == NEWTM_HASH_OK);
  assert (pthread_mutex_lock (&gate) == 0);
  worker_finished = true;
  assert (pthread_mutex_unlock (&gate) == 0);
  return NULL;
}

static void
check_pending_cleanup (void)
{
  assert (newtm_hash_owner_acquire (&cleaning_lease) == NEWTM_HASH_OK);
  /* Test-only setup of a policy for the otherwise metadata-free bridge. */
  cleaning_lease.table->destroy_policy = gated_destructor;
  assert (newtm_hash_acquire (cleaning_lease.table, &object, sizeof (object),
			    &cleaning_entry) == NEWTM_HASH_OK);
  void *data = malloc (1);
  assert (data != NULL);
  newtm_hash_entry_visit (cleaning_lease.table, cleaning_entry,
			install_metadata, data);
  pthread_t thread;
  assert (pthread_create (&thread, NULL, cleanup_worker, NULL) == 0);
  assert (pthread_mutex_lock (&gate) == 0);
  while (!destructor_entered)
    assert (pthread_cond_wait (&condition, &gate) == 0);
  assert (pthread_mutex_unlock (&gate) == 0);
  assert (newtm_hash_owner_close () == NEWTM_HASH_BUSY);
  /* Main has no admitted table access: inspect only the stable owner control. */
  gomp_mutex_lock (&newtm_trace_lock);
  assert (newtm_trace_users == 1 && newtm_trace_table != NULL);
  gomp_mutex_unlock (&newtm_trace_lock);
  assert (pthread_mutex_lock (&gate) == 0);
  assert (!worker_finished);
  finish_destructor = true;
  assert (pthread_cond_signal (&condition) == 0);
  assert (pthread_mutex_unlock (&gate) == 0);
  assert (pthread_join (thread, NULL) == 0);
  assert (worker_finished);
  check_closed ();
  assert (pthread_cond_destroy (&condition) == 0);
  assert (pthread_mutex_destroy (&gate) == 0);
}

enum { WORKERS = 8 };
static pthread_barrier_t barrier;
static enum newtm_hash_status admissions[WORKERS];
static int objects[WORKERS];

static void
wait_barrier (void)
{
  int status = pthread_barrier_wait (&barrier);
  assert (status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void *
admission_worker (void *argument)
{
  size_t id = (size_t) argument;
  struct newtm_hash_owner_lease lease = { 0 };
  struct newtm_hash_entry *entry = NULL;
  wait_barrier ();
  admissions[id] = newtm_hash_owner_acquire (&lease);
  if (admissions[id] == NEWTM_HASH_OK)
    assert (newtm_hash_acquire (lease.table, &objects[id], sizeof (int), &entry)
	    == NEWTM_HASH_OK);
  else
    assert (admissions[id] == NEWTM_HASH_BUSY && !lease.active);
  wait_barrier ();
  wait_barrier ();
  if (lease.active)
    {
      newtm_hash_release (lease.table, entry);
      assert (newtm_hash_owner_release (&lease) == NEWTM_HASH_OK);
    }
  return NULL;
}

static void
check_admission_race (void)
{
  struct newtm_hash_owner_lease sentinel = { 0 };
  assert (newtm_hash_owner_acquire (&sentinel) == NEWTM_HASH_OK);
  assert (pthread_barrier_init (&barrier, NULL, WORKERS + 1) == 0);
  pthread_t threads[WORKERS];
  for (size_t i = 0; i < WORKERS; ++i)
    assert (pthread_create (&threads[i], NULL, admission_worker, (void *) i) == 0);
  wait_barrier ();
  assert (newtm_hash_owner_close () == NEWTM_HASH_BUSY);
  wait_barrier ();
  size_t admitted = 1;
  for (size_t i = 0; i < WORKERS; ++i)
    admitted += admissions[i] == NEWTM_HASH_OK;
  gomp_mutex_lock (&newtm_trace_lock);
  assert (newtm_trace_users == admitted && newtm_trace_state == NEWTM_OWNER_CLOSING);
  gomp_mutex_unlock (&newtm_trace_lock);
  wait_barrier ();
  for (size_t i = 0; i < WORKERS; ++i)
    assert (pthread_join (threads[i], NULL) == 0);
  assert (pthread_barrier_destroy (&barrier) == 0);
  assert (newtm_hash_owner_release (&sentinel) == NEWTM_HASH_OK);
  check_closed ();
}

static void
check_lease_errors (void)
{
  struct newtm_hash_owner_lease lease = { 0 }, rejected = { 0 };
  assert (newtm_hash_owner_acquire (NULL) == NEWTM_HASH_INVALID);
  assert (newtm_hash_owner_release (NULL) == NEWTM_HASH_INVALID);
  assert (newtm_hash_owner_release (&lease) == NEWTM_HASH_INVALID);
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_OK);
  assert (newtm_hash_owner_acquire (&lease) == NEWTM_HASH_INVALID && lease.active);
  /* Test-only overflow injection, restored before any real release. */
  gomp_mutex_lock (&newtm_trace_lock);
  newtm_trace_users = SIZE_MAX;
  gomp_mutex_unlock (&newtm_trace_lock);
  assert (newtm_hash_owner_acquire (&rejected) == NEWTM_HASH_OVERFLOW);
  assert (!rejected.active && rejected.table == NULL);
  gomp_mutex_lock (&newtm_trace_lock);
  assert (newtm_trace_users == SIZE_MAX);
  newtm_trace_users = 1;
  gomp_mutex_unlock (&newtm_trace_lock);
  assert (newtm_hash_owner_release (&lease) == NEWTM_HASH_OK);
  assert (newtm_hash_owner_release (&lease) == NEWTM_HASH_INVALID);
  check_closed ();
}

int
main (int argc, char **argv)
{
  assert (argc == 2);
  if (strcmp (argv[1], "unused") == 0)
    check_closed ();
  else if (strcmp (argv[1], "nested") == 0)
    check_nested ();
  else if (strcmp (argv[1], "concurrent-close") == 0)
    check_concurrent_close ();
  else if (strcmp (argv[1], "creation-inflight") == 0)
    check_pending_creation_log ();
  else if (strcmp (argv[1], "residual-entries") == 0)
    check_residual_entries ();
  else if (strcmp (argv[1], "pending-cleanup") == 0)
    check_pending_cleanup ();
  else if (strcmp (argv[1], "admission-race") == 0)
    check_admission_race ();
  else if (strcmp (argv[1], "lease-errors") == 0)
    check_lease_errors ();
  else if (strcmp (argv[1], "late-begin") == 0)
    {
      check_closed ();
      GOMP_transaction_begin ();
      abort ();
    }
  else if (strcmp (argv[1], "exit-active") == 0)
    {
      GOMP_transaction_begin ();
      GOMP_transaction_register (&object, sizeof (object));
      puts ("[teste] exit with active participation");
      return 0; /* Automatic finalizer must neither wait nor force destruction. */
    }
  else if (strcmp (argv[1], "finalize-active") == 0)
    {
      GOMP_transaction_begin ();
      GOMP_transaction_register (&object, sizeof (object));
      newtm_trace_finalize ();
      assert (newtm_trace_state == NEWTM_OWNER_CLOSING && newtm_trace_users == 1);
      GOMP_transaction_begin ();
      GOMP_transaction_end ();
      GOMP_transaction_end ();
      check_closed ();
    }
  else
    abort ();
  puts ("[teste] hash owner OK");
  return 0;
}
