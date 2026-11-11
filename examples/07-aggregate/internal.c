/* Agregado e ranges — version internal. See index.html. */
#include <stdio.h>
#include <stdlib.h>
#include "internal.h"
struct accounts
{
  int source;
  int destination;
};
static struct accounts accounts =
{
  10000, 2000
};
static enum newtm_lifecycle_status body(struct newtm_context *ctx, void *argument)
{
  (void)argument;
  LIFECYCLE(newtm_register(ctx, &accounts, sizeof accounts, NEWTM_OBJECT_AGGREGATE));
  int a, b;
  ACCESS(newtm_read(ctx, &accounts, offsetof(struct accounts, source), &a, sizeof a));
  ACCESS(newtm_read(ctx, &accounts, offsetof(struct accounts, destination), &b, sizeof b));
  a -= 2500;
  b += 2500;
  ACCESS(newtm_write(ctx, &accounts, offsetof(struct accounts, source), &a, sizeof a));
  ACCESS(newtm_write(ctx, &accounts, offsetof(struct accounts, destination), &b, sizeof b));
  return NEWTM_LIFECYCLE_OK;
}
int main(void)
{
  execute_body(body, NULL);
  if (accounts.source != 7500 || accounts.destination != 4500 || accounts.source + accounts.destination != 12000) return EXIT_FAILURE;
  printf("RESULT source=%d destination=%d total=%d\n", accounts.source, accounts.destination, accounts.source + accounts.destination);
  return EXIT_SUCCESS;
}
