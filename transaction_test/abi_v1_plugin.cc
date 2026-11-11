/* Test-only consumer before omplower: exercise production type/call builders.
   This is not the production pragma instrumentation pass. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "context.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "ggc.h"
#include "diagnostic-core.h"
#include "newtm-abi.h"
#include "newtm-descriptor.h"
int plugin_is_GPL_compatible;
static unsigned seen[11];
static const built_in_function operations[] = {
  BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN,
  BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER,
  BUILT_IN_GOMP_TRANSACTION_STM_V1_READ,
  BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE,
  BUILT_IN_GOMP_TRANSACTION_STM_V1_END,
  BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL,
  BUILT_IN_GOMP_TRANSACTION_STM_V1_DISCARD,
  BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE,
  BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN,
  BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END,
  BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS
};
static tree rewrite (gimple_stmt_iterator *it, bool *, walk_stmt_info *)
{
  gimple *stmt = gsi_stmt (*it);
  if (!is_gimple_call (stmt)) return NULL_TREE;
  tree fn = gimple_call_fndecl (stmt);
  if (!fn || !DECL_NAME (fn)) return NULL_TREE;
  const char *name = IDENTIFIER_POINTER (DECL_NAME (fn));
  for (unsigned i = 0; i < 11; ++i)
    {
      tree builtin = builtin_decl_explicit (operations[i]);
      const char *builtin_name = IDENTIFIER_POINTER (DECL_NAME (builtin));
      if (!strncmp (builtin_name, "__builtin_", 10)) builtin_name += 10;
      if (strcmp (name, builtin_name)) continue;
      auto_vec<tree> args;
      for (unsigned j = 0; j < gimple_call_num_args (stmt); ++j)
        args.safe_push (gimple_call_arg (stmt, j));
      tree lhs = gimple_call_lhs (stmt);
      if (!lhs) lhs = create_tmp_var (uint32_type_node, "abi_status");
      gcall *call = newtm_abi_build_call (operations[i], lhs,
                                        args.length (), args.address ());
      gimple_set_location (call, gimple_location (stmt));
      gsi_set_stmt (it, call);
      ++seen[i];
      break;
    }
  return NULL_TREE;
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_abi_probe", OPTGROUP_NONE,
                         TV_NONE, 0, 0, 0, 0, 0 };
class probe : public gimple_opt_pass {
public:
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *) final override {
    tree binding = newtm_abi_binding_type ();
    tree context = newtm_abi_context_type ();
    gcc_assert (TREE_CODE (context) == POINTER_TYPE);
    gcc_assert (TREE_CODE (TREE_TYPE (context)) == RECORD_TYPE);
    gcc_assert (!COMPLETE_TYPE_P (TREE_TYPE (context)));
    gcc_assert (TREE_TYPE (newtm_abi_slot_type ()) == context);
    ggc_collect (GGC_COLLECT_FORCE);
    gcc_assert (binding == newtm_abi_binding_type ());
    gcc_assert (context == newtm_abi_context_type ());
    gcc_assert (TYPE_PRECISION (uint32_type_node) == 32);
    if (!strcmp (IDENTIFIER_POINTER (DECL_NAME (current_function_decl)), "abi_layout"))
      {
        tree source = TREE_TYPE (TREE_TYPE (DECL_ARGUMENTS (current_function_decl)));
        gcc_assert (tree_int_cst_equal (TYPE_SIZE_UNIT (binding), TYPE_SIZE_UNIT (source)));
        gcc_assert (TYPE_ALIGN (binding) == TYPE_ALIGN (source));
        tree a = TYPE_FIELDS (binding), b = TYPE_FIELDS (source);
        for (; a && b; a = DECL_CHAIN (a), b = DECL_CHAIN (b))
          {
            gcc_assert (DECL_NAME (a) == DECL_NAME (b));
            gcc_assert (tree_int_cst_equal (DECL_FIELD_OFFSET (a), DECL_FIELD_OFFSET (b)));
            gcc_assert (tree_int_cst_equal (DECL_FIELD_BIT_OFFSET (a), DECL_FIELD_BIT_OFFSET (b)));
            gcc_assert (tree_int_cst_equal (TYPE_SIZE (TREE_TYPE (a)), TYPE_SIZE (TREE_TYPE (b))));
          }
        /* C++ appends its injected class-name TYPE_DECL. */
        while (b && TREE_CODE (b) != FIELD_DECL) b = DECL_CHAIN (b);
        gcc_assert (!a && !b);
      }
    walk_stmt_info wi = {};
    walk_gimple_seq (gimple_body (current_function_decl), rewrite, NULL, &wi);
    return 0;
  }
};
}
static void finish (void *, void *) {
  for (unsigned i = 0; i < 11; ++i)
    if (!seen[i]) error ("ABI test missed operation %u", i);
}
int plugin_init (plugin_name_args *info, plugin_gcc_version *version) {
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  register_pass_info p = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (info->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
