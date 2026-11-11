/* Example helpers: keep access and lifecycle status types distinct. */
#ifndef EXAMPLES_INTERNAL_H
#define EXAMPLES_INTERNAL_H
#include "newtm-context.h"
#include "newtm-abi.h"
#include <stdlib.h>
static inline enum newtm_lifecycle_status
convert_access(enum newtm_access_status s)
{
  switch (s) {
  case NEWTM_ACCESS_OK: return NEWTM_LIFECYCLE_OK;
  case NEWTM_ACCESS_CONFLICT: return NEWTM_LIFECYCLE_CONFLICT;
  default: return NEWTM_LIFECYCLE_INVALID;
  }
}
/* Return immediately so failed operations cannot expose results to the caller. */
#define LIFECYCLE(expr) do { enum newtm_lifecycle_status s_ = (expr); \
  if (s_ != NEWTM_LIFECYCLE_OK) return s_; } while (0)
#define ACCESS(expr) do { enum newtm_access_status s_ = (expr); \
  if (s_ != NEWTM_ACCESS_OK) return convert_access(s_); } while (0)
#define REG(obj) LIFECYCLE(newtm_register(ctx, &(obj), sizeof(obj), NEWTM_OBJECT_SCALAR))
#define READ(obj, tmp) ACCESS(newtm_read(ctx, &(obj), 0, &(tmp), sizeof(tmp)))
#define WRITE(obj, tmp) ACCESS(newtm_write(ctx, &(obj), 0, &(tmp), sizeof(tmp)))
static inline void execute_body(newtm_body_fn body, void *argument)
{
  if (newtm_execute(body, argument) != NEWTM_LIFECYCLE_OK) abort();
}
#endif
