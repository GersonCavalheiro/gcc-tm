/* Fresh LTO proofs, adversarial serialized IR, revocation and restoration. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "ssa.h"
#include "tree-ssanames.h"
#include "tree-dfa.h"
#include "tree-ssa.h"
#include "gimple-ssa.h"
#include "tree-pass.h"
#include "context.h"
#include "cgraph.h"
#include "function.h"
#include "options.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "attribs.h"
#include "ggc.h"
#include "newtm-current.h"
#include "newtm-clone.h"
#include "newtm-lto.h"
int plugin_is_GPL_compatible;
static const char *mode = "observe";
static unsigned level;
static bool changed, observed;
static auto_vec<tree> members;
static bool named (tree d, const char *name)
{ return d && DECL_NAME (d) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (d)), name); }
namespace {
const pass_data data = {GIMPLE_PASS, "lto_graph_mutate", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0};
struct mutate : gimple_opt_pass {
  mutate (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (in_lto_p || changed || !strcmp (mode, "observe") || !strcmp (mode, "rebuild")
        || !strcmp (mode, "revoke")) return 0;
    tree original = newtm_clone_original (fn->decl);
    if (!strcmp (mode, "owner") || !strcmp (mode, "entry"))
      { if (!named (fn->decl, "main")) return 0; }
    else if (!strcmp (mode, "ssa_use"))
      { if (!named (original, "sequential")) return 0; }
    else if (!named (original, "recurse")) return 0;
    tree payload = newtm_lto_payload (fn->decl);
    if (!payload) { error ("LTO mutation has no transported contracts"); return 0; }
    if (!strcmp (mode, "version"))
      { TREE_VEC_ELT (payload, 0) = build_int_cst (unsigned_type_node, 999); changed = true; }
    else if (!strcmp (mode, "mode"))
      { tree attr = lookup_attribute ("newtm lto contracts", DECL_ATTRIBUTES (fn->decl));
        tree settings = TREE_VALUE (TREE_VALUE (attr));
        TREE_PURPOSE (TREE_CHAIN (settings)) = integer_zero_node; changed = true; }
    else if (!strcmp (mode, "envelope"))
      { TREE_VEC_ELT (payload, 3) = integer_one_node; changed = true; }
    else if (!strcmp (mode, "map"))
      { TREE_VEC_ELT (TREE_VEC_ELT (payload, 3), 0) = integer_one_node; changed = true; }
    else if (!strcmp (mode, "roots"))
      { TREE_VEC_ELT (payload, 1) = NULL_TREE; changed = true; }
    else if (!strcmp (mode, "component"))
      { TREE_VEC_ELT (TREE_VEC_ELT (payload, 3), 0) = NULL_TREE; changed = true; }
    else if (!strcmp (mode, "missing"))
      { DECL_ATTRIBUTES (fn->decl) = remove_attribute ("newtm lto contracts", DECL_ATTRIBUTES (fn->decl)); changed = true; }
    else if (!strcmp (mode, "layout"))
      { tree field = TYPE_FIELDS (TREE_TYPE (TREE_TYPE (newtm_clone_channel (fn->decl, 2))));
        field = DECL_CHAIN (field); DECL_FIELD_OFFSET (field) = size_zero_node; changed = true; }
    basic_block bb;
    if (!changed && !strcmp (mode, "phi"))
      FOR_EACH_BB_FN (bb, fn)
        for (gphi_iterator it = gsi_start_phis (bb); !gsi_end_p (it) && !changed; gsi_next (&it))
          if (!virtual_operand_p (gimple_phi_result (it.phi ())))
            { tree result = gimple_phi_result (it.phi ());
              SET_PHI_ARG_DEF (it.phi (), 0, build_zero_cst (TREE_TYPE (result))); changed = true; }
    FOR_EACH_BB_FN (bb, fn)
      for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it) && !changed; gsi_next (&it))
        {
          gimple *s = gsi_stmt (it);
          if (!strcmp (mode, "raw") && is_gimple_call (s))
            {
              varpool_node *v; tree global = NULL_TREE;
              FOR_EACH_VARIABLE (v) if (named (v->decl, "unrelated")) global = v->decl;
              if (!global) { error ("missing global for LTO mutation"); return 0; }
              tree lhs = make_ssa_name (TREE_TYPE (global));
              gimple *read = gimple_build_assign (lhs, global); SSA_NAME_DEF_STMT (lhs) = read;
              gsi_insert_before (&it, read, GSI_SAME_STMT); update_stmt (read); changed = true;
            }
          else if (!strcmp (mode, "guard") && gimple_code (s) == GIMPLE_COND)
            { gimple_cond_set_code (as_a<gcond *> (s), gimple_cond_code (s) == EQ_EXPR ? NE_EXPR : EQ_EXPR); changed = true; }
          else if (!strcmp (mode, "ssa_use") && is_gimple_assign (s) && gimple_assign_single_p (s))
            {
              tree rhs = gimple_assign_rhs1 (s);
              if (TREE_CODE (rhs) != SSA_NAME || SSA_NAME_IS_DEFAULT_DEF (rhs)
                  || !named (SSA_NAME_VAR (rhs), "n")) continue;
              tree old = ssa_default_def (fn, SSA_NAME_VAR (rhs));
              if (!old) continue;
              gimple_assign_set_rhs1 (s, old); update_stmt (s); changed = true;
            }
          else if (is_gimple_call (s))
            {
              tree callee = gimple_call_fndecl (s);
              bool builtin = callee && fndecl_built_in_p (callee, BUILT_IN_NORMAL);
              if (!strcmp (mode, "entry") && callee && newtm_clone_original (callee)
                  && newtm_clone_region_owner (callee))
                {
                  tree lhs = gimple_call_lhs (s);
                  gimple *replacement = gimple_build_assign (lhs, build_zero_cst (TREE_TYPE (lhs)));
                  unlink_stmt_vdef (s);
                  gsi_replace (&it, replacement, true);
                  if (TREE_CODE (lhs) == SSA_NAME) SSA_NAME_DEF_STMT (lhs) = replacement;
                  update_stmt (replacement); changed = true; continue;
                }
              unsigned arg = 0; bool match = false;
              if (!strcmp (mode, "context")) match = builtin && DECL_FUNCTION_CODE (callee) == BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS;
              if (!strcmp (mode, "status")) { match = builtin && DECL_FUNCTION_CODE (callee) == BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL; arg = 1; }
              if (!strcmp (mode, "owner")) match = builtin && DECL_FUNCTION_CODE (callee) == BUILT_IN_GOMP_TRANSACTION_STM_V1_END;
              if (!strcmp (mode, "channel"))
                { match = callee && newtm_clone_original (callee); arg = gimple_call_num_args (s) - 1; }
              if (match)
                { tree old = gimple_call_arg (s, arg); gimple_call_set_arg (s, arg, build_zero_cst (TREE_TYPE (old))); update_stmt (s); changed = true; }
            }
        }
    if (changed) fprintf (stderr, "LTO_MUTATION %s serialized\n", mode);
    return changed ? TODO_update_ssa_only_virtuals : 0;
  }
};
}
static void ipa (void *, void *)
{
  if (!in_lto_p || seen_error ()) return;
  ggc_collect (GGC_COLLECT_FORCE);
  unsigned approved = 0;
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_current_count (); ++i)
    {
      newtm_current_view v;
      if (!newtm_current_get (i, &v) || !v.current_approved || v.dependencies || v.proved != 31)
        { error ("LTO current graph was not reconstructed atomically"); return; }
      ++approved;
      for (unsigned j = 0; j < v.members; ++j)
        { tree fn; unsigned mask; newtm_current_member (i, j, &fn, &mask); members.safe_push (fn); }
    }
  observed = true;
  fprintf (stderr, "LTO_GRAPH %s approved=%u\n", flag_wpa ? "WPA" : flag_ltrans ? "LTRANS" : "MONOLITHIC", approved);
  if (approved && !strcmp (mode, "rebuild"))
    {
      newtm_current_view old; newtm_current_get (1, &old);
      unsigned HOST_WIDE_INT count = newtm_current_count ();
      newtm_current_invalidate (old.root);
      if (!newtm_current_verify (old.root) || newtm_current_count () != count + 1)
        error ("LTO complete reconstruction did not create a new generation");
      newtm_current_view v; newtm_current_get (1, &v);
      if (v.current_approved || v.state != NEWTM_CURRENT_INVALID) error ("LTO rebuild revived an old generation");
      newtm_current_get (count + 1, &v);
      if (!v.current_approved) error ("LTO rebuilt generation is not approved");
      fputs ("LTO_REBUILD new generation\n", stderr);
    }
  if (approved && !strcmp (mode, "revoke"))
    {
      newtm_current_view first; newtm_current_get (1, &first);
      function *body = DECL_STRUCT_FUNCTION (first.root); gimple *call = NULL; basic_block bb;
      FOR_EACH_BB_FN (bb, body)
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          if (!call && is_gimple_call (gsi_stmt (it))) call = gsi_stmt (it);
      if (!call) { error ("LTO revoke probe found no call"); return; }
      tree arg = gimple_call_arg (call, 0); gimple_call_set_arg (call, 0, build_zero_cst (TREE_TYPE (arg)));
      newtm_current_view v; newtm_current_get (1, &v);
      if (v.current_approved) error ("LTO current approval survived a changed operand");
      gimple_call_set_arg (call, 0, arg); newtm_current_get (1, &v);
      if (v.current_approved || v.state != NEWTM_CURRENT_INVALID) error ("restoring LTO IR revived approval");
      fputs ("LTO_REVOKE irreversible generation\n", stderr);
    }
}
static void finish (void *, void *)
{
  if (!in_lto_p)
    {
      if (strcmp (mode, "observe") && strcmp (mode, "rebuild") && strcmp (mode, "revoke") && !changed && !seen_error ())
        error ("LTO probe did not mutate its source SSA");
      return;
    }
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_current_count (); ++i)
    { newtm_current_view v; newtm_current_get (i, &v);
      if (v.current_approved) error ("LTO graph survived its transformation frontier"); }
  if (observed) fputs ("LTO_FRONTIER revoked\n", stderr);
  if (!flag_wpa && flag_incremental_link != INCREMENTAL_LINK_LTO)
    for (tree fn : members)
      if (newtm_lto_protected_p (fn) || (unsigned) opt_for_fn (fn, optimize) != level)
        { error ("LTO function optimization options were not restored"); break; }
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i)
    { if (!strcmp (args->argv[i].key, "mode")) mode = args->argv[i].value;
      if (!strcmp (args->argv[i].key, "level")) level = atoi (args->argv[i].value); }
  register_pass_info p = {new mutate (g), "ssa", 1, PASS_POS_INSERT_AFTER};
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (args->base_name, PLUGIN_ALL_IPA_PASSES_START, ipa, NULL);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
