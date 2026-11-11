/* Adversarial checks after ompexp, through the production native verifier. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "gimplify.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "cgraph.h"
#include "cfghooks.h"
#include "tree-cfg.h"
#include "diagnostic-core.h"
#include "newtm-clone.h"
#include "newtm-memory.h"
int plugin_is_GPL_compatible;
static const char *mode = "observe";
static bool changed;
static bool scanning_entries;
static bool named (tree t, const char *s)
{ return t && DECL_P (t) && DECL_NAME (t) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (t)), s); }
static bool builtin (gimple *s, built_in_function code)
{ return is_gimple_call (s) && gimple_call_fndecl (s) && fndecl_built_in_p (gimple_call_fndecl (s), code); }
namespace {
const pass_data data = { GIMPLE_PASS, "current_status_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass {
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (changed || !strcmp (mode, "observe")) return 0;
    bool entry = !strncmp (mode, "entry_", 6);
    const char *action = entry ? mode + 6 : mode;
    if (entry && !scanning_entries)
      {
        scanning_entries = true; cgraph_node *node;
        FOR_EACH_DEFINED_FUNCTION (node)
          if (function *body = DECL_STRUCT_FUNCTION (node->decl))
            if (body->cfg && !newtm_clone_original (node->decl))
              {
                bool switch_function = cfun != body;
                if (switch_function) push_cfun (body);
                execute (body);
                if (switch_function) pop_cfun ();
                if (changed) break;
              }
        scanning_entries = false; return 0;
      }
    bool publication = !strncmp (action, "publish_", 8) || !strcmp (action, "drop_final");
    bool cleanup = !strncmp (action, "cleanup_", 8);
    if (entry && (!DECL_NAME (fn->decl) || strncmp (IDENTIFIER_POINTER (DECL_NAME (fn->decl)), "main", 4))) return 0;
    if (!entry && !named (newtm_clone_original (fn->decl), cleanup ? "captured" : (!strcmp (action, "unknown_return") || !strcmp (action, "subobject_output")) ? "pure_read" : publication ? "advance" : "relay")) return 0;
    gimple *read = NULL, *prepared = NULL, *fail = NULL, *final = NULL, *previous = NULL;
    gimple *guard = NULL; basic_block bb;
    gimple *entry_call = NULL, *begin = NULL, *end = NULL;
    FOR_EACH_BB_FN (bb, fn)
      for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
        {
          gimple *s = gsi_stmt (at);
          if (!read && builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_READ)) read = s;
          if (!prepared && is_gimple_call (s) && named (newtm_clone_original (gimple_call_fndecl (s)), "advance")) prepared = s;
          if (builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL)) fail = s;
          if (builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS)) { final = s; if (!previous) previous = s; }
          if (builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN)) begin = s;
          if (builtin (s, BUILT_IN_GOMP_TRANSACTION_STM_V1_END)) end = s;
          if (entry && is_gimple_call (s) && newtm_clone_region_owner (gimple_call_fndecl (s))) entry_call = s;
        }
    gimple *target = publication ? final : !strcmp (action, "binding_early") ? prepared : read;
    if (entry)
      {
        if (!entry_call) return 0;
        target = !strcmp (action, "begin") || !strcmp (action, "outer") ? begin
          : !strcmp (action, "end") ? end : entry_call;
      }
    if (!target && !fail) return 0;
    if (target)
      FOR_EACH_BB_FN (bb, fn)
        for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
          if (gimple_code (gsi_stmt (at)) == GIMPLE_COND
              && gimple_cond_lhs (gsi_stmt (at)) == gimple_call_lhs (target)) guard = gsi_stmt (at);
    if (entry && target)
      {
        guard = NULL; basic_block block = gimple_bb (target);
        for (unsigned depth = 0; depth < 16 && block && !guard; ++depth)
          {
            for (gimple_stmt_iterator at = gsi_start_bb (block); !gsi_end_p (at); gsi_next (&at))
              if (gimple_code (gsi_stmt (at)) == GIMPLE_COND
                  && gimple_cond_lhs (gsi_stmt (at)) == gimple_call_lhs (target)) guard = gsi_stmt (at);
            if (guard) break;
            basic_block next = NULL; edge e; edge_iterator ei;
            FOR_EACH_EDGE (e, ei, block->succs)
              if (!(e->flags & (EDGE_EH | EDGE_ABNORMAL)))
                { if (next) { next = NULL; break; } next = e->dest; }
            if (!next || next == EXIT_BLOCK_PTR_FOR_FN (fn)) break;
            block = next;
          }
      }
    gimple *remove_address = NULL, *remove_store = NULL;
    edge cycle_edge = NULL; basic_block cycle_destination = NULL;
    if (cleanup)
      {
        gimple *dispatch = NULL;
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
            if (gimple_code (gsi_stmt (at)) == GIMPLE_COND)
              {
                tree lhs = gimple_cond_lhs (gsi_stmt (at));
                if (DECL_P (lhs) && DECL_NAME (lhs) && !strncmp (IDENTIFIER_POINTER (DECL_NAME (lhs)), "finally_tmp", 11)) dispatch = gsi_stmt (at);
              }
        if (!dispatch) return 0;
        if (!strcmp (action, "cleanup_invert"))
          { gcond *c = as_a<gcond *> (dispatch); gimple_cond_set_code (c, gimple_cond_code (c) == EQ_EXPR ? NE_EXPR : EQ_EXPR); }
        else
          {
            tree selector = gimple_cond_lhs (dispatch);
            if (TREE_ADDRESSABLE (selector)) error ("cleanup selector was already addressable");
            tree p = create_tmp_var (build_pointer_type (TREE_TYPE (selector)));
            remove_address = gimple_build_assign (p, build_fold_addr_expr (selector));
            remove_store = gimple_build_assign (build_simple_mem_ref (p), build_one_cst (TREE_TYPE (selector)));
            gimple_stmt_iterator at = gsi_for_stmt (dispatch);
            gsi_insert_before (&at, remove_address, GSI_SAME_STMT); gsi_insert_before (&at, remove_store, GSI_SAME_STMT);
          }
      }
    else if (!strcmp (action, "subobject_output"))
      {
        if (!read) return 0;
        tree type = make_node (RECORD_TYPE), field = build_decl (UNKNOWN_LOCATION, FIELD_DECL, NULL_TREE, integer_type_node);
        TYPE_FIELDS (type) = field; DECL_CONTEXT (field) = type; layout_type (type);
        tree local = create_tmp_var (type); TREE_ADDRESSABLE (local) = 1;
        tree ref = build3 (COMPONENT_REF, integer_type_node, local, field, NULL_TREE);
        gimple_call_set_arg (read, 3, build_fold_addr_expr (ref));
      }
    else if (!strcmp (action, "unknown_return"))
      {
        if (!read) return 0;
        basic_block prefix = gimple_bb (read);
        edge normal = split_block (prefix, read);
        basic_block returned = create_empty_bb (prefix);
        tree status = gimple_call_lhs (read), raw = build_int_cst (TREE_TYPE (status), 77);
        gimple_stmt_iterator at = gsi_last_bb (prefix);
        gsi_insert_after (&at, gimple_build_cond (EQ_EXPR, status, raw, NULL_TREE, NULL_TREE), GSI_NEW_STMT);
        at = gsi_start_bb (returned); gsi_insert_after (&at, gimple_build_return (raw), GSI_NEW_STMT);
        normal->flags = EDGE_FALSE_VALUE; normal->probability = profile_probability::even ();
        edge failed = make_edge (prefix, returned, EDGE_TRUE_VALUE); failed->probability = profile_probability::even ();
        make_edge (returned, EXIT_BLOCK_PTR_FOR_FN (fn), 0);
      }
    else if (!strcmp (action, "drop_fail") || !strcmp (action, "drop_final"))
      {
        gimple *s = !strcmp (action, "drop_fail") ? fail : final;
        if (!s) return 0;
        gimple_stmt_iterator at = gsi_for_stmt (s);
        gsi_replace (&at, gimple_build_assign (gimple_call_lhs (s), build_int_cst (TREE_TYPE (gimple_call_lhs (s)), 2)), false);
      }
    else if (!strcmp (action, "cause"))
      { if (!fail) return 0; gimple_call_set_arg (fail, 1, build_int_cst (TREE_TYPE (gimple_call_arg (fail, 1)), 1)); }
    else if (!strcmp (action, "failure_return"))
      {
        bool found = false;
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
            if (gimple_code (gsi_stmt (at)) == GIMPLE_RETURN
                && gimple_return_retval (as_a<greturn *> (gsi_stmt (at))) == gimple_call_lhs (fail))
              { gimple_return_set_retval (as_a<greturn *> (gsi_stmt (at)), build_zero_cst (TREE_TYPE (gimple_call_lhs (fail)))); found = true; }
        if (!found) return 0;
      }
    else if (!strcmp (action, "private_dummy"))
      {
        if (!read || TREE_CODE (gimple_call_arg (read, 3)) != ADDR_EXPR) return 0;
        tree result = TREE_OPERAND (gimple_call_arg (read, 3), 0); bool found = false;
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
            if (is_gimple_assign (gsi_stmt (at)) && gimple_assign_lhs (gsi_stmt (at)) == result)
              { gimple_assign_set_rhs1 (gsi_stmt (at), build_zero_cst (TREE_TYPE (result))); found = true; }
        if (!found) return 0;
      }
    else
      {
        if (!guard) return 0;
        gimple_stmt_iterator at = gsi_for_stmt (guard); tree status = gimple_call_lhs (target);
        if (!strcmp (action, "copy"))
          {
            tree temp = create_tmp_var (TREE_TYPE (status));
            gsi_insert_before (&at, gimple_build_assign (temp, status), GSI_SAME_STMT);
            gimple_cond_set_lhs (as_a<gcond *> (guard), temp);
          }
        else if (!strcmp (action, "invert") || !strcmp (action, "reverse") || !strcmp (action, "begin") || !strcmp (action, "helper") || !strcmp (action, "end"))
          {
            gcond *c = as_a<gcond *> (guard);
            gimple_cond_set_code (c, gimple_cond_code (c) == EQ_EXPR ? NE_EXPR : EQ_EXPR);
            if (!strcmp (action, "reverse"))
              { edge e; edge_iterator ei; FOR_EACH_EDGE (e, ei, gimple_bb (guard)->succs)
                  if (e->flags & (EDGE_TRUE_VALUE | EDGE_FALSE_VALUE)) e->flags ^= EDGE_TRUE_VALUE | EDGE_FALSE_VALUE; }
          }
        else if (!strcmp (action, "cycle"))
          {
            edge e; edge_iterator ei; FOR_EACH_EDGE (e, ei, gimple_bb (guard)->succs)
              if (e->flags & EDGE_FALSE_VALUE) cycle_edge = e;
            if (!cycle_edge) return 0;
            cycle_destination = cycle_edge->dest;
            cycle_edge = redirect_edge_and_branch (cycle_edge, gimple_bb (guard));
          }
        else if (!strcmp (action, "old_status")) gimple_cond_set_lhs (as_a<gcond *> (guard), gimple_call_lhs (previous));
        else if (!strcmp (action, "publish_bypass")) gimple_cond_set_lhs (as_a<gcond *> (guard), build_zero_cst (TREE_TYPE (status)));
        else if (!strcmp (action, "overwrite"))
          gsi_insert_before (&at, gimple_build_assign (status, build_zero_cst (TREE_TYPE (status))), GSI_SAME_STMT);
        else if (!strcmp (action, "raw_alias"))
          {
            if (TREE_ADDRESSABLE (status)) error ("raw status was already addressable");
            tree p = create_tmp_var (build_pointer_type (TREE_TYPE (status)));
            remove_address = gimple_build_assign (p, build_fold_addr_expr (status));
            remove_store = gimple_build_assign (build_simple_mem_ref (p), build_zero_cst (TREE_TYPE (status)));
            gsi_insert_before (&at, remove_address, GSI_SAME_STMT);
            gsi_insert_before (&at, remove_store, GSI_SAME_STMT);
          }
        else if (!strcmp (action, "early") || !strcmp (action, "binding_early") || !strcmp (action, "outer") || !strcmp (action, "clobber") || !strcmp (action, "constructor"))
          {
            unsigned arg = !strcmp (action, "binding_early") ? gimple_call_num_args (target) - 1 : !strcmp (action, "outer") ? 1 : 3;
            tree pointer = gimple_call_arg (target, arg);
            if (TREE_CODE (pointer) != ADDR_EXPR) return 0;
            tree buffer = TREE_OPERAND (pointer, 0);
            if (!strcmp (action, "clobber"))
              gsi_insert_before (&at, gimple_build_assign (buffer, build_clobber (TREE_TYPE (buffer), CLOBBER_STORAGE_END)), GSI_SAME_STMT);
            else if (!strcmp (action, "constructor"))
              {
                tree type = make_node (RECORD_TYPE), field = build_decl (UNKNOWN_LOCATION, FIELD_DECL, NULL_TREE, TREE_TYPE (buffer));
                TYPE_FIELDS (type) = field; DECL_CONTEXT (field) = type; layout_type (type);
                vec<constructor_elt, va_gc> *fields = NULL; CONSTRUCTOR_APPEND_ELT (fields, field, buffer);
                tree temp = create_tmp_var (type);
                gsi_insert_before (&at, gimple_build_assign (temp, build_constructor (type, fields)), GSI_SAME_STMT);
              }
            else gsi_insert_before (&at, gimple_build_assign (create_tmp_var (TREE_TYPE (buffer)), buffer), GSI_SAME_STMT);
          }
        else return 0;
      }
    changed = true;
    if (strcmp (action, "copy") && strcmp (action, "reverse"))
      {
        if (entry ? newtm_status_verify_entry (fn->decl, entry_call) : newtm_status_verify_cfg (fn->decl)) error ("native status mutation was accepted");
        if (cycle_edge) redirect_edge_and_branch (cycle_edge, cycle_destination);
        if (remove_store)
          { gimple_stmt_iterator at = gsi_for_stmt (remove_store); gsi_remove (&at, false);
            at = gsi_for_stmt (remove_address); gsi_remove (&at, false);
            fputs ("CURRENT_STATUS_RAW_ALIAS unflagged\n", stderr); }
        fputs ("CURRENT_STATUS_MUTATION refused\n", stderr);
      }
    return 0;
  }
};
static void finish (void *, void *)
{
  if (strcmp (mode, "observe")) { if (!changed) error ("native status probe did not mutate its target"); return; }
  unsigned functions = 0, operations = 0; cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    {
      newtm_memory_view view;
      if (!newtm_status_get (node->decl, &view)) continue;
      if (view.state != NEWTM_MEMORY_DEPENDENT || view.dependencies != NEWTM_MEMORY_OWNER) error ("native status lost OWNER dependency");
      ++functions; operations += view.operations;
    }
  if (!functions || !operations) error ("missing native status proofs");
  fprintf (stderr, "CURRENT_STATUS dependent functions=%u operations=%u dependencies=4\n", functions, operations);
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
