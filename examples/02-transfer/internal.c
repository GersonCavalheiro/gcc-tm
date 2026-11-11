/* Transfer — version internal. See index.html. */
#include <stdio.h>
#include <stdlib.h>
#include "internal.h"
static int source = 10000, destination = 2000;
static enum newtm_lifecycle_status body(struct newtm_context *ctx, void *argument)
{
  (void)argument;
  REG(source);
  REG(destination);
  int a, b;
  READ(source, a);
  READ(destination, b);
  if (a >= 2500)
  {
    a -= 2500;
    b += 2500;
    WRITE(source, a);
    WRITE(destination, b);
  }
  return NEWTM_LIFECYCLE_OK;
}
int main(void)
{
  execute_body(body, NULL);
  if (source != 7500 || destination != 4500 || source + destination != 12000) return EXIT_FAILURE;
  printf("RESULT source=%d destination=%d total=%d\n", source, destination, source + destination);
  return EXIT_SUCCESS;
}
