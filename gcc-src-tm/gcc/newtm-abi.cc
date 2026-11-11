/* Executable STM ABI types and GIMPLE call construction.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "fold-const.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "stor-layout.h"
#include "stringpool.h"
#include "newtm-abi.h"
#include "gimple-iterator.h"
#include "diagnostic-core.h"

static GTY(()) tree newtm_binding_type;
static GTY(()) tree newtm_context_pointer;
static GTY(()) tree newtm_clone_binding_type;

tree
newtm_abi_context_type ()
{
  if (!newtm_context_pointer)
    {
      tree type = make_node (RECORD_TYPE);
      TYPE_NAME (type) = get_identifier ("newtm_context");
      /* Deliberately incomplete: compiler code must not inspect runtime data. */
      newtm_context_pointer = build_pointer_type (type);
    }
  return newtm_context_pointer;
}

tree
newtm_abi_slot_type ()
{
  return build_pointer_type (newtm_abi_context_type ());
}

tree
newtm_abi_binding_type ()
{
  if (!newtm_binding_type)
    {
      tree type = make_node (RECORD_TYPE);
      TYPE_NAME (type) = get_identifier ("newtm_binding_v1");
      tree *tail = &TYPE_FIELDS (type);
#define NEWTM_ABI_PTR ptr_type_node
#define NEWTM_ABI_SIZE size_type_node
#define NEWTM_ABI_U32 uint32_type_node
#define NEWTM_ABI_FIELD(T, N) \
      { tree f = build_decl (UNKNOWN_LOCATION, FIELD_DECL, get_identifier (#N), \
                             NEWTM_ABI_##T); \
        DECL_CONTEXT (f) = type; *tail = f; tail = &DECL_CHAIN (f); }
      NEWTM_ABI_BINDING_FIELDS (NEWTM_ABI_FIELD)
#undef NEWTM_ABI_FIELD
#undef NEWTM_ABI_PTR
#undef NEWTM_ABI_SIZE
#undef NEWTM_ABI_U32
      layout_type (type);
      newtm_binding_type = type;
    }
  return newtm_binding_type;
}

tree
newtm_abi_clone_binding_type ()
{
  if (!newtm_clone_binding_type)
    {
      tree type = make_node (RECORD_TYPE);
      TYPE_NAME (type) = get_identifier ("newtm_clone_binding_v1");
      tree identity = build_decl (UNKNOWN_LOCATION, FIELD_DECL,
                                 get_identifier ("identity"), newtm_abi_binding_type ());
      tree storage = build_decl (UNKNOWN_LOCATION, FIELD_DECL,
                                get_identifier ("storage"), uint32_type_node);
      DECL_CONTEXT (identity) = DECL_CONTEXT (storage) = type;
      TYPE_FIELDS (type) = identity;
      DECL_CHAIN (identity) = storage;
      layout_type (type);
      newtm_clone_binding_type = type;
    }
  return newtm_clone_binding_type;
}

namespace {
bool same_layout (tree a, tree b, unsigned depth = 0)
{
  if (!a || !b || depth > 8 || TREE_CODE (a) != TREE_CODE (b)
      || TYPE_ALIGN (a) != TYPE_ALIGN (b) || !TYPE_SIZE_UNIT (a) || !TYPE_SIZE_UNIT (b)
      || !operand_equal_p (TYPE_SIZE_UNIT (a), TYPE_SIZE_UNIT (b), 0)) return false;
  if (INTEGRAL_TYPE_P (a))
    return TYPE_PRECISION (a) == TYPE_PRECISION (b) && TYPE_UNSIGNED (a) == TYPE_UNSIGNED (b);
  if (POINTER_TYPE_P (a)) return TREE_CODE (TREE_TYPE (a)) == TREE_CODE (TREE_TYPE (b));
  if (TREE_CODE (a) != RECORD_TYPE || TYPE_PACKED (a) != TYPE_PACKED (b)
      || TYPE_REVERSE_STORAGE_ORDER (a) != TYPE_REVERSE_STORAGE_ORDER (b)) return false;
  tree fa = TYPE_FIELDS (a), fb = TYPE_FIELDS (b);
  while (fa && fb)
    {
      if (TREE_CODE (fa) != FIELD_DECL || TREE_CODE (fb) != FIELD_DECL
          || DECL_ALIGN (fa) != DECL_ALIGN (fb) || DECL_PACKED (fa) != DECL_PACKED (fb)
          || !operand_equal_p (DECL_FIELD_OFFSET (fa), DECL_FIELD_OFFSET (fb), 0)
          || !operand_equal_p (DECL_FIELD_BIT_OFFSET (fa), DECL_FIELD_BIT_OFFSET (fb), 0)
          || !same_layout (TREE_TYPE (fa), TREE_TYPE (fb), depth + 1)) return false;
      fa = DECL_CHAIN (fa); fb = DECL_CHAIN (fb);
    }
  return !fa && !fb;
}
}
bool newtm_abi_load_types (tree context, tree transport)
{
  if (!context || !POINTER_TYPE_P (context) || TREE_CODE (TREE_TYPE (context)) != RECORD_TYPE
      || COMPLETE_TYPE_P (TREE_TYPE (context)) || TYPE_READONLY (TREE_TYPE (context))
      || !transport || !POINTER_TYPE_P (transport) || !TYPE_READONLY (TREE_TYPE (transport))) return false;
  tree type = TYPE_MAIN_VARIANT (TREE_TYPE (transport));
  if (!same_layout (type, newtm_abi_clone_binding_type ())) return false;
  newtm_context_pointer = context;
  newtm_clone_binding_type = type;
  newtm_binding_type = TYPE_MAIN_VARIANT (TREE_TYPE (TYPE_FIELDS (type)));
  return true;
}

gcall *
newtm_abi_build_call (built_in_function operation, tree status,
                      unsigned nargs, tree *args)
{
  unsigned expected;
  switch (operation)
    {
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE: expected = 2; break;
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER: expected = 4; break;
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_READ:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE: expected = 5; break;
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_END:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_DISCARD: expected = 1; break;
    default: gcc_unreachable ();
    }
  gcc_assert (nargs == expected);
  gcc_assert (operation == BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL
              ? !status
              : status && TYPE_MAIN_VARIANT (TREE_TYPE (status))
                  == TYPE_MAIN_VARIANT (uint32_type_node));
  auto_vec<tree> operands;
  for (unsigned i = 0; i < nargs; ++i)
    {
      gcc_assert (is_gimple_val (args[i]));
      operands.safe_push (args[i]);
    }
  gcall *call = gimple_build_call_vec (builtin_decl_explicit (operation), operands);
  if (status) gimple_call_set_lhs (call, status);
  return call;
}
/* Roles are defined by the versioned ABI, independently of mutable DECL
   prototypes and of any retired descriptor or historical certificate. */
static const char *abi_roles (built_in_function code)
{
  switch (code)
    {
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN: return "pp";
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER: return "ppsu";
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_READ:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE: return "ppsps";
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL: return "pu";
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE: return "ps";
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL: return "u";
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_END:
    case BUILT_IN_GOMP_TRANSACTION_STM_V1_DISCARD: return "p";
    default: return NULL;
    }
}
static bool abi_role_type (tree type, char role)
{
  if (!type) return false;
  if (role == 'p')
    return POINTER_TYPE_P (type) && TYPE_ADDR_SPACE (TREE_TYPE (type)) == ADDR_SPACE_GENERIC;
  tree expected = role == 's' ? size_type_node : uint32_type_node;
  return TREE_CODE (type) == INTEGER_TYPE
    && TYPE_PRECISION (type) == TYPE_PRECISION (expected)
    && TYPE_UNSIGNED (type) == TYPE_UNSIGNED (expected);
}
static bool abi_prototype (tree type, const char *roles, bool fatal)
{
  if (!type || TREE_CODE (type) != FUNCTION_TYPE) return false;
  if (fatal ? TREE_TYPE (type) != void_type_node : !abi_role_type (TREE_TYPE (type), 'u')) return false;
  tree arg = TYPE_ARG_TYPES (type);
  for (const char *p = roles; *p; ++p)
    {
      if (!arg || !abi_role_type (TREE_VALUE (arg), *p)) return false;
      arg = TREE_CHAIN (arg);
    }
  return arg && TREE_VALUE (arg) == void_type_node && !TREE_CHAIN (arg);
}
bool newtm_abi_verify_cfg (function *fn)
{
  if (!fn || !fn->cfg) return true;
  bool valid = true;
  basic_block bb;
  FOR_EACH_BB_FN (bb, fn)
    for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
      {
        gimple *stmt = gsi_stmt (it);
        if (!is_gimple_call (stmt)) continue;
        tree callee = gimple_call_fndecl (stmt);
        if (!callee || !fndecl_built_in_p (callee, BUILT_IN_NORMAL)) continue;
        built_in_function code = DECL_FUNCTION_CODE (callee);
        const char *roles = abi_roles (code);
        if (!roles) continue;
        bool fatal = code == BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL;
        bool ok = gimple_call_num_args (stmt) == strlen (roles)
          && abi_prototype (TREE_TYPE (callee), roles, fatal)
          && abi_prototype (gimple_call_fntype (stmt), roles, fatal);
        for (unsigned i = 0; ok && roles[i]; ++i)
          ok = abi_role_type (TREE_TYPE (gimple_call_arg (stmt, i)), roles[i]);
        tree lhs = gimple_call_lhs (stmt);
        if (lhs && (fatal || !abi_role_type (TREE_TYPE (lhs), 'u'))) ok = false;
        if (!ok)
          { error_at (gimple_location (stmt), "transaction ABI mismatch in transformed call to %qD", callee); valid = false; }
      }
  return valid;
}
#include "gt-newtm-abi.h"
