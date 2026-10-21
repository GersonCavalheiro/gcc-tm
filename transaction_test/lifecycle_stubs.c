/* Contract checks for trace-only lifecycle stubs, not a functional context.
   Link the static libgomp archive to exercise its hidden internal symbols.  */
#include "newtm-transaction.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>

int
main (void)
{
  /* Valid, aligned storage passed opaquely: the stub must not inspect it.
     Do not define struct newtm_context here or simulate its future layout.  */
  union { max_align_t alignment; unsigned char bytes[64]; } context, table_data;
  unsigned char saved_context[sizeof context], saved_table[sizeof table_data];
  unsigned char object[32], saved_object[sizeof object];
  memset (&context, 0xa5, sizeof context);
  memset (&table_data, 0x5a, sizeof table_data);
  memset (object, 0x33, sizeof object);
  memcpy (saved_context, &context, sizeof context);
  memcpy (saved_table, &table_data, sizeof table_data);
  memcpy (saved_object, object, sizeof object);
  struct newtm_context *opaque = (struct newtm_context *) &context;
  struct newtm_context *slot = opaque, *empty = NULL;
  struct newtm_hash_table *table = (struct newtm_hash_table *) &table_data;
  bool outermost = true;

#define CHECK(call) do { \
  assert ((call) == NEWTM_LIFECYCLE_INVALID); \
  assert (slot == opaque && empty == NULL); \
  assert (memcmp (&context, saved_context, sizeof context) == 0); \
  assert (memcmp (&table_data, saved_table, sizeof table_data) == 0); \
  assert (memcmp (object, saved_object, sizeof object) == 0); \
} while (0)

  CHECK (newtm_begin (table, &slot, &outermost));
  assert (!outermost);
  outermost = true;
  CHECK (newtm_begin (table, &empty, &outermost));
  assert (!outermost);
  outermost = true;
  CHECK (newtm_begin (NULL, NULL, &outermost));
  assert (!outermost);
  CHECK (newtm_begin (table, &slot, NULL));
  CHECK (newtm_begin (NULL, NULL, NULL));

  CHECK (newtm_register (slot, object, sizeof object));
  CHECK (newtm_register (NULL, NULL, 0));
  CHECK (newtm_register (slot, object, SIZE_MAX));

  CHECK (newtm_end (&slot));
  CHECK (newtm_end (&empty));
  CHECK (newtm_end (NULL));

  CHECK (newtm_fail (slot, NEWTM_ACCESS_CONFLICT));
  CHECK (newtm_fail (slot, NEWTM_ACCESS_INVALID));
  CHECK (newtm_fail (slot, NEWTM_ACCESS_OK));
  CHECK (newtm_fail (slot, (enum newtm_access_status) 99));
  CHECK (newtm_fail (NULL, NEWTM_ACCESS_CONFLICT));

  CHECK (newtm_discard (&slot));
  /* Repeated disposal/exit must not manufacture success or consume the opaque
     handle.  No actual cleanup or CLOSED transition occurs in a stub.  */
  CHECK (newtm_discard (&slot));
  CHECK (newtm_end (&slot));
  CHECK (newtm_discard (&empty));
  CHECK (newtm_discard (NULL));

  CHECK (newtm_backoff (0));
  CHECK (newtm_backoff (1));
  CHECK (newtm_backoff (SIZE_MAX));
  printf ("[teste] lifecycle stubs OK; size_max=%zu\n", (size_t) SIZE_MAX);
  return 0;
}
