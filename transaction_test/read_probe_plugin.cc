/* Test-only entry into prepared read bodies; no production activation. */
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
static unsigned invoked, reads, split_stores;
static bool expect_split_store;
static tree inspect (gimple_stmt_iterator *it, bool *, walk_stmt_info *)
{
  gimple *stmt = gsi_stmt (*it);
  if (newtm_clone_original (current_function_decl))
    {
      /* Only statically private source reads may retain ordinary metadata. */
      if (tree d = newtm_descriptor_find (stmt, NEWTM_DESC_READ))
        gcc_assert (TREE_CODE (TREE_VEC_ELT (d, NEWTM_D_STORAGE)) == INTEGER_CST
                    && tree_to_uhwi (TREE_VEC_ELT (d, NEWTM_D_STORAGE)) == 1);
      if (tree d = newtm_descriptor_find (stmt, NEWTM_DESC_WRITE))
        gcc_assert (TREE_CODE (TREE_VEC_ELT (d, NEWTM_D_STORAGE)) == INTEGER_CST
                    && tree_to_uhwi (TREE_VEC_ELT (d, NEWTM_D_STORAGE)) == 1);
    }
  if (!is_gimple_call (stmt)) return NULL_TREE;
  tree fn = gimple_call_fndecl (stmt);
  if (!fn || !DECL_NAME (fn)) return NULL_TREE;
  if (newtm_clone_original (current_function_decl)
      && fndecl_built_in_p (fn, BUILT_IN_GOMP_TRANSACTION_STM_V1_READ)) ++reads;
  if (newtm_clone_original (current_function_decl)
      && fndecl_built_in_p (fn, BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE))
    {
      tree original = newtm_clone_original (current_function_decl);
      if (DECL_NAME (original) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), "copy_pair"))
        ++split_stores;
    }
  const char *name = IDENTIFIER_POINTER (DECL_NAME (fn));
  if (!strncmp (name, "invoke_", 7))
    {
      cgraph_node *node; tree clone = NULL_TREE;
      FOR_EACH_FUNCTION (node)
        if (tree original = newtm_clone_original (node->decl))
          if (DECL_NAME (original) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), name + 7))
            clone = node->decl;
      gcc_assert (clone);
      gimple_call_set_fndecl (stmt, clone);
      gimple_call_set_fntype (as_a<gcall *> (stmt), TREE_TYPE (clone));
      ++invoked;
    }
  else if (!strcmp (name, "probe_read_body"))
    {
      cgraph_node *node; tree helper = NULL_TREE;
      FOR_EACH_FUNCTION (node)
        if (newtm_clone_region_owner (node->decl) == current_function_decl)
          { gcc_assert (!helper); helper = node->decl; }
      gcc_assert (helper && !newtm_clone_region_rejection (helper));
      auto_vec<tree> args;
      for (unsigned i = 0; i < gimple_call_num_args (stmt); ++i) args.safe_push (gimple_call_arg (stmt, i));
      for (tree p = newtm_clone_captures (helper); p; p = TREE_CHAIN (p))
        args.safe_push (build_fold_addr_expr (TREE_VALUE (p)));
      gcall *call = gimple_build_call_vec (helper, args);
      gimple_call_set_lhs (call, gimple_call_lhs (stmt));
      gimple_set_location (call, gimple_location (stmt));
      gsi_replace (it, call, false);
      ++invoked;
    }
  return NULL_TREE;
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_read_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
class probe : public gimple_opt_pass {
public:
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *) final override {
    ggc_collect (GGC_COLLECT_FORCE);
    tree original = newtm_clone_original (current_function_decl);
    if (original && DECL_NAME (original)
        && !strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), "copy_pair"))
      expect_split_store = true;
    if (const char *reason = newtm_clone_region_rejection (current_function_decl))
      fprintf (stderr, "read-probe refusal %s: %s\n", IDENTIFIER_POINTER (DECL_NAME (current_function_decl)), reason);
    walk_stmt_info wi = {};
    walk_gimple_seq (gimple_body (current_function_decl), inspect, NULL, &wi);
    return 0;
  }
};
}
static void finish (void *, void *) {
  if (!invoked || !reads || (expect_split_store && !split_stores))
    error ("read probe did not exercise generated reads and split stores");
}
int plugin_init (plugin_name_args *info, plugin_gcc_version *version) {
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  register_pass_info p = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (info->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
