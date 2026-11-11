/* Exercise the real internal resolver, including a constant forged PRIVATE
   tuple. Hidden copies isolate compiler-private maps from the running GCC.
   This is a component test, not a claim of whole-graph mutation coverage. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "../gcc-src-tm/gcc/newtm-memory.cc"
#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "cgraph.h"
int plugin_is_GPL_compatible __attribute__((visibility ("default")));
static const char *mode = "global_private";
static bool tested;
static bool named (tree d, const char *name)
{ return d && DECL_NAME (d) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (d)), name); }
namespace {
const pass_data data = {GIMPLE_PASS, "storage_rule_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0};
struct probe : gimple_opt_pass {
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (tested || !named (fn->decl, "storage_probe")) return 0;
    tree global = NULL_TREE, local = NULL_TREE, decl; unsigned i;
    varpool_node *node;
    FOR_EACH_VARIABLE (node) if (named (node->decl, "storage_global")) global = node->decl;
    FOR_EACH_LOCAL_DECL (fn, i, decl) if (named (decl, "storage_local")) local = decl;
    if (!global || !local) { error ("storage rule fixture lost its allocations"); return 0; }
    newtm_flow_graph flow; const char *why = NULL; gimple *bad = NULL;
    if (!flow.build_cfg (fn, &why, &bad)) { error ("storage rule fixture has no valid CFG"); return 0; }
    gimple *use = NULL;
    for (unsigned j = 0; j < flow.node_count (); ++j)
      if (flow.node (j).reachable && flow.node (j).statement) { use = flow.node (j).statement; break; }
    if (!use) { error ("storage rule fixture has no use"); return 0; }
    resolver r (fn->decl, flow, true);
    tree values[TUPLE_FIELDS] = {};
    bool own = !strcmp (mode, "valid_private") || !strcmp (mode, "wrong_branch");
    values[OBJECT] = build_fold_addr_expr (own ? local : global);
    values[EXTENT] = !strcmp (mode, "range") ? size_zero_node : size_one_node;
    values[OFFSET] = size_zero_node; values[SIZE] = size_zero_node;
    values[KIND] = build_int_cst (unsigned_type_node, 0);
    unsigned storage = (!strcmp (mode, "registered") || !strcmp (mode, "range")
                        || !strcmp (mode, "wrong_branch")) ? 0
      : !strcmp (mode, "invalid_tag") ? 2 : 1;
    values[STORAGE] = build_int_cst (unsigned_type_node, storage);
    if (!strcmp (mode, "first_failure")) r.refuse ("earlier identity failure", NEWTM_V03);
    bool accepted = r.ranges (use, values, !strcmp (mode, "wrong_branch")
                             ? NEWTM_MEMORY_PRIVATE_READ : NEWTM_MEMORY_ADDRESS);
    tested = true;
    bool positive = !strcmp (mode, "valid_private") || !strcmp (mode, "registered");
    fprintf (stderr, "STORAGE_RULE mode=%s accepted=%u rule=%s\n", mode, accepted, r.failure_rule_name ());
    if (positive)
      { if (!accepted || r.failure) error ("valid storage classification refused"); return 0; }
    bool identity = !strcmp (mode, "range") || !strcmp (mode, "first_failure");
    if (accepted || !r.failure || r.failure_rule != (identity ? NEWTM_V03 : NEWTM_V04))
      { error ("storage failure lost its cause or rule"); return 0; }
    /* The actual production consumer must retain the typed rule. */
    memory_range_fail (r, use);
    return 0;
  }
};
}
static void finish (void *, void *)
{ if (!tested) error ("storage classification probe did not run"); }
int __attribute__((visibility ("default")))
plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i)
    if (!strcmp (args->argv[i].key, "mode")) mode = args->argv[i].value;
  register_pass_info p = {new probe (g), "ompexp", 1, PASS_POS_INSERT_AFTER};
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
