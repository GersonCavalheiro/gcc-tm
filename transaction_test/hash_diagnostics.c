/* Deterministic regressions for stdio/runtime lock inversion.  Interpose only
   in this white-box test, without production hooks or ABI changes. */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include "../gcc-src-tm/libgomp/libgomp.h"

static void *test_malloc (size_t);
static void *test_calloc (size_t, size_t);
static int test_printf (const char *, ...);
extern void test_fatal (const char *, ...) __attribute__ ((noreturn));

#define malloc test_malloc
#define calloc test_calloc
#define printf test_printf
#define gomp_fatal test_fatal
#include "../gcc-src-tm/libgomp/newtm-hash.c"
#undef gomp_fatal
#undef printf
#undef calloc
#undef malloc

#undef NDEBUG
#include <assert.h>

enum test_mode { INIT_STDOUT, ENTRY_OOM, TABLE_OOM, BUCKETS_OOM };
static enum test_mode mode;
static _Thread_local bool fail_malloc, fail_calloc;
static int create_log_entered, fatal_entered, create_logs;
static struct newtm_hash_table *local_table;
static int objects[2];

static void *
test_malloc (size_t size)
{
  if (fail_malloc)
    {
      fail_malloc = false;
      return NULL;
    }
  return malloc (size);
}

static void *
test_calloc (size_t count, size_t size)
{
  if (fail_calloc)
    {
      fail_calloc = false;
      return NULL;
    }
  return calloc (count, size);
}

static int
test_printf (const char *format, ...)
{
  if (strstr (format, "[newtm] newtm_hash_create:") != NULL)
    {
      __atomic_add_fetch (&create_logs, 1, __ATOMIC_RELAXED);
      __atomic_store_n (&create_log_entered, 1, __ATOMIC_RELEASE);
    }
  va_list args;
  va_start (args, format);
  int result = vprintf (format, args);
  va_end (args);
  return result;
}

void
test_fatal (const char *format, ...)
{
  /* Linux mutex inspection is test-only. Main has not started its competing
     operation yet, so zero specifically verifies release before diagnostics. */
  if (mode == ENTRY_OOM)
    {
      assert (__atomic_load_n (&local_table->buckets[0].mutex,
			      __ATOMIC_RELAXED) == 0);
      assert (local_table->buckets[0].entries == NULL);
      assert (local_table->buckets[0].entry_count == 0);
    }
  else
    assert (__atomic_load_n (&newtm_trace_lock, __ATOMIC_RELAXED) == 0);
  __atomic_store_n (&fatal_entered, 1, __ATOMIC_RELEASE);
  va_list args;
  va_start (args, format);
  gomp_vfatal (format, args);
  abort ();
}

static void *
worker (void *argument)
{
  (void) argument;
  if (mode == ENTRY_OOM)
    {
      struct newtm_hash_entry *entry = (void *) (uintptr_t) 1;
      fail_malloc = true;
      newtm_hash_acquire (local_table, &objects[0], sizeof (int), &entry);
      abort (); /* Failure must be fatal, not a recoverable result. */
    }
  fail_malloc = mode == TABLE_OOM;
  fail_calloc = mode == BUCKETS_OOM;
  GOMP_transaction_begin ();
  GOMP_transaction_end ();
  return NULL;
}

int
main (int argc, char **argv)
{
  assert (argc == 2);
  if (strcmp (argv[1], "init-stdout") == 0)
    mode = INIT_STDOUT;
  else if (strcmp (argv[1], "entry-oom") == 0)
    mode = ENTRY_OOM;
  else if (strcmp (argv[1], "table-oom") == 0)
    mode = TABLE_OOM;
  else
    {
      assert (strcmp (argv[1], "buckets-oom") == 0);
      mode = BUCKETS_OOM;
    }

  pthread_t thread;
  if (mode == INIT_STDOUT)
    {
      flockfile (stdout);
      assert (pthread_create (&thread, NULL, worker, NULL) == 0);
      while (!__atomic_load_n (&create_log_entered, __ATOMIC_ACQUIRE))
	sched_yield ();
      /* Worker is about to wait for stdout. We must still be able to enter
	 and leave the initialized bridge while owning that stream lock. */
      assert (__atomic_load_n (&newtm_trace_lock, __ATOMIC_RELAXED) == 0);
      GOMP_transaction_begin ();
      GOMP_transaction_end ();
      funlockfile (stdout);
      assert (pthread_join (thread, NULL) == 0);
      assert (__atomic_load_n (&create_logs, __ATOMIC_RELAXED) == 1);
      puts ("[teste] init stdout without lock inversion OK");
      return 0;
    }

  if (mode == ENTRY_OOM)
    assert (newtm_hash_create (1, NULL, &local_table) == NEWTM_HASH_OK);
  flockfile (stderr);
  assert (pthread_create (&thread, NULL, worker, NULL) == 0);
  while (!__atomic_load_n (&fatal_entered, __ATOMIC_ACQUIRE))
    sched_yield ();
  /* The diagnostic thread waits for stderr. Same-bucket acquisition, or lazy
     initialization after a failed attempt, must complete before releasing it. */
  if (mode == ENTRY_OOM)
    {
      struct newtm_hash_entry *entry;
      assert (newtm_hash_acquire (local_table, &objects[1], sizeof (int), &entry)
	      == NEWTM_HASH_OK);
      newtm_hash_release (local_table, entry);
    }
  else
    {
      assert (!newtm_trace_initialized && newtm_trace_table == NULL);
      GOMP_transaction_begin ();
      GOMP_transaction_end ();
    }
  puts ("[teste] fatal stderr without lock inversion OK");
  fflush (stdout);
  funlockfile (stderr);
  /* Worker terminates the process with EXIT_FAILURE. Do not destroy its table
     or manufacture quiescence while its fatal operation is still in flight. */
  assert (pthread_join (thread, NULL) == 0);
  return 2;
}
