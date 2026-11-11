/* Exercise the production channel checker on actual post-ompexp GIMPLE. */
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
#include "diagnostic-core.h"
#include "newtm-clone.h"
#include "newtm-memory.h"
int plugin_is_GPL_compatible;
static const char *mode = "observe";
static bool changed;
static bool scanning_entries;
static bool named (tree t, const char *name)
{ return t && DECL_P (t) && DECL_NAME (t) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (t)), name); }
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_channel_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass {
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (changed || !strcmp (mode, "observe")) return 0;
    bool entry = !strncmp (mode, "entry_", 6);
    const char *action = entry ? mode + 6 : mode;
    if (entry && !scanning_entries)
      {
        /* ompexp creates outlined bodies during the parent's pass. They need
           not run that pass again, so inspect the actual newly created CFGs. */
        scanning_entries = true;
        cgraph_node *node;
        FOR_EACH_DEFINED_FUNCTION (node)
          if (function *body = DECL_STRUCT_FUNCTION (node->decl))
            if (body->cfg)
              {
                bool switch_function = cfun != body;
                if (switch_function) push_cfun (body);
                execute (body);
                if (switch_function) pop_cfun ();
                if (changed) break;
              }
        scanning_entries = false;
        return 0;
      }
    if (!entry && !newtm_clone_original (fn->decl)) return 0;
    basic_block bb; gimple *target = NULL;
    FOR_EACH_BB_FN (bb, fn)
      {
      for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
        {
          gimple *s = gsi_stmt (it);
          if (!strcmp (action, "incoming_write"))
            {
              tree incoming = newtm_clone_channel (fn->decl, 2);
              if (!named (newtm_clone_original (fn->decl), "relay")) continue;
              tree value = newtm_clone_field (newtm_clone_identity (build_simple_mem_ref (incoming)), 1);
              gsi_insert_before (&it, gimple_build_assign (value, size_one_node), GSI_SAME_STMT);
              changed = true; break;
            }
          if (!strncmp (action, "publish_", 8))
            {
              if (!named (newtm_clone_original (fn->decl), "shift") || !is_gimple_assign (s)) continue;
              tree out = newtm_clone_channel (fn->decl, 3), bound = newtm_clone_channel (fn->decl, 4);
              tree l = gimple_assign_lhs (s);
              if (TREE_CODE (l) != MEM_REF) continue;
              if (!strcmp (action, "publish_value") && TREE_OPERAND (l, 0) == out)
                gimple_assign_set_rhs1 (s, build_zero_cst (TREE_TYPE (l)));
              else if (!strcmp (action, "publish_binding") && TREE_OPERAND (l, 0) == bound)
                {
                  tree ref = newtm_clone_storage (gimple_assign_rhs1 (s));
                  gsi_insert_before (&it, gimple_build_assign (ref, build_int_cst (TREE_TYPE (ref), 0)), GSI_SAME_STMT);
                }
              else continue;
              changed = true; break;
            }
          if (!is_gimple_call (s)) continue;
          tree callee = gimple_call_fndecl (s);
          if (entry ? !newtm_clone_region_owner (callee) : !named (newtm_clone_original (callee), "relay")) continue;
          tree incoming = gimple_call_arg (s, 1);
          if (TREE_CODE (incoming) != ADDR_EXPR) continue;
          tree first = TREE_OPERAND (incoming, 0), array = get_base_address (first);
          if (!VAR_P (array) || TREE_CODE (TREE_TYPE (array)) != ARRAY_TYPE) continue;
          if (!strcmp (action, "copy"))
            {
              tree temp = create_tmp_var (TREE_TYPE (incoming));
              gsi_insert_before (&it, gimple_build_assign (temp, incoming), GSI_SAME_STMT);
              gimple_call_set_arg (s, 1, temp);
            }
          else if (!strcmp (action, "incoming_short"))
            gimple_call_set_arg (s, 1, build_fold_addr_expr (build4 (ARRAY_REF,
              TREE_TYPE (first), array, size_one_node, NULL_TREE, NULL_TREE)));
          else if (!strcmp (action, "overlap"))
            {
              unsigned last = gimple_call_num_args (s) - 1;
              gimple_call_set_arg (s, last, incoming);
            }
          else if (!strcmp (action, "result_dead"))
            {
              tree output = gimple_call_arg (s, gimple_call_num_args (s) - 2);
              if (TREE_CODE (output) != ADDR_EXPR) continue;
              tree ref = TREE_OPERAND (output, 0);
              tree clobber = build_clobber (TREE_TYPE (ref), CLOBBER_STORAGE_END);
              gsi_insert_before (&it, gimple_build_assign (ref, clobber), GSI_SAME_STMT);
            }
          else if (!strcmp (action, "incoming_dead"))
            gsi_insert_before (&it, gimple_build_assign (array,
              build_clobber (TREE_TYPE (array), CLOBBER_STORAGE_END)), GSI_SAME_STMT);
          else if (!strcmp (action, "escape"))
            {
              /* Passing the transport itself as program memory preserves the
                 call prototype but violates its exclusive compiler role. */
              tree temp = create_tmp_var (TREE_TYPE (gimple_call_arg (s, 2)));
              gsi_insert_before (&it, gimple_build_assign (temp, NOP_EXPR, incoming), GSI_SAME_STMT);
              gimple_call_set_arg (s, 2, temp);
            }
          else if (!strcmp (action, "slot_swap") || !strcmp (action, "missing_field") || !strcmp (action, "alias"))
            {
              tree storage = newtm_clone_storage (first);
              if (!strcmp (action, "slot_swap"))
                {
                  tree other = build4 (ARRAY_REF, TREE_TYPE (first), array, size_one_node, NULL_TREE, NULL_TREE);
                  tree destination = newtm_clone_field (newtm_clone_identity (first), 0);
                  tree temp = create_tmp_var (TREE_TYPE (destination));
                  gsi_insert_before (&it, gimple_build_assign (temp, newtm_clone_field (newtm_clone_identity (other), 0)), GSI_SAME_STMT);
                  gsi_insert_before (&it, gimple_build_assign (destination, temp), GSI_SAME_STMT);
                }
              else if (!strcmp (action, "missing_field"))
                {
                  bool removed = false; basic_block scan;
                  FOR_EACH_BB_FN (scan, fn)
                    for (gimple_stmt_iterator at = gsi_start_bb (scan); !gsi_end_p (at); gsi_next (&at))
                      if (is_gimple_assign (gsi_stmt (at)) && operand_equal_p (gimple_assign_lhs (gsi_stmt (at)), storage, 0))
                        { gsi_remove (&at, false); removed = true; break; }
                  if (!removed) continue;
                }
              else
                {
                  tree temp = create_tmp_var (build_pointer_type (TREE_TYPE (storage)));
                  gsi_insert_before (&it, gimple_build_assign (temp, build_fold_addr_expr (storage)), GSI_SAME_STMT);
                  gsi_insert_before (&it, gimple_build_assign (build_simple_mem_ref (temp), build_int_cst (TREE_TYPE (storage), 9)), GSI_SAME_STMT);
                }
            }
          else continue;
          changed = true; target = s; break;
        }
      if (changed) break;
      }
    if (changed && strcmp (action, "copy"))
      {
        bool accepted = entry ? newtm_channels_verify_entry (fn->decl, target) : newtm_channels_verify_cfg (fn->decl);
        if (accepted) error ("channel mutation was accepted");
        fputs ("CHANNEL_MUTATION refused\n", stderr);
      }
    return 0;
  }
};
static void finish (void *, void *)
{
  if (strcmp (mode, "observe"))
    { if (!changed) error ("channel probe did not mutate its target"); return; }
  unsigned functions = 0, operations = 0; cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    {
      newtm_memory_view view;
      if (!newtm_channels_get (node->decl, &view)) continue;
      if (view.state != NEWTM_MEMORY_DEPENDENT || view.dependencies != (NEWTM_MEMORY_STATUS | NEWTM_MEMORY_OWNER))
        error ("channel proof lost its explicit dependencies");
      ++functions; operations += view.operations;
    }
  if (!functions || !operations) error ("missing current channel proofs");
  fprintf (stderr, "CHANNEL_MODEL dependent functions=%u operations=%u dependencies=6\n", functions, operations);
}
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i) if (!strcmp (args->argv[i].key, "mode")) mode = args->argv[i].value;
  register_pass_info info = { new probe (g), "ompexp", 1, PASS_POS_INSERT_AFTER };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &info);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
