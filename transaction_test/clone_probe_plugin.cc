/* Test-only entry points into dormant clones and failure injection. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "context.h"
#include "cgraph.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "function.h"
#include "gimple-walk.h"
#include "ggc.h"
#include "newtm-clone.h"
#include "newtm-abi.h"
#include "newtm-descriptor.h"
#include "newtm-occurrence.h"
#include "newtm-memory.h"
#include "diagnostic-core.h"
int plugin_is_GPL_compatible;
static const char *injected;
static const char *corrupted;
static bool corrupt_now;
static unsigned corruptions;
static unsigned cause = 1, invoked, clones;
static tree find_clone (const char *name)
{
  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    if (tree original = newtm_clone_original (node->decl))
      if (DECL_NAME (original) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), name))
        return node->decl;
  return NULL_TREE;
}
static tree inspect (gimple_stmt_iterator *it, bool *, walk_stmt_info *)
{
  gimple *stmt = gsi_stmt (*it);
  tree original = newtm_clone_original (current_function_decl);
  /* Deliberate post-verification ABI fault, solely for the caller boundary
     test. No expired descriptor is read and no approval is manufactured. */
  if (corrupt_now && corrupted && original && !strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), corrupted)
      && is_gimple_assign (stmt) && TREE_CODE (gimple_assign_lhs (stmt)) == MEM_REF
      && TREE_OPERAND (gimple_assign_lhs (stmt), 0) == newtm_clone_channel (current_function_decl, 3))
    {
      tree p = DECL_CHAIN (DECL_CHAIN (DECL_ARGUMENTS (current_function_decl)));
      gcc_assert (POINTER_TYPE_P (TREE_TYPE (p))
                  && types_compatible_p (TREE_TYPE (p), TREE_TYPE (gimple_assign_lhs (stmt))));
      gsi_replace (it, gimple_build_assign (gimple_assign_lhs (stmt), p), false);
      stmt = gsi_stmt (*it);
      ++corruptions;
    }
  if (original && !strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), "assign_global")
      && is_gimple_assign (stmt) && gimple_assign_lhs (stmt)
      && VAR_P (gimple_assign_lhs (stmt)) && DECL_NAME (gimple_assign_lhs (stmt))
      && !strcmp (IDENTIFIER_POINTER (DECL_NAME (gimple_assign_lhs (stmt))), "global"))
    {
      tree d = newtm_descriptor_find (stmt, NEWTM_DESC_WRITE);
      tree occurrence = newtm_occurrence_binding (stmt);
      gcc_assert (d && TREE_VEC_ELT (d, NEWTM_D_VALID)
                  && TREE_VEC_ELT (d, NEWTM_D_STORAGE));
      gcc_assert (occurrence && (tree_to_uhwi (TREE_VEC_ELT (occurrence, 7)) & 2));
    }
  if (!is_gimple_call (stmt)) return NULL_TREE;
  tree fn = gimple_call_fndecl (stmt);
  if (!fn || !DECL_NAME (fn)) return NULL_TREE;
  const char *name = IDENTIFIER_POINTER (DECL_NAME (fn));
  if (!strncmp (name, "invoke_", 7))
    {
      tree clone = find_clone (name + 7);
      if (!clone) { error ("missing prepared clone for %s", name + 7); return NULL_TREE; }
      gimple_call_set_fndecl (stmt, clone);
      gimple_call_set_fntype (as_a<gcall *> (stmt), TREE_TYPE (clone));
      ++invoked;
    }
  if (newtm_clone_original (current_function_decl))
    {
      gcc_assert (!newtm_clone_for (fn));
      gcc_assert (strcmp (name, "__newtm_publish_result"));
      gcc_assert (strcmp (name, "__builtin_GOMP_transaction_begin"));
      gcc_assert (strcmp (name, "__builtin_GOMP_transaction_register"));
      gcc_assert (strcmp (name, "__builtin_GOMP_transaction_end"));
    }
  return NULL_TREE;
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_clone_probe", OPTGROUP_NONE,
                         TV_NONE, 0, 0, 0, 0, 0 };
class probe : public gimple_opt_pass {
public:
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *) final override {
    tree fn = current_function_decl, original = newtm_clone_original (fn);
    ggc_collect (GGC_COLLECT_FORCE);
    gcc_assert (original == newtm_clone_original (fn));
    if (original)
      {
        ++clones;
        gcc_assert (!TREE_PUBLIC (fn));
        gcc_assert (TREE_TYPE (TREE_TYPE (fn)) == uint32_type_node);
        if (injected && !strcmp (injected, IDENTIFIER_POINTER (DECL_NAME (original))))
          {
            gbind *bind = as_a<gbind *> (gimple_seq_first_stmt (gimple_body (fn)));
            tree status = create_tmp_var (uint32_type_node, "injected_status");
            DECL_CHAIN (status) = gimple_bind_vars (bind);
            gimple_bind_set_vars (bind, status);
            tree args[] = { newtm_clone_context (fn), build_int_cst (uint32_type_node, cause) };
            gimple_seq seq = NULL;
            gimple_seq_add_stmt (&seq, newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL, status, 2, args));
            gimple_seq_add_stmt (&seq, gimple_build_return (status));
            gimple_stmt_iterator start = gsi_start (*gimple_bind_body_ptr (bind));
            gsi_insert_seq_before (&start, seq, GSI_SAME_STMT);
          }
      }
    walk_stmt_info wi = {};
    walk_gimple_seq (gimple_body (fn), inspect, NULL, &wi);
    return 0;
  }
};
}
static void finish (void *, void *) {
  if (!invoked || !clones) error ("clone probe did not exercise prepared functions");
  if (corrupted && corruptions != 1) error ("boundary probe did not corrupt one verified publication");
}
static void corrupt_after_verification (void *, void *)
{
  if (!corrupted || seen_error ()) return;
  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    if (tree original = newtm_clone_original (node->decl))
      if (DECL_NAME (original) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), corrupted))
        {
          newtm_memory_view proof;
          if (!newtm_channels_get (node->decl, &proof) || proof.state != NEWTM_MEMORY_DEPENDENT)
            { error ("boundary fault lacks a preceding current channel inspection"); return; }
          if (!newtm_status_get (node->decl, &proof) || proof.state != NEWTM_MEMORY_DEPENDENT)
            { error ("boundary fault lacks a preceding current status inspection"); return; }
          push_cfun (DECL_STRUCT_FUNCTION (node->decl));
          corrupt_now = true;
          basic_block bb;
          FOR_EACH_BB_FN (bb, cfun)
            for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
              { bool handled = false; inspect (&at, &handled, NULL); }
          corrupt_now = false;
          pop_cfun ();
          fputs ("CLONE_BOUNDARY_FAULT after_current_channel_proof\n", stderr);
          fputs ("CLONE_BOUNDARY_FAULT after_current_status_proof\n", stderr);
        }
}
int plugin_init (plugin_name_args *info, plugin_gcc_version *version) {
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < info->argc; ++i)
    {
      if (!strcmp (info->argv[i].key, "inject")) injected = info->argv[i].value;
      if (!strcmp (info->argv[i].key, "corrupt")) corrupted = info->argv[i].value;
      if (!strcmp (info->argv[i].key, "cause")) cause = atoi (info->argv[i].value);
    }
  register_pass_info p = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (info->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  register_callback (info->base_name, PLUGIN_ALL_IPA_PASSES_START, corrupt_after_verification, NULL);
  return 0;
}
