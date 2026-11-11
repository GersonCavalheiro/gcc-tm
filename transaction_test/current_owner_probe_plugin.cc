/* Mutations of actual post-ompexp ownership paths, before SSA. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "gimplify.h"
#include "fold-const.h"
#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "cgraph.h"
#include "cfghooks.h"
#include "tree-cfg.h"
#include "diagnostic-core.h"
#include "newtm-clone.h"
#include "newtm-memory.h"
#include "newtm-obligation.h"
int plugin_is_GPL_compatible;
static const char *mode = "observe";
static bool changed, scanning;
static bool builtin (gimple *s, built_in_function code)
{ return s && is_gimple_call (s) && gimple_call_fndecl (s)
    && fndecl_built_in_p (gimple_call_fndecl (s), code); }
static void before (gimple *target, gimple *s)
{ gimple_stmt_iterator at = gsi_for_stmt (target); gsi_insert_before (&at, s, GSI_SAME_STMT); }
static void after (gimple *target, gimple *s)
{ gimple_stmt_iterator at = gsi_for_stmt (target); gsi_insert_after (&at, s, GSI_SAME_STMT); }
namespace {
const pass_data data = { GIMPLE_PASS, "current_owner_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass {
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (changed || !strcmp (mode, "observe")) return 0;
    if (!scanning)
      {
        scanning = true; cgraph_node *node;
        FOR_EACH_DEFINED_FUNCTION (node)
          if (function *body = DECL_STRUCT_FUNCTION (node->decl))
            if (body->cfg && !newtm_clone_original (node->decl))
              {
                bool switch_fn = cfun != body; if (switch_fn) push_cfun (body);
                execute (body); if (switch_fn) pop_cfun ();
                if (changed) break;
              }
        scanning = false; return 0;
      }
    if (!DECL_NAME (fn->decl) || strncmp (IDENTIFIER_POINTER (DECL_NAME (fn->decl)), "main", 4)) return 0;
    gimple *body = NULL, *begin = NULL, *status = NULL, *end = NULL, *fail = NULL, *cleanup = NULL, *fatal = NULL;
    basic_block bb;
    FOR_EACH_BB_FN (bb, fn)
      for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
        {
          gimple *s = gsi_stmt (at);
          if (!begin && builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN)) begin = s;
          if (!body && is_gimple_call (s) && newtm_clone_region_owner (gimple_call_fndecl (s))) body = s;
        }
    if (!body || !begin) return 0;
    tree slot = TREE_OPERAND (gimple_call_arg (begin, 0), 0);
    FOR_EACH_BB_FN (bb, fn)
      for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
        {
          gimple *s = gsi_stmt (at);
          if (!status && builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS)) status = s;
          if (builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_END)
              && operand_equal_p (gimple_call_arg (s, 0), gimple_call_arg (begin, 0), 0)) end = s;
          if (!fail && builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL)) fail = s;
          if (builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE)
              && operand_equal_p (gimple_call_arg (s, 0), gimple_call_arg (begin, 0), 0)) cleanup = s;
          if (!fatal && builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL)) fatal = s;
        }
    if (!end || !fail || !cleanup || !status || !fatal) return 0;
    tree counter = gimple_call_arg (cleanup, 1), outer = TREE_OPERAND (gimple_call_arg (begin, 1), 0);
    gimple *boundary = NULL, *increment = NULL, *bound = NULL, *retry = NULL, *outer_load = NULL;
    FOR_EACH_BB_FN (bb, fn)
      for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
        {
          gimple *s = gsi_stmt (at);
          if (is_gimple_assign (s))
            {
              if (gimple_assign_lhs (s) == slot && gimple_clobber_p (s, CLOBBER_STORAGE_END)) boundary = s;
              if (gimple_assign_lhs (s) == counter && gimple_assign_rhs_code (s) == PLUS_EXPR) increment = s;
              if (gimple_assign_single_p (s) && gimple_assign_rhs1 (s) == outer) outer_load = s;
            }
          if (gimple_code (s) == GIMPLE_COND)
            {
              if (gimple_cond_lhs (s) == counter) bound = s;
              if (gimple_cond_lhs (s) == gimple_call_lhs (cleanup)
                  && tree_fits_uhwi_p (gimple_cond_rhs (s)) && tree_to_uhwi (gimple_cond_rhs (s)) == 1) retry = s;
            }
        }
    if (!boundary || !increment || !bound || !retry || !outer_load) return 0;
    if (!newtm_owner_verify_entry (fn->decl, body))
      { error ("native owner baseline was refused before mutation"); return 0; }
    gimple *undo_address = NULL, *undo_store = NULL;
    edge redirected = NULL; basic_block old_dest = NULL;
    gimple *target = !strcmp (mode, "drop_begin") ? begin : !strcmp (mode, "drop_body") ? body
      : !strcmp (mode, "drop_status") ? status : !strcmp (mode, "drop_end") ? end
      : !strcmp (mode, "drop_fail") ? fail : !strcmp (mode, "drop_cleanup") ? cleanup : NULL;
    if (target)
      { gimple_stmt_iterator at = gsi_for_stmt (target);
        gsi_replace (&at, gimple_build_assign (gimple_call_lhs (target), build_zero_cst (TREE_TYPE (gimple_call_lhs (target)))), false); }
    else if (!strcmp (mode, "outer"))
      { gimple_stmt_iterator at = gsi_for_stmt (outer_load);
        gsi_replace (&at, gimple_build_assign (gimple_assign_lhs (outer_load), build_one_cst (TREE_TYPE (outer))), false); }
    else if (!strcmp (mode, "foreign_context")) gimple_call_set_arg (body, 0, build_zero_cst (TREE_TYPE (slot)));
    else if (!strcmp (mode, "foreign_slot"))
      { tree foreign = create_tmp_var (TREE_TYPE (slot)); TREE_ADDRESSABLE (foreign) = 1;
        before (end, gimple_build_assign (foreign, slot)); gimple_call_set_arg (end, 0, build_fold_addr_expr (foreign)); }
    else if (!strcmp (mode, "slot_escape")) before (body, gimple_build_assign (create_tmp_var (build_pointer_type (TREE_TYPE (slot))), build_fold_addr_expr (slot)));
    else if (!strcmp (mode, "slot_overwrite")) before (body, gimple_build_assign (slot, build_zero_cst (TREE_TYPE (slot))));
    else if (!strcmp (mode, "cause_conflict") || !strcmp (mode, "cause_invalid"))
      gimple_call_set_arg (fail, 1, build_int_cst (TREE_TYPE (gimple_call_arg (fail, 1)), !strcmp (mode, "cause_conflict") ? 1 : 2));
    /* The local FAIL result is dead here: AFTER_FAILURE reads the condemned
       context and overwrites this carrier.  Do not reject harmless code. */
    else if (!strcmp (mode, "dead_status")) after (fail, gimple_build_assign (gimple_call_lhs (fail), build_one_cst (TREE_TYPE (gimple_call_lhs (fail)))));
    else if (!strcmp (mode, "counter_reset")) before (cleanup, gimple_build_assign (counter, build_zero_cst (TREE_TYPE (counter))));
    else if (!strcmp (mode, "counter_double")) after (increment, gimple_copy (increment));
    else if (!strcmp (mode, "counter_wrap")) gimple_assign_set_rhs2 (increment, TYPE_MAX_VALUE (TREE_TYPE (counter)));
    else if (!strcmp (mode, "counter_bound")) gimple_cond_set_rhs (as_a<gcond *> (bound), build_zero_cst (TREE_TYPE (counter)));
    else if (!strcmp (mode, "retry_invalid") || !strcmp (mode, "retry_unknown"))
      gimple_cond_set_rhs (as_a<gcond *> (retry), build_int_cst (TREE_TYPE (gimple_cond_rhs (retry)), !strcmp (mode, "retry_invalid") ? 2 : 77));
    else if (!strcmp (mode, "retry_spoof")) before (retry, gimple_build_assign (gimple_call_lhs (cleanup), build_one_cst (TREE_TYPE (gimple_call_lhs (cleanup)))));
    else if (!strcmp (mode, "fatal_conflict"))
      gimple_cond_set_lhs (as_a<gcond *> (retry), build_zero_cst (TREE_TYPE (gimple_cond_lhs (retry))));
    else if (!strcmp (mode, "begin_success") || !strcmp (mode, "body_success")
             || !strcmp (mode, "status_success") || !strcmp (mode, "end_success"))
      {
        gimple *s = !strcmp (mode, "begin_success") ? begin : !strcmp (mode, "body_success") ? body
          : !strcmp (mode, "status_success") ? status : end;
        after (s, gimple_build_assign (gimple_call_lhs (s), build_zero_cst (TREE_TYPE (gimple_call_lhs (s)))));
      }
    else if (!strcmp (mode, "unknown_return"))
      {
        basic_block prefix = gimple_bb (begin); edge normal = split_block (prefix, begin);
        basic_block returned = create_empty_bb (prefix); tree result = gimple_call_lhs (begin);
        tree raw = build_int_cst (TREE_TYPE (result), 77);
        gimple_stmt_iterator at = gsi_last_bb (prefix);
        gsi_insert_after (&at, gimple_build_cond (EQ_EXPR, result, raw, NULL_TREE, NULL_TREE), GSI_NEW_STMT);
        at = gsi_start_bb (returned); gsi_insert_after (&at, gimple_build_return (integer_zero_node), GSI_NEW_STMT);
        normal->flags = EDGE_FALSE_VALUE; normal->probability = profile_probability::even ();
        edge failed = make_edge (prefix, returned, EDGE_TRUE_VALUE); failed->probability = profile_probability::even ();
        make_edge (returned, EXIT_BLOCK_PTR_FOR_FN (fn), 0);
      }
    else if (!strcmp (mode, "stale_fields") || !strcmp (mode, "entry_bypass"))
      {
        if (!strcmp (mode, "stale_fields"))
          {
            edge e; edge_iterator ei;
            FOR_EACH_EDGE (e, ei, gimple_bb (outer_load)->succs)
              if (e->flags & EDGE_TRUE_VALUE) redirected = e;
          }
        else
          {
            gimple *initial = NULL;
            FOR_EACH_BB_FN (bb, fn)
              for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
                if (is_gimple_assign (gsi_stmt (at)) && gimple_assign_lhs (gsi_stmt (at)) == slot
                    && gimple_assign_single_p (gsi_stmt (at)) && integer_zerop (gimple_assign_rhs1 (gsi_stmt (at)))) initial = gsi_stmt (at);
            if (initial && EDGE_COUNT (gimple_bb (initial)->preds) == 1) redirected = EDGE_PRED (gimple_bb (initial), 0);
          }
        if (!redirected) return 0;
        old_dest = redirected->dest; redirected = redirect_edge_and_branch (redirected, gimple_bb (body));
      }
    else if (!strcmp (mode, "retry_skip") || !strcmp (mode, "failure_cycle"))
      {
        edge e; edge_iterator ei;
        FOR_EACH_EDGE (e, ei, gimple_bb (retry)->succs)
          if (e->flags & EDGE_TRUE_VALUE) redirected = e;
        if (!redirected) return 0;
        old_dest = redirected->dest;
        redirected = redirect_edge_and_branch (redirected, gimple_bb (!strcmp (mode, "retry_skip") ? body : fail));
      }
    else if (!strcmp (mode, "live_clobber")) before (body, gimple_build_assign (outer, build_clobber (TREE_TYPE (outer), CLOBBER_STORAGE_END)));
    else if (!strcmp (mode, "missing_boundary"))
      { gimple_stmt_iterator at = gsi_for_stmt (boundary); gsi_remove (&at, false); }
    else if (!strcmp (mode, "early_boundary"))
      { gimple_stmt_iterator at = gsi_for_stmt (boundary); gsi_remove (&at, false); before (end, boundary); }
    else if (!strcmp (mode, "outside")) after (boundary, gimple_build_assign (create_tmp_var (TREE_TYPE (slot)), slot));
    else if (!strcmp (mode, "new_alias_outside"))
      {
        tree alias = create_tmp_var (TREE_TYPE (slot));
        before (body, gimple_build_assign (alias, gimple_call_arg (body, 0)));
        after (boundary, gimple_build_assign (create_tmp_var (TREE_TYPE (slot)), alias));
      }
    else if (!strcmp (mode, "context_subscript"))
      {
        tree incoming = gimple_call_arg (body, 1);
        if (TREE_CODE (incoming) != ADDR_EXPR || TREE_CODE (TREE_OPERAND (incoming, 0)) != ARRAY_REF) return 0;
        tree index = create_tmp_var (size_type_node);
        before (body, gimple_build_assign (index, NOP_EXPR, gimple_call_arg (body, 0)));
        tree ref = copy_node (TREE_OPERAND (incoming, 0)); TREE_OPERAND (ref, 1) = index;
        gimple_call_set_arg (body, 1, build_fold_addr_expr (ref));
      }
    else if (!strcmp (mode, "consumed_context")) before (boundary, gimple_build_assign (create_tmp_var (TREE_TYPE (slot)), gimple_call_arg (body, 0)));
    else if (!strcmp (mode, "duplicate_body")) after (body, gimple_copy (body));
    else if (!strcmp (mode, "alias_status"))
      {
        tree result = gimple_call_lhs (body);
        if (TREE_ADDRESSABLE (result)) error ("owner raw status was already addressable");
        tree p = create_tmp_var (build_pointer_type (TREE_TYPE (result)));
        undo_address = gimple_build_assign (p, build_fold_addr_expr (result));
        undo_store = gimple_build_assign (build_simple_mem_ref (p), build_zero_cst (TREE_TYPE (result)));
        after (body, undo_address); after (undo_address, undo_store);
      }
    else if (!strcmp (mode, "copy"))
      { tree ctx = create_tmp_var (TREE_TYPE (gimple_call_arg (body, 0)));
        before (body, gimple_build_assign (ctx, gimple_call_arg (body, 0))); gimple_call_set_arg (body, 0, ctx); }
    else if (!strcmp (mode, "reverse"))
      { gcond *c = as_a<gcond *> (retry); tree a = gimple_cond_lhs (c);
        gimple_cond_set_lhs (c, gimple_cond_rhs (c)); gimple_cond_set_rhs (c, a); }
    else return 0;
    changed = true;
    bool positive = !strcmp (mode, "copy") || !strcmp (mode, "reverse") || !strcmp (mode, "dead_status");
    bool accepted = newtm_owner_verify_entry (fn->decl, body);
    if (accepted != positive) error ("native owner mutation has unexpected result");
    if (redirected) redirect_edge_and_branch (redirected, old_dest);
    if (undo_store)
      { gimple_stmt_iterator at = gsi_for_stmt (undo_store); gsi_remove (&at, false);
        at = gsi_for_stmt (undo_address); gsi_remove (&at, false);
        fputs ("CURRENT_OWNER_RAW_ALIAS unflagged\n", stderr); }
    if (!positive) fputs ("CURRENT_OWNER_MUTATION refused\n", stderr);
    return 0;
  }
};
static void finish (void *, void *)
{
  if (strcmp (mode, "observe")) { if (!changed) error ("native owner probe did not mutate its target"); return; }
  unsigned controllers = 0, operations = 0; cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    {
      newtm_memory_view view;
      if (!newtm_owner_get (node->decl, &view)) continue;
      if (view.state != NEWTM_MEMORY_DEPENDENT || view.dependencies != (NEWTM_MEMORY_CHANNELS | NEWTM_MEMORY_STATUS))
        error ("native owner lost its composition dependencies");
      ++controllers; operations += view.operations;
    }
  if (!controllers || !operations) error ("missing current owner proofs");
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_graph_count (); ++i)
    {
      newtm_graph_view graph;
      if (newtm_obligation_graph_get (i, &graph) && graph.current_approved)
        error ("owner component prematurely approved the current graph");
    }
  fprintf (stderr, "CURRENT_OWNER dependent controllers=%u operations=%u dependencies=3\n", controllers, operations);
}
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i) if (!strcmp (args->argv[i].key, "mode")) mode = args->argv[i].value;
  register_pass_info p = { new probe (g), "ompexp", 1, PASS_POS_INSERT_AFTER };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
