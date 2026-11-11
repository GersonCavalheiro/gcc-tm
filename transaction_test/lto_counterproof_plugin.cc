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
#include "newtm-lto.h"
#include "options.h"
#include "ssa.h"
#include "tree-ssanames.h"
#include "tree-dfa.h"
#include "gimple-ssa.h"
#include "tree-into-ssa.h"
#include "cgraph.h"
int plugin_is_GPL_compatible;
static const char *mode = "observe";
static bool changed, observed;
static const char *phase = "ltrans";
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
                tree value = ssa_default_def (fn, q);
                if (!value) return 0;
                TREE_OPERAND (ref, 0) = value;
                gimple_assign_set_rhs_from_tree (&it, ref); update_stmt (gsi_stmt (it)); changed = true; break;
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
            tree value = make_ssa_name (TREE_TYPE (buffer));
            gimple *load = gimple_build_assign (value, buffer);
            SSA_NAME_DEF_STMT (value) = load;
            gsi_insert_before (&it, load, GSI_SAME_STMT); update_stmt (load);
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
              unlink_stmt_vdef (gsi_stmt (it));
              gsi_replace (&it, replacement, true);
              if (TREE_CODE (status) == SSA_NAME) SSA_NAME_DEF_STMT (status) = replacement;
              update_stmt (replacement); changed = true; break;
            }
    if (changed) fprintf (stderr, "INTEGRATED_MUTATION %s applied\n", mode);
    return 0;
  }
};
}
static void ipa (void *, void *)
{
  if (!in_lto_p || seen_error ()) return;
  const char *here = flag_wpa ? "wpa" : flag_ltrans ? "ltrans"
    : flag_incremental_link == INCREMENTAL_LINK_LTO ? "incremental" : "monolithic";
  unsigned HOST_WIDE_INT count = newtm_current_count ();
  if (!count)
    {
      /* max/1to1 can place ordinary code in separate LTRANS units.
         Those units must not acquire fictional proof roots. */
      if (flag_ltrans)
        { fputs ("LTO_COUNTERPROOF ltrans no-roots\n", stderr); return; }
      error ("LTO counterproof has no reconstructed root"); return;
    }
  for (unsigned HOST_WIDE_INT i = 1; i <= count; ++i)
    {
      newtm_current_view v; newtm_current_get (i, &v);
      if (!v.current_approved || v.proved != NEWTM_CURRENT_ALL || v.dependencies)
        { error ("LTO counterproof lacks approved baseline"); return; }
    }
  observed = true;
  fprintf (stderr, "LTO_COUNTERPROOF %s baseline approved=%llu\n", here, (unsigned long long) count);
  if (!strcmp (mode, "observe") || strcmp (phase, here)) return;
  newtm_current_view old; newtm_current_get (1, &old);
  if (!strcmp (mode, "retired"))
    {
      /* Closing LTO restores attributes/options, so a full declaration
         snapshot changes legitimately. Check actual statements and operands. */
      auto_vec<gimple *> statements;
      auto_vec<tree> operands;
      function *body = DECL_STRUCT_FUNCTION (old.root); basic_block bb;
      FOR_EACH_BB_FN (bb, body)
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          { gimple *s = gsi_stmt (it); statements.safe_push (s);
            for (unsigned j = 0; j < gimple_num_ops (s); ++j) operands.safe_push (gimple_op (s, j)); }
      newtm_current_end_frontend ();
      unsigned si = 0, oi = 0;
      FOR_EACH_BB_FN (bb, body)
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          { gimple *s = gsi_stmt (it);
            if (si >= statements.length () || statements[si++] != s)
              { error ("LTO retirement changed statements"); return; }
            for (unsigned j = 0; j < gimple_num_ops (s); ++j)
              if (oi >= operands.length () || operands[oi++] != gimple_op (s, j))
                { error ("LTO retirement changed operands"); return; } }
      if (si != statements.length () || oi != operands.length ())
        { error ("LTO retirement changed statement count"); return; }
      changed = true;
    }
  else
    {
      mutate pass (g);
      cgraph_node *node;
      FOR_EACH_DEFINED_FUNCTION (node)
        if (!changed && DECL_STRUCT_FUNCTION (node->decl))
          {
            function *fn = DECL_STRUCT_FUNCTION (node->decl);
            push_cfun (fn); pass.execute (fn);
            if (changed) update_ssa (TODO_update_ssa_only_virtuals);
            pop_cfun ();
          }
    }
  if (!changed) { error ("LTO counterproof target was not mutated"); return; }
  fprintf (stderr, "LTO_COUNTERPROOF %s mutation=%s applied\n", here, mode);
  if (newtm_current_verify (old.root)) error ("LTO mutated graph was approved");
  newtm_current_view last; newtm_current_get (newtm_current_count (), &last);
  if (last.current_approved || last.proved || last.state != NEWTM_CURRENT_REFUSED)
    error ("LTO counterproof did not atomically refuse");
  else fprintf (stderr, "LTO_COUNTERPROOF %s mutation=%s refused proved=0\n", here, mode);
}
static void finish (void *, void *)
{
  if (!in_lto_p) return;
  if (observed && !seen_error ())
    {
      for (unsigned HOST_WIDE_INT i = 1; i <= newtm_current_count (); ++i)
        { newtm_current_view v; newtm_current_get (i, &v);
          if (v.current_approved) error ("LTO approval survived phase boundary"); }
      fputs ("LTO_COUNTERPROOF frontier revoked\n", stderr);
    }
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i)
    { if (!strcmp (args->argv[i].key, "mode")) mode = args->argv[i].value;
      if (!strcmp (args->argv[i].key, "phase")) phase = args->argv[i].value; }
  register_callback (args->base_name, PLUGIN_ALL_IPA_PASSES_START, ipa, NULL);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
