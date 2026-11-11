/* Current frontend publication, mutation, revocation, rebuild and frontier. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimplify.h"
#include "fold-const.h"
#include "function.h"
#include "cgraph.h"
#include "ggc.h"
#include "diagnostic-core.h"
#include "options.h"
#include "newtm-clone.h"
#include "newtm-current.h"
#include "newtm-memory.h"
#include "newtm-obligation.h"
#include "tree-pass.h"
#include "context.h"
int plugin_is_GPL_compatible;
static const char *mode = "observe";
static bool observed;
static bool partial_checked;
static bool named (tree d, const char *name)
{ return d && DECL_NAME (d) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (d)), name); }
namespace {
const pass_data data = { GIMPLE_PASS, "current_graph_partial", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct partial : gimple_opt_pass {
  partial (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (newtm_current_count ()) error ("current graph was published before whole-unit composition");
    if (!strcmp (mode, "partial") && !partial_checked && named (newtm_clone_original (fn->decl), "recurse"))
      {
        if (!newtm_memory_verify_cfg (fn->decl) || !newtm_channels_verify_cfg (fn->decl)
            || !newtm_status_verify_cfg (fn->decl) || !newtm_coverage_verify_cfg (fn->decl))
          error ("standalone current components refused their original body");
        if (newtm_current_count ()) error ("standalone components granted graph approval");
        partial_checked = true; fputs ("CURRENT_GRAPH_PARTIAL no approval\n", stderr);
      }
    return 0;
  }
};
}
static void ipa (void *, void *)
{
  if (in_lto_p)
    {
      for (unsigned HOST_WIDE_INT i = 1; i <= newtm_current_count (); ++i)
        { newtm_current_view v; newtm_current_get (i, &v);
          if (!v.current_approved || v.dependencies) error ("LTO current graph was not reconstructed"); }
      fputs ("CURRENT_GRAPH_LTO reconstructed\n", stderr); return;
    }
  if (seen_error ()) return;
  unsigned HOST_WIDE_INT count = newtm_current_count ();
  if (!count) { error ("missing current graph publication"); return; }
  ggc_collect (GGC_COLLECT_FORCE);
  tree target = NULL_TREE; unsigned approved = 0;
  for (unsigned HOST_WIDE_INT i = 1; i <= count; ++i)
    {
      newtm_current_view v;
      if (!newtm_current_get (i, &v) || !v.current_approved || v.state != NEWTM_CURRENT_APPROVED
          || v.required != NEWTM_CURRENT_ALL || v.proved != v.required || v.dependencies || !v.controller)
        { error ("current graph was not atomically approved"); return; }
      ++approved;
      for (unsigned j = 0; j < v.members; ++j)
        {
          tree fn; unsigned mask;
          if (!newtm_current_member (i, j, &fn, &mask)) { error ("missing current graph member"); return; }
          if (tree original = newtm_clone_original (fn))
            if (named (original, "recurse")) target = fn;
        }
    }
  observed = true;
  if (!strcmp (mode, "observe") || !strcmp (mode, "partial"))
    { fprintf (stderr, "CURRENT_GRAPH approved=%u dependencies=0\n", approved); return; }
  if (!strcmp (mode, "controller") || !strcmp (mode, "producer"))
    {
      newtm_current_view first; newtm_current_get (1, &first);
      tree changed = !strcmp (mode, "controller") ? first.controller : NULL_TREE;
      if (!changed)
        for (unsigned j = 0; j < first.members; ++j)
          { tree fn; unsigned mask; newtm_current_member (1, j, &fn, &mask);
            if (!mask) changed = fn; }
      if (!changed) { error ("missing outlining producer in current certificate"); return; }
      auto_vec<unsigned HOST_WIDE_INT> affected;
      for (unsigned HOST_WIDE_INT i = 1; i <= count; ++i)
        {
          newtm_current_view v; newtm_current_get (i, &v);
          for (unsigned j = 0; j < v.members; ++j)
            { tree fn; unsigned mask; newtm_current_member (i, j, &fn, &mask);
              if (fn == changed) { affected.safe_push (i); break; } }
        }
      push_cfun (DECL_STRUCT_FUNCTION (changed));
      gimple *inserted = gimple_build_nop ();
      basic_block bb = single_succ (ENTRY_BLOCK_PTR_FOR_FN (cfun));
      gimple_stmt_iterator it = gsi_start_bb (bb);
      gsi_insert_before (&it, inserted, GSI_SAME_STMT);
      for (unsigned HOST_WIDE_INT i : affected)
        { newtm_current_view v; newtm_current_get (i, &v);
          if (v.current_approved || v.proved) error ("controller/producer mutation retained current approval"); }
      it = gsi_for_stmt (inserted); gsi_remove (&it, false);
      for (unsigned HOST_WIDE_INT i : affected)
        { newtm_current_view v; newtm_current_get (i, &v);
          if (v.state != NEWTM_CURRENT_INVALID) error ("restored controller/producer revived approval"); }
      pop_cfun ();
      fprintf (stderr, "CURRENT_GRAPH_%s checked\n", mode); return;
    }
  if (!target) { error ("current graph mutation has no recursive member"); return; }
  auto_vec<unsigned HOST_WIDE_INT> affected, independent;
  for (unsigned HOST_WIDE_INT i = 1; i <= count; ++i)
    {
      newtm_current_view v; newtm_current_get (i, &v); bool contains = false;
      for (unsigned j = 0; j < v.members; ++j)
        { tree fn; unsigned mask; newtm_current_member (i, j, &fn, &mask); contains |= fn == target; }
      if (contains) affected.safe_push (i); else independent.safe_push (i);
    }
  if (affected.length () < 2 || independent.is_empty ())
    { error ("current graph fixture lost shared/independent roots"); return; }
  newtm_current_view first; newtm_current_get (affected[0], &first);
  bool switch_fn = cfun != DECL_STRUCT_FUNCTION (target);
  if (switch_fn) push_cfun (DECL_STRUCT_FUNCTION (target));
  gimple *statement = NULL, *inserted = NULL; basic_block bb;
  FOR_EACH_BB_FN (bb, cfun)
    for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
      if (!statement && is_gimple_call (gsi_stmt (it))) statement = gsi_stmt (it);
  if (!statement) { error ("current graph has no call to mutate"); if (switch_fn) pop_cfun (); return; }
  tree old_arg = NULL_TREE, old_type = NULL_TREE, type_node = NULL_TREE;
  unsigned old_align = 0;
  edge changed_edge = NULL; int edge_flags = 0;
  if (!strcmp (mode, "operand"))
    { old_arg = gimple_call_arg (statement, 0);
      gimple_call_set_arg (statement, 0, build_zero_cst (TREE_TYPE (old_arg))); }
  else if (!strcmp (mode, "signature"))
    { old_type = TREE_TYPE (target); TREE_TYPE (target) = build_function_type_list (integer_type_node, NULL_TREE); }
  else if (!strcmp (mode, "layout"))
    { type_node = TREE_TYPE (newtm_clone_channel (target, 2));
      old_align = TYPE_ALIGN (type_node); SET_TYPE_ALIGN (type_node, old_align * 2); }
  else if (!strcmp (mode, "edge"))
    {
      FOR_EACH_BB_FN (bb, cfun)
        if (!changed_edge && EDGE_COUNT (bb->succs) > 1) changed_edge = EDGE_SUCC (bb, 0);
      if (!changed_edge) { error ("no native edge for current graph mutation"); if (switch_fn) pop_cfun (); return; }
      edge_flags = changed_edge->flags; changed_edge->flags ^= EDGE_TRUE_VALUE;
    }
  else if (!strcmp (mode, "failed"))
    {
      tree global = NULL_TREE; varpool_node *v;
      FOR_EACH_VARIABLE (v) if (named (v->decl, "unrelated")) global = v->decl;
      if (!global) { error ("no global for current graph failure"); if (switch_fn) pop_cfun (); return; }
      inserted = gimple_build_assign (global, build_one_cst (TREE_TYPE (global)));
      gimple_stmt_iterator it = gsi_for_stmt (statement); gsi_insert_before (&it, inserted, GSI_SAME_STMT);
    }
  else if (!strcmp (mode, "recheck"))
    { if (!newtm_channels_verify_cfg (target)) error ("unchanged standalone component refused"); }
  else if (!strcmp (mode, "notify") || !strcmp (mode, "rebuild")) newtm_current_invalidate (target);
  else if (!strcmp (mode, "metadata")) newtm_clone_record_decl (target, target);
  else { error ("unknown current graph probe mode"); if (switch_fn) pop_cfun (); return; }
  for (unsigned HOST_WIDE_INT i : affected)
    { newtm_current_view v; if (!newtm_current_get (i, &v) || v.current_approved || v.proved) error ("stale current graph approval survived mutation"); }
  if (old_arg) gimple_call_set_arg (statement, 0, old_arg);
  if (old_type) TREE_TYPE (target) = old_type;
  if (type_node) SET_TYPE_ALIGN (type_node, old_align);
  if (changed_edge) changed_edge->flags = edge_flags;
  bool failed = !strcmp (mode, "failed");
  if (failed)
    {
      if (newtm_current_verify (first.root)) error ("invalid current graph was reapproved");
      newtm_current_view v; newtm_current_get (newtm_current_count (), &v);
      if (v.current_approved || v.members || v.proved || v.state != NEWTM_CURRENT_REFUSED)
        error ("failed current graph exposed a partial approval");
      gimple_stmt_iterator it = gsi_for_stmt (inserted); gsi_remove (&it, false);
    }
  for (unsigned HOST_WIDE_INT i : affected)
    { newtm_current_view v; newtm_current_get (i, &v);
      if (v.current_approved || v.state != NEWTM_CURRENT_INVALID) error ("restoring IR revived an old current certificate"); }
  if (strcmp (mode, "layout") && strcmp (mode, "metadata"))
    for (unsigned HOST_WIDE_INT i : independent)
      { newtm_current_view v; if (!newtm_current_get (i, &v) || !v.current_approved) error ("unrelated current graph was revoked"); }
  if (!strcmp (mode, "rebuild"))
    {
      unsigned HOST_WIDE_INT before = newtm_current_count ();
      if (!newtm_current_verify (first.root) || newtm_current_count () != before + 1) error ("current graph could not be rebuilt");
      newtm_current_view v; newtm_current_get (newtm_current_count (), &v);
      if (!v.current_approved || v.dependencies) error ("rebuilt graph did not discharge all components");
      for (unsigned HOST_WIDE_INT i : affected)
        { newtm_current_get (i, &v); if (v.current_approved) error ("rebuild resurrected a previous generation"); }
    }
  if (switch_fn) pop_cfun ();
  fprintf (stderr, "CURRENT_GRAPH_%s checked shared=%u independent=%u\n", mode, affected.length (), independent.length ());
}
static void finish (void *, void *)
{
  if (in_lto_p)
    {
      for (unsigned HOST_WIDE_INT i = 1; i <= newtm_current_count (); ++i)
        { newtm_current_view v; newtm_current_get (i, &v);
          if (v.current_approved) error ("LTO approval survived its frontier"); }
      return;
    }
  if (!observed && !seen_error ()) error ("current graph probe missed its frontend window");
  if (!strcmp (mode, "partial") && !partial_checked) error ("partial component probe did not run");
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_current_count (); ++i)
    { newtm_current_view v;
      if (!newtm_current_get (i, &v) || v.current_approved) error ("frontend certificate survived its IPA frontier"); }
  if (observed) fputs ("CURRENT_GRAPH_FRONTIER revoked\n", stderr);
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i) if (!strcmp (args->argv[i].key, "mode")) mode = args->argv[i].value;
  register_pass_info p = { new partial (g), "ompexp", 1, PASS_POS_INSERT_AFTER };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (args->base_name, PLUGIN_ALL_IPA_PASSES_START, ipa, NULL);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL); return 0;
}
