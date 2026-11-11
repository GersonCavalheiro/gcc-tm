/* Actual residual effects, after ompexp and before the production audit. */
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
#include "cgraph.h"
#include "diagnostic-core.h"
#include "internal-fn.h"
#include "newtm-clone.h"
#include "newtm-memory.h"
#include "newtm-obligation.h"
int plugin_is_GPL_compatible;
static const char *mode = "observe";
static bool changed, scanning;
static bool named (tree d, const char *name)
{ return d && DECL_P (d) && DECL_NAME (d) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (d)), name); }
static void before (gimple *target, gimple *s)
{ gimple_stmt_iterator it = gsi_for_stmt (target); gsi_insert_before (&it, s, GSI_SAME_STMT); }
namespace {
const pass_data data = { GIMPLE_PASS, "coverage_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass {
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (changed || !strcmp (mode, "observe")) return 0;
    bool entry = !strncmp (mode, "entry_", 6);
    const char *action = entry ? mode + 6 : mode;
    if (entry && !scanning)
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
    bool prepared = !strncmp (action, "prepared_", 9);
    if (!entry && !named (newtm_clone_original (fn->decl), prepared ? "relay" : "step")) return 0;
    if (entry && (!DECL_NAME (fn->decl) || strncmp (IDENTIFIER_POINTER (DECL_NAME (fn->decl)), "main", 4))) return 0;
    gimple *target = NULL, *entry_call = NULL, *private_read = NULL, *private_write = NULL, *callee = NULL, *guard = NULL;
    tree program = DECL_ARGUMENTS (fn->decl);
    for (unsigned i = 0; i < 2 && program; ++i) program = DECL_CHAIN (program);
    basic_block bb;
    FOR_EACH_BB_FN (bb, fn)
      for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
        {
          gimple *s = gsi_stmt (it);
          if (!target && is_gimple_call (s)) target = s;
          if (is_gimple_call (s) && newtm_clone_region_owner (gimple_call_fndecl (s))) entry_call = s;
          if (is_gimple_call (s) && named (newtm_clone_original (gimple_call_fndecl (s)), "step")) callee = s;
          if (gimple_code (s) == GIMPLE_COND) guard = s;
          if (is_gimple_assign (s) && !gimple_clobber_p (s))
            {
              if (gimple_assign_single_p (s) && TREE_CODE (gimple_assign_rhs1 (s)) == MEM_REF
                  && program && TREE_OPERAND (gimple_assign_rhs1 (s), 0) == program) private_read = s;
              if (TREE_CODE (gimple_assign_lhs (s)) == MEM_REF
                  && program && TREE_OPERAND (gimple_assign_lhs (s), 0) == program) private_write = s;
            }
        }
    if (entry) target = entry_call;
    if (!target) return 0;
    if (!(entry ? newtm_coverage_verify_entry (fn->decl, entry_call) : newtm_coverage_verify_cfg (fn->decl)))
      { error ("coverage baseline refused before mutation"); return 0; }
    tree global = NULL_TREE, array = NULL_TREE; varpool_node *var;
    FOR_EACH_VARIABLE (var)
      { if (named (var->decl, "unrelated")) global = var->decl;
        if (named (var->decl, "unrelated_array")) array = var->decl; }
    if (!global) return 0;
    tree temp = create_tmp_var (TREE_TYPE (global));
    if (!strcmp (action, "prefix"))
      {
        if (!entry) return 0;
        gimple *initial = NULL;
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
            {
              gimple *s = gsi_stmt (it);
              if (is_gimple_call (s) && gimple_call_fndecl (s)
                  && fndecl_built_in_p (gimple_call_fndecl (s), BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN))
                {
                  tree slot = TREE_OPERAND (gimple_call_arg (s, 0), 0);
                  basic_block b;
                  FOR_EACH_BB_FN (b, fn)
                    for (gimple_stmt_iterator jt = gsi_start_bb (b); !gsi_end_p (jt); gsi_next (&jt))
                      if (is_gimple_assign (gsi_stmt (jt)) && gimple_assign_lhs (gsi_stmt (jt)) == slot
                          && gimple_assign_single_p (gsi_stmt (jt)) && integer_zerop (gimple_assign_rhs1 (gsi_stmt (jt))))
                        initial = gsi_stmt (jt);
                  break;
                }
            }
        if (!initial) return 0;
        before (initial, gimple_build_assign (temp, global));
      }
    else if (!strcmp (action, "read")) before (target, gimple_build_assign (temp, global));
    else if (!strcmp (action, "write")) before (target, gimple_build_assign (global, build_zero_cst (TREE_TYPE (global))));
    else if (!strcmp (action, "alias_read") || !strcmp (action, "alias_write"))
      {
        tree p = create_tmp_var (build_pointer_type (TREE_TYPE (global)));
        before (target, gimple_build_assign (p, build_fold_addr_expr (global)));
        before (target, !strcmp (action, "alias_read") ? gimple_build_assign (temp, build_simple_mem_ref (p))
                : gimple_build_assign (build_simple_mem_ref (p), build_zero_cst (TREE_TYPE (global))));
      }
    else if (!strcmp (action, "clobber"))
      before (target, gimple_build_assign (global, build_clobber (TREE_TYPE (global), CLOBBER_STORAGE_END)));
    else if (!strcmp (action, "volatile"))
      {
        TREE_THIS_VOLATILE (global) = 1;
        gimple *s = gimple_build_assign (temp, global);
        gimple_set_has_volatile_ops (s, false); before (target, s);
      }
    else if (!strcmp (action, "qualified"))
      {
        tree d = create_tmp_var (build_qualified_type (TREE_TYPE (global), TYPE_QUAL_VOLATILE));
        TREE_THIS_VOLATILE (d) = 0;
        gimple *s = gimple_build_assign (d, build_zero_cst (TREE_TYPE (d)));
        gimple_set_has_volatile_ops (s, false); before (target, s);
      }
    else if (!strcmp (action, "indirect"))
      {
        tree original = entry ? NULL_TREE : newtm_clone_original (fn->decl);
        if (!original) return 0;
        tree p = create_tmp_var (build_pointer_type (TREE_TYPE (original)));
        before (target, gimple_build_assign (p, build_fold_addr_expr (original)));
        gcall *s = gimple_build_call (p, 1, program);
        gimple_call_set_lhs (s, create_tmp_var (integer_type_node)); before (target, s);
      }
    else if (!strcmp (action, "asm")) before (target, gimple_build_asm_vec ("", NULL, NULL, NULL, NULL));
    else if (!strcmp (action, "index"))
      {
        if (!array) return 0;
        tree ref = build4 (ARRAY_REF, integer_type_node, array, global, NULL_TREE, NULL_TREE);
        before (target, gimple_build_assign (create_tmp_var (build_pointer_type (integer_type_node)), build_fold_addr_expr (ref)));
      }
    else if (!strcmp (action, "condition"))
      { if (!guard) return 0;
        gimple_cond_set_lhs (as_a<gcond *> (guard), global);
        gimple_cond_set_rhs (as_a<gcond *> (guard), build_zero_cst (TREE_TYPE (global))); }
    else if (!strcmp (action, "return"))
      {
        greturn *returned = NULL;
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
            if (gimple_code (gsi_stmt (it)) == GIMPLE_RETURN) returned = as_a<greturn *> (gsi_stmt (it));
        if (!returned) return 0;
        gimple_return_set_retval (returned, global);
      }
    else if (!strcmp (action, "internal_argument") || !strcmp (action, "arithmetic"))
      {
        tree pair = create_tmp_var (build_complex_type (TREE_TYPE (global)));
        gcall *s = gimple_build_call_internal (IFN_ADD_OVERFLOW, 2,
          !strcmp (action, "arithmetic") ? build_one_cst (TREE_TYPE (global)) : global,
          build_zero_cst (TREE_TYPE (global)));
        gimple_call_set_lhs (s, pair); before (target, s);
      }
    else if (!strcmp (action, "builtin"))
      {
        gcall *s = gimple_build_call (builtin_decl_explicit (BUILT_IN_EXPECT), 2,
                                    build_zero_cst (long_integer_type_node), build_zero_cst (long_integer_type_node));
        gimple_call_set_lhs (s, create_tmp_var (long_integer_type_node)); before (target, s);
      }
    else if (!strcmp (action, "cfa"))
      {
        gcall *s = gimple_build_call (builtin_decl_explicit (BUILT_IN_DWARF_CFA), 1, integer_zero_node);
        gimple_call_set_lhs (s, create_tmp_var (ptr_type_node)); before (target, s);
      }
    else if (!strcmp (action, "extra_status"))
      {
        gcall *s = gimple_build_call (builtin_decl_explicit (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS), 1,
                                     entry ? gimple_call_arg (entry_call, 0) : DECL_ARGUMENTS (fn->decl));
        gimple_call_set_lhs (s, create_tmp_var (uint32_type_node)); before (target, s);
      }
    else if (!strcmp (action, "duplicate_private_read"))
      {
        if (!private_read) return 0;
        before (private_read, gimple_build_assign (create_tmp_var (TREE_TYPE (gimple_assign_lhs (private_read))), gimple_assign_rhs1 (private_read)));
      }
    else if (!strcmp (action, "duplicate_private_write"))
      { if (!private_write) return 0; before (private_write, gimple_copy (private_write)); }
    else if (!strcmp (action, "prepared_duplicate"))
      { if (!callee) return 0; before (callee, gimple_copy (callee)); }
    else if (!strcmp (action, "prepared_original"))
      {
        if (!callee) return 0;
        tree original = newtm_clone_original (gimple_call_fndecl (callee));
        gcall *s = gimple_build_call (original, 1, gimple_call_arg (callee, 2));
        gimple_call_set_lhs (s, create_tmp_var (integer_type_node)); before (callee, s);
      }
    else if (!strcmp (action, "prepared_constructor") || !strcmp (action, "prepared_hidden_call"))
      {
        if (!callee || TREE_CODE (gimple_call_arg (callee, 1)) != ADDR_EXPR) return 0;
        tree buffer = get_base_address (TREE_OPERAND (gimple_call_arg (callee, 1), 0));
        if (!buffer || TREE_CODE (TREE_TYPE (buffer)) != ARRAY_TYPE) return 0;
        tree element = TREE_TYPE (TREE_TYPE (buffer)), storage = DECL_CHAIN (TYPE_FIELDS (element));
        vec<constructor_elt, va_gc> *fields = NULL, *items = NULL;
        tree value = global;
        if (!strcmp (action, "prepared_hidden_call"))
          {
            tree foreign = NULL_TREE; cgraph_node *node;
            FOR_EACH_DEFINED_FUNCTION (node) if (named (node->decl, "foreign")) foreign = node->decl;
            if (!foreign) return 0;
            tree call = build_call_expr (foreign, 1, integer_zero_node);
            TREE_SIDE_EFFECTS (call) = 0;
            value = fold_convert (TREE_TYPE (storage), call);
            TREE_SIDE_EFFECTS (value) = 0;
          }
        CONSTRUCTOR_APPEND_ELT (fields, storage, value);
        CONSTRUCTOR_APPEND_ELT (items, size_zero_node, build_constructor (element, fields));
        gimple *returned = NULL;
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
            if (gimple_code (gsi_stmt (it)) == GIMPLE_RETURN) returned = gsi_stmt (it);
        if (!returned) return 0;
        before (returned, gimple_build_assign (buffer, build_constructor (TREE_TYPE (buffer), items)));
      }
    else if (!strcmp (action, "scalar")) before (target, gimple_build_assign (temp, build_zero_cst (TREE_TYPE (temp))));
    else if (!strcmp (action, "address"))
      {
        TREE_ADDRESSABLE (global) = 1;
        before (target, gimple_build_assign (create_tmp_var (build_pointer_type (TREE_TYPE (global))), build_fold_addr_expr (global)));
      }
    else return 0;
    changed = true;
    bool positive = !strcmp (action, "scalar") || !strcmp (action, "address") || !strcmp (action, "arithmetic");
    bool accepted = entry ? newtm_coverage_verify_entry (fn->decl, entry_call) : newtm_coverage_verify_cfg (fn->decl);
    if (accepted != positive) error ("coverage mutation has unexpected result");
    if (!positive) fputs ("CURRENT_COVERAGE_MUTATION refused\n", stderr);
    return 0;
  }
};
static void finish (void *, void *)
{
  if (strcmp (mode, "observe")) { if (!changed) error ("coverage probe did not mutate"); return; }
  unsigned functions = 0, entries = 0, effects = 0; cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    {
      newtm_memory_view v;
      if (newtm_coverage_get (node->decl, &v))
        { if (v.state != NEWTM_MEMORY_DEPENDENT || v.dependencies != 15) error ("coverage lost component dependencies");
          ++functions; effects += v.operations; }
      if (newtm_coverage_entry_get (node->decl, &v))
        { if (v.state != NEWTM_MEMORY_DEPENDENT || v.dependencies != 15) error ("entry coverage lost dependencies");
          ++entries; effects += v.operations; }
    }
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_graph_count (); ++i)
    { newtm_graph_view v; if (newtm_obligation_graph_get (i, &v) && v.current_approved) error ("coverage prematurely approved current graph"); }
  if (!functions || !entries || !effects) error ("missing current coverage components");
  fprintf (stderr, "CURRENT_COVERAGE dependent functions=%u entries=%u effects=%u dependencies=15\n", functions, entries, effects);
}
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i) if (!strcmp (args->argv[i].key, "mode")) mode = args->argv[i].value;
  register_pass_info p = { new probe (g), "ompexp", 1, PASS_POS_INSERT_AFTER };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL); return 0;
}
