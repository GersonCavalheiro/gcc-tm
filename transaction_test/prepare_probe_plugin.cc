/* Inspect the live preparation window; invoke dormant region bodies in tests. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "context.h"
#include "cgraph.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "fold-const.h"
#include "ggc.h"
#include "newtm-clone.h"
#include "newtm-access.h"
#include "newtm-occurrence.h"
#include "diagnostic-core.h"
int plugin_is_GPL_compatible;
static unsigned bodies, invoked, accesses, refused, associations;
static unsigned storage_counts[4];
static bool require_ready;
static tree inspect (gimple_stmt_iterator *it, bool *, walk_stmt_info *)
{
  gimple *stmt = gsi_stmt (*it);
  if (tree body = newtm_clone_region_body (stmt))
    {
      gcc_assert (gimple_code (stmt) == GIMPLE_OMP_TRANSACTION);
      gcc_assert (newtm_clone_region_owner (body) == current_function_decl);
      gimple *copy = gimple_copy (stmt);
      gcc_assert (!newtm_clone_region_body (copy));
      newtm_occurrence_retire (gimple_omp_body (copy));
      newtm_descriptor_retire (gimple_omp_body (copy));
      newtm_occurrence_forget (copy);
      ++associations;
    }
  if (newtm_clone_region_owner (current_function_decl)
      && newtm_occurrence_binding (stmt))
    {
      newtm_access access;
      gcc_assert (!newtm_access_describe (stmt, NEWTM_DESC_READ, 0, &access));
    }
  if (!is_gimple_call (stmt)) return NULL_TREE;
  tree fn = gimple_call_fndecl (stmt);
  if (!fn || !DECL_NAME (fn) || strcmp (IDENTIFIER_POINTER (DECL_NAME (fn)), "probe_body"))
    return NULL_TREE;
  cgraph_node *node;
  tree body = NULL_TREE;
  FOR_EACH_FUNCTION (node)
    if (newtm_clone_region_owner (node->decl) == current_function_decl)
      { gcc_assert (!body); body = node->decl; }
  gcc_assert (body);
  auto_vec<tree> args;
  for (unsigned i = 0; i < gimple_call_num_args (stmt); ++i)
    args.safe_push (gimple_call_arg (stmt, i));
  for (tree p = newtm_clone_captures (body); p; p = TREE_CHAIN (p))
    args.safe_push (build_fold_addr_expr (TREE_VALUE (p)));
  gcall *call = gimple_build_call_vec (body, args);
  gimple_call_set_lhs (call, gimple_call_lhs (stmt));
  gimple_set_location (call, gimple_location (stmt));
  gsi_replace (it, call, false);
  ++invoked;
  return NULL_TREE;
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_prepare_probe", OPTGROUP_NONE,
                         TV_NONE, 0, 0, 0, 0, 0 };
class probe : public gimple_opt_pass {
public:
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *) final override {
    ggc_collect (GGC_COLLECT_FORCE);
    if (newtm_clone_region_owner (current_function_decl))
      {
        ++bodies;
        for (unsigned i = 0; i < 4; ++i)
          storage_counts[i] += newtm_access_count (current_function_decl, (newtm_access_storage) i);
        accesses += newtm_access_count (current_function_decl, NEWTM_ACCESS_REGISTERED)
          + newtm_access_count (current_function_decl, NEWTM_ACCESS_PRIVATE)
          + newtm_access_count (current_function_decl, NEWTM_ACCESS_DYNAMIC);
        const char *late = NULL;
        gcc_assert (!newtm_access_begin (current_function_decl));
        gcc_assert (!newtm_access_preflight (current_function_decl, &late) && late);
        gcc_assert (!TREE_PUBLIC (current_function_decl));
        gcc_assert (TREE_TYPE (TREE_TYPE (current_function_decl)) == uint32_type_node);
        if (newtm_clone_region_rejection (current_function_decl)) ++refused;
        if (require_ready) newtm_clone_require_region (current_function_decl);
      }
    walk_stmt_info wi = {};
    walk_gimple_seq (gimple_body (current_function_decl), inspect, NULL, &wi);
    return 0;
  }
};
}
static void finish (void *, void *) {
  if (bodies != 8 || invoked != 6 || !accesses || !refused || associations != 8
      || !storage_counts[NEWTM_ACCESS_REGISTERED] || !storage_counts[NEWTM_ACCESS_PRIVATE]
      || !storage_counts[NEWTM_ACCESS_DYNAMIC] || !storage_counts[NEWTM_ACCESS_REJECTED])
    error ("preparation probe counts: bodies=%u invoked=%u accesses=%u refused=%u", bodies, invoked, accesses, refused);
}
int plugin_init (plugin_name_args *info, plugin_gcc_version *version) {
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < info->argc; ++i)
    if (!strcmp (info->argv[i].key, "require")) require_ready = true;
  register_pass_info p = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (info->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
