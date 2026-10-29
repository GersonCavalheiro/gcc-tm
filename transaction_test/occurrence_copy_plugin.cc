/* Test-only consumer of High-GIMPLE occurrence bindings.  Copies are NOT
   inserted into the program: changing a copied operand must leave the original
   intact, and binding/dump consumers must see the copy's current operands. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "context.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "gimple-pretty-print.h"
#include "gimplify.h"
#include "fold-const.h"
#include "ggc.h"
#include "newtm-occurrence.h"
#include "diagnostic-core.h"

int plugin_is_GPL_compatible;
static unsigned checked_stores, checked_calls;

static void check_identity (gimple *original, gimple *copy)
{
  tree a = newtm_occurrence_binding (original), b = newtm_occurrence_binding (copy);
  gcc_assert (a && b && a != b);
  gcc_assert (TREE_VEC_ELT (a, 10) == TREE_VEC_ELT (b, 10));
  gcc_assert (TREE_VEC_ELT (TREE_VEC_ELT (a, 10), 6) != NULL_TREE);
  gcc_assert (!operand_equal_p (TREE_VEC_ELT (a, 0), TREE_VEC_ELT (b, 0), 0));
  gcc_assert (operand_equal_p (TREE_VEC_ELT (a, 1), TREE_VEC_ELT (b, 1), 0));
  gcc_assert (operand_equal_p (TREE_VEC_ELT (a, 0), TREE_VEC_ELT (b, 8), 0));
}

static tree check_stmt (gimple_stmt_iterator *gsi, bool *, walk_stmt_info *)
{
  gimple *stmt = gsi_stmt (*gsi);
  if (!newtm_occurrence_binding (stmt)) return NULL_TREE;
  if (!checked_stores && is_gimple_assign (stmt)
      && TREE_CODE (gimple_assign_lhs (stmt)) == ARRAY_REF)
    {
      tree original = gimple_assign_lhs (stmt);
      tree old_index = TREE_OPERAND (original, 1);
      if (TREE_CODE (old_index) == INTEGER_CST) return NULL_TREE;
      gimple *copy = gimple_copy (stmt);
      check_identity (stmt, copy);
      tree replacement = unshare_expr (original);
      TREE_OPERAND (replacement, 1) = build_zero_cst (TREE_TYPE (old_index));
      gimple_assign_set_lhs (copy, replacement);
      gcc_assert (TREE_OPERAND (gimple_assign_lhs (stmt), 1) == old_index);
      gcc_assert (integer_zerop (TREE_OPERAND (gimple_assign_lhs (copy), 1)));
      fprintf (stderr, "newtm-copy-store: ");
      print_gimple_stmt (stderr, copy, 0, TDF_NONE);
      ++checked_stores;
    }
  if (!checked_calls && is_gimple_call (stmt))
    {
      tree fn = gimple_call_fndecl (stmt);
      if (!fn || !DECL_NAME (fn)
          || strcmp (IDENTIFIER_POINTER (DECL_NAME (fn)), "combine")) return NULL_TREE;
      tree original_arg = gimple_call_arg (stmt, 0);
      gimple *copy = gimple_copy (stmt);
      check_identity (stmt, copy);
      gimple_call_set_arg (copy, 0, build_int_cst (TREE_TYPE (original_arg), 99));
      gcc_assert (gimple_call_arg (stmt, 0) == original_arg);
      fprintf (stderr, "newtm-copy-call: ");
      print_gimple_stmt (stderr, copy, 0, TDF_NONE);
      ++checked_calls;
    }
  return NULL_TREE;
}

namespace {
const pass_data data = { GIMPLE_PASS, "newtm_copy_probe", OPTGROUP_NONE,
                         TV_NONE, 0, 0, 0, 0, 0 };
class copy_probe : public gimple_opt_pass
{
public:
  copy_probe (gcc::context *ctxt) : gimple_opt_pass (data, ctxt) {}
  unsigned int execute (function *) final override
  {
    /* Bindings and source sites must remain rooted after GENERIC retirement. */
    ggc_collect (GGC_COLLECT_FORCE);
    walk_stmt_info wi = {};
    walk_gimple_seq (gimple_body (current_function_decl), check_stmt, NULL, &wi);
    return 0;
  }
};
}
static void finish (void *, void *)
{
  if (checked_stores != 1 || checked_calls != 1)
    error ("newtm copy probe did not inspect both a store and a call");
}
int plugin_init (plugin_name_args *info, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  register_pass_info pass = { new copy_probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass);
  register_callback (info->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
