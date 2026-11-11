/* Mutate real CFG; leave refusal to whole-unit composition, never a component. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimplify.h"
#include "fold-const.h"
#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "diagnostic-core.h"
#include "newtm-clone.h"
#include "newtm-current.h"
int plugin_is_GPL_compatible;
static const char *mode = "observe";
static bool changed, observed;
static bool named (tree d, const char *s)
{ return d && DECL_NAME (d) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (d)), s); }
static bool builtin (gimple *s, built_in_function code)
{ return is_gimple_call (s) && gimple_call_fndecl (s) && fndecl_built_in_p (gimple_call_fndecl (s), code); }
namespace {
const pass_data data = {GIMPLE_PASS, "integrated_counterproof", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0};
struct mutate : gimple_opt_pass {
  mutate (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (changed || !strcmp (mode, "observe") || !strcmp (mode, "retired")) return 0;
    tree original = newtm_clone_original (fn->decl);
    basic_block bb;
    if (!strcmp (mode, "private") && named (original, "transfer"))
      FOR_EACH_BB_FN (bb, fn)
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          {
            gimple *s = gsi_stmt (it);
            if (is_gimple_assign (s) && gimple_assign_single_p (s)
                && TREE_CODE (gimple_assign_rhs1 (s)) == MEM_REF
                && INTEGRAL_TYPE_P (TREE_TYPE (gimple_assign_rhs1 (s))))
              {
                /* Redirect an access on a proven PRIVATE arm to a distinct
                   incoming pointer, retaining the original guard/tag. */
                tree q = DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (DECL_ARGUMENTS (fn->decl))));
                tree ref = unshare_expr (gimple_assign_rhs1 (s));
                TREE_OPERAND (ref, 0) = fold_convert (TREE_TYPE (TREE_OPERAND (ref, 0)), q);
                gimple_assign_set_rhs_from_tree (&it, ref); changed = true; break;
              }
          }
    if (!strcmp (mode, "early") && named (original, "relay"))
      {
        gimple *read = NULL, *guard = NULL;
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
            if (builtin (gsi_stmt (it), BUILT_IN_GOMP_TRANSACTION_STM_V1_READ)) read = gsi_stmt (it);
        if (read)
          FOR_EACH_BB_FN (bb, fn)
            for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
              if (gimple_code (gsi_stmt (it)) == GIMPLE_COND
                  && gimple_cond_lhs (gsi_stmt (it)) == gimple_call_lhs (read)) guard = gsi_stmt (it);
        if (guard && TREE_CODE (gimple_call_arg (read, 3)) == ADDR_EXPR)
          {
            tree buffer = TREE_OPERAND (gimple_call_arg (read, 3), 0);
            gimple_stmt_iterator it = gsi_for_stmt (guard);
            gsi_insert_before (&it, gimple_build_assign (create_tmp_var (TREE_TYPE (buffer)), buffer), GSI_SAME_STMT);
            changed = true;
          }
      }
    if (!strcmp (mode, "drop_store") && named (original, "copy_pair"))
      FOR_EACH_BB_FN (bb, fn)
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          if (builtin (gsi_stmt (it), BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE))
            {
              tree status = gimple_call_lhs (gsi_stmt (it));
              gimple *replacement = gimple_build_assign (status, build_zero_cst (TREE_TYPE (status)));
              gimple_set_location (replacement, gimple_location (gsi_stmt (it)));
              gsi_replace (&it, replacement, true); changed = true; break;
            }
    if (changed) fprintf (stderr, "INTEGRATED_MUTATION %s applied\n", mode);
    return 0;
  }
};
}
static void ipa (void *, void *)
{
  if (seen_error ()) return;
  unsigned HOST_WIDE_INT count = newtm_current_count ();
  if (!count) { error ("counterproof lacks a current graph baseline"); return; }
  for (unsigned HOST_WIDE_INT i = 1; i <= count; ++i)
    {
      newtm_current_view v; newtm_current_get (i, &v);
      if (!v.current_approved || v.proved != NEWTM_CURRENT_ALL || v.dependencies)
        { error ("counterproof baseline is not approved"); return; }
    }
  observed = true;
  if (!strcmp (mode, "retired"))
    {
      newtm_current_view old; newtm_current_get (1, &old);
      tree before = newtm_current_ir (old.root);
      /* Close the actual proof window; attempt reuse on exactly the same IR. */
      newtm_current_end_frontend ();
      tree after = newtm_current_ir (old.root);
      if (!before || !after || TREE_STRING_LENGTH (before) != TREE_STRING_LENGTH (after)
          || memcmp (TREE_STRING_POINTER (before), TREE_STRING_POINTER (after), TREE_STRING_LENGTH (before)))
        { error ("retirement counterproof unexpectedly changed IR"); return; }
      newtm_current_get (1, &old);
      if (old.current_approved || old.state != NEWTM_CURRENT_INVALID)
        { error ("retired certificate still approved"); return; }
      changed = true; fputs ("INTEGRATED_MUTATION retired applied unchanged-IR\n", stderr);
      if (newtm_current_verify (old.root)) error ("retired evidence approved a new graph");
      newtm_current_view last; newtm_current_get (newtm_current_count (), &last);
      if (last.current_approved || last.state != NEWTM_CURRENT_REFUSED || last.proved)
        error ("retired evidence was not atomically refused");
      else fputs ("INTEGRATED_RETIRED refused old=INVALID new=REFUSED proved=0\n", stderr);
    }
  else if (!strcmp (mode, "observe")) fputs ("INTEGRATED_BASELINE approved\n", stderr);
  else error ("whole-unit composition accepted the mutated graph");
}
static void finish (void *, void *)
{
  if (strcmp (mode, "observe") && !changed) error ("integrated counterproof did not mutate");
  if (!strcmp (mode, "observe") && !observed) error ("integrated baseline not observed");
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_current_count (); ++i)
    { newtm_current_view v; newtm_current_get (i, &v);
      if (v.current_approved) error ("counterproof retained aggregate approval"); }
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i)
    if (!strcmp (args->argv[i].key, "mode")) mode = args->argv[i].value;
  register_pass_info p = {new mutate (g), "ompexp", 1, PASS_POS_INSERT_AFTER};
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (args->base_name, PLUGIN_ALL_IPA_PASSES_START, ipa, NULL);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
