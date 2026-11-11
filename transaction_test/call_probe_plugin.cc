/* Test-only invocation and transitive readiness of dormant prepared bodies. */
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
#include "diagnostic-core.h"
int plugin_is_GPL_compatible;
static bool require_graph;
static bool invoke_refused;
static unsigned invoked, prepared_calls;
static auto_vec<tree> required;
static tree inspect (gimple_stmt_iterator *it, bool *, walk_stmt_info *)
{
  gimple *stmt = gsi_stmt (*it);
  if (!is_gimple_call (stmt)) return NULL_TREE;
  tree callee = gimple_call_fndecl (stmt);
  if (!callee || !DECL_NAME (callee)) return NULL_TREE;
  if (newtm_clone_original (current_function_decl))
    {
      /* No original selected callee or publication marker may survive. */
      gcc_assert (!newtm_clone_for (callee));
      gcc_assert (strcmp (IDENTIFIER_POINTER (DECL_NAME (callee)), "__newtm_publish_result"));
      if (newtm_clone_original (callee)) ++prepared_calls;
    }
  const char *name = IDENTIFIER_POINTER (DECL_NAME (callee));
  tree target = NULL_TREE;
  cgraph_node *node;
  bool region = !strcmp (name, "probe_call_body");
  if (region)
    {
      FOR_EACH_FUNCTION (node)
        if (newtm_clone_region_owner (node->decl) == current_function_decl)
          { gcc_assert (!target); target = node->decl; }
    }
  else if (!strncmp (name, "invoke_", 7))
    {
      FOR_EACH_FUNCTION (node)
        if (tree original = newtm_clone_original (node->decl))
          if (DECL_NAME (original) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), name + 7))
            { gcc_assert (!target); target = node->decl; }
    }
  else return NULL_TREE;
  gcc_assert (target);
  /* The original owner can be lowered before its helper. Check the stable
     dependency summaries after every body has finished gimplification. */
  if (region && require_graph) required.safe_push (target);
  auto_vec<tree> args;
  for (unsigned i = 0; i < gimple_call_num_args (stmt); ++i)
    args.safe_push (gimple_call_arg (stmt, i));
  if (region)
    for (tree p = newtm_clone_captures (target); p; p = TREE_CHAIN (p))
      {
        /* A rejected body only calls fail(INVALID); do not reconstruct an
           original capture already remapped by tree-nested in this late test
           hook. No production activation is permitted for this body. */
        if (invoke_refused)
          args.safe_push (build_zero_cst (build_pointer_type (TREE_TYPE (TREE_VALUE (p)))));
        else args.safe_push (build_fold_addr_expr (TREE_VALUE (p)));
      }
  gcall *call = gimple_build_call_vec (target, args);
  gimple_call_set_lhs (call, gimple_call_lhs (stmt));
  gimple_set_location (call, gimple_location (stmt));
  gsi_replace (it, call, false);
  ++invoked;
  return NULL_TREE;
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_call_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
class probe : public gimple_opt_pass {
public:
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *) final override {
    ggc_collect (GGC_COLLECT_FORCE);
    walk_stmt_info wi = {};
    walk_gimple_seq (gimple_body (current_function_decl), inspect, NULL, &wi);
    return 0;
  }
};
}
static void finish (void *, void *)
{
  if (!seen_error () && (!invoked || (!prepared_calls && !invoke_refused)))
    error ("call probe did not exercise prepared calls");
}
static void check_graphs (void *, void *)
{
  for (tree fn : required) newtm_clone_require_region (fn);
  if (require_graph)
    {
      cgraph_node *node;
      FOR_EACH_FUNCTION (node)
        if (newtm_clone_region_owner (node->decl))
          newtm_clone_require_region (node->decl);
    }
}
int plugin_init (plugin_name_args *info, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < info->argc; ++i)
    {
      if (!strcmp (info->argv[i].key, "require")) require_graph = true;
      if (!strcmp (info->argv[i].key, "refused")) invoke_refused = true;
    }
  register_pass_info p = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (info->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  register_callback (info->base_name, PLUGIN_ALL_IPA_PASSES_START, check_graphs, NULL);
  return 0;
}
