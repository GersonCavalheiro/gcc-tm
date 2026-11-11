/* Synthetic negative IR is inspected, never inserted in executable bodies. */
#include "flow_probe_common.h"
#include "gimple-iterator.h"
#undef require
#include "newtm-status.h"
#include "newtm-abi.h"
#define expect(V) require_at ((V), __LINE__)
struct invocation { gimple *call; tree value, binding; };
struct local_producer { gimple *call, *store; };
static unsigned status_cases;
static bool unit_only;
static tree service, status_var, value, binding, consumer;
static gimple *invoke (tree status, tree out = NULL_TREE, tree bound = NULL_TREE)
{
  gcall *c = gimple_build_call (service, 2,
    out ? build_fold_addr_expr (out) : null_pointer_node,
    bound ? build_fold_addr_expr (bound) : null_pointer_node);
  gimple_call_set_lhs (c, status); return c;
}
static gimple *check (tree status, tree yes, tree no, tree_code code = EQ_EXPR)
{ return gimple_build_cond (code, status, build_zero_cst (TREE_TYPE (status)), yes, no); }
static gimple *use (tree object)
{ return gimple_build_assign (consumer, object); }
static void prove (tree fn, gimple_seq body, std::initializer_list<invocation> calls,
                   bool expected, std::initializer_list<gimple *> resets = {},
                   std::initializer_list<local_producer> alternatives = {})
{
  sample s (fn, body);
  newtm_status_analysis proof (*s.graph);
  for (const invocation &c : calls) proof.operation (c.call, c.value, c.binding);
  for (gimple *r : resets) proof.reset (r);
  for (const local_producer &p : alternatives) proof.alternative (p.call, p.store);
  ggc_collect (GGC_COLLECT_FORCE);
  const char *why = NULL; gimple *bad = NULL;
  bool actual = proof.verify (&why, &bad);
  if (actual != expected)
    internal_error ("status probe case %u: expected %u, observed %u: %s", status_cases,
                    expected, actual, why ? why : "proved");
  expect (expected ? !why : why && strstr (why, "V05"));
  ggc_collect (GGC_COLLECT_FORCE); ++status_cases;
}
static void synthetic (tree fn)
{
  x = keep (create_tmp_var (integer_type_node, "choice"));
  service = keep (build_fn_decl ("status_service", build_function_type_list (
    uint32_type_node, ptr_type_node, ptr_type_node, NULL_TREE)));
  status_var = keep (create_tmp_var (uint32_type_node, "reused_status"));
  value = keep (create_tmp_var (integer_type_node, "result"));
  binding = keep (create_tmp_var (integer_type_node, "binding"));
  consumer = keep (create_tmp_var (integer_type_node, "consume"));
  TREE_ADDRESSABLE (value) = TREE_ADDRESSABLE (binding) = 1;
  for (unsigned variant = 0; variant < 8; ++variant)
    {
      gimple *c = invoke (status_var, value, binding);
      tree yes = label (), no = label ();
      gimple_seq body = NULL;
      gimple_seq_add_stmt (&body, c);
      if (variant == 1) gimple_seq_add_stmt (&body, use (value));
      if (variant == 7) gimple_seq_add_stmt (&body, use (binding));
      if (variant == 6) gimple_seq_add_stmt (&body, gimple_build_assign (consumer, integer_zero_node));
      if (variant != 2) gimple_seq_add_stmt (&body, check (status_var, yes, no, variant == 3 ? NE_EXPR : EQ_EXPR));
      gimple_seq_add_stmt (&body, lab (no));
      if (variant != 2) gimple_seq_add_stmt (&body, ret (-1));
      gimple_seq_add_stmt (&body, lab (yes));
      if (variant == 4) gimple_seq_add_stmt (&body, gimple_build_assign (value, integer_one_node));
      if (variant == 5) gimple_seq_add_stmt (&body, gimple_build_assign (binding, integer_one_node));
      gimple_seq_add_stmt (&body, use (variant == 5 ? binding : value));
      gimple_seq_add_stmt (&body, use (binding));
      gimple_seq_add_stmt (&body, ret (0));
      prove (fn, body, {{c, value, binding}}, variant == 0 || variant == 6);
    }
  for (unsigned variant = 0; variant < 6; ++variant)
    {
      tree copy = keep (create_tmp_var (variant == 4 ? unsigned_char_type_node : uint32_type_node, "status_copy"));
      tree yes = label (), no = label ();
      gimple *c = invoke (status_var, value);
      gimple_seq body = seq ({ c, variant == 4 ? gimple_build_assign (copy, NOP_EXPR, status_var)
                                            : gimple_build_assign (copy, status_var) });
      if (variant == 1) gimple_seq_add_stmt (&body, gimple_build_assign (status_var, build_int_cst (uint32_type_node, 2)));
      if (variant == 2) gimple_seq_add_stmt (&body, gimple_build_assign (copy, build_zero_cst (TREE_TYPE (copy))));
      if (variant == 3) gimple_seq_add_stmt (&body, gimple_build_assign (status_var, build_zero_cst (uint32_type_node)));
      gimple_seq_add_stmt (&body, check (variant == 5 ? status_var : copy, yes, no));
      gimple_seq_add_stmt (&body, lab (no)); gimple_seq_add_stmt (&body, ret (-1));
      gimple_seq_add_stmt (&body, lab (yes)); gimple_seq_add_stmt (&body, use (value));
      prove (fn, body, {{c, value, NULL_TREE}}, variant != 2 && variant != 4);
    }
  for (unsigned variant = 0; variant < 5; ++variant)
    {
      tree old = keep (create_tmp_var (uint32_type_node, "old_status"));
      tree yes = label (), no = label (), yes2 = label (), no2 = label ();
      gimple *a = invoke (status_var), *b = invoke (status_var, value, binding);
      gimple_seq body = seq ({ a, check (status_var, yes, no), lab (no), ret (-1), lab (yes),
        gimple_build_assign (old, status_var), b });
      if (variant != 1) gimple_seq_add_stmt (&body, check (variant == 0 ? old : status_var, yes2, no2));
      gimple_seq_add_stmt (&body, lab (no2));
      if (variant != 1) gimple_seq_add_stmt (&body, ret (-1));
      gimple_seq_add_stmt (&body, lab (yes2));
      if (variant != 3) gimple_seq_add_stmt (&body, use (variant == 4 ? binding : value));
      gimple_seq_add_stmt (&body, ret (0));
      prove (fn, body, {{a, NULL_TREE, NULL_TREE}, {b, value, binding}}, variant >= 2);
    }
  for (unsigned variant = 0; variant < 4; ++variant)
    {
      tree flag = keep (create_tmp_var (boolean_type_node, "ok"));
      tree yes = label (), no = label ();
      gimple *c = invoke (status_var, value);
      gimple_seq body = seq ({ c, gimple_build_assign (flag, variant == 1 ? NE_EXPR : EQ_EXPR,
        status_var, build_zero_cst (uint32_type_node)) });
      if (variant == 2) gimple_seq_add_stmt (&body, gimple_build_assign (flag, boolean_true_node));
      if (variant == 3) gimple_seq_add_stmt (&body, gimple_build_assign (status_var, build_zero_cst (uint32_type_node)));
      gimple_seq_add_stmt (&body, check (flag, yes, no, NE_EXPR));
      gimple_seq_add_stmt (&body, lab (no)); gimple_seq_add_stmt (&body, ret (-1));
      gimple_seq_add_stmt (&body, lab (yes)); gimple_seq_add_stmt (&body, use (value));
      prove (fn, body, {{c, value, NULL_TREE}}, variant == 0 || variant == 3);
    }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      tree left = label (), right = label (), done = label (), yes = label (), no = label ();
      gimple *c = invoke (status_var, value);
      gimple_seq body = seq ({ c, branch (left, right), lab (left), check (status_var, yes, no),
        lab (no), ret (-1), lab (yes), jump (done), lab (right) });
      if (variant != 1)
        {
          tree ry = label (), rn = label ();
          gimple_seq_add_stmt (&body, check (status_var, ry, rn));
          gimple_seq_add_stmt (&body, lab (rn)); gimple_seq_add_stmt (&body, ret (-1));
          gimple_seq_add_stmt (&body, lab (ry));
        }
      if (variant == 2) gimple_seq_add_stmt (&body, gimple_build_assign (value, integer_zero_node));
      gimple_seq_add_stmt (&body, lab (done)); gimple_seq_add_stmt (&body, use (value));
      prove (fn, body, {{c, value, NULL_TREE}}, variant == 0);
    }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      tree head = label (), yes = label (), no = label (), done = label (), again = label ();
      gimple *c = invoke (status_var, value);
      gimple_seq body = seq ({ lab (head), c });
      if (variant != 1) gimple_seq_add_stmt (&body, check (status_var, yes, no));
      gimple_seq_add_stmt (&body, lab (no));
      if (variant != 1) gimple_seq_add_stmt (&body, ret (-1));
      gimple_seq_add_stmt (&body, lab (yes)); gimple_seq_add_stmt (&body, use (value));
      gimple_seq_add_stmt (&body, branch (again, done)); gimple_seq_add_stmt (&body, lab (again));
      if (variant == 2) gimple_seq_add_stmt (&body, gimple_build_assign (value, integer_one_node));
      gimple_seq_add_stmt (&body, jump (head)); gimple_seq_add_stmt (&body, lab (done));
      prove (fn, body, {{c, value, NULL_TREE}}, variant != 1);
    }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      tree yes = label (), no = label ();
      tree other = keep (create_tmp_var (integer_type_node, "other"));
      gimple *c = invoke (status_var, value);
      gimple_seq cleanup = seq ({ gimple_build_assign (variant == 0 ? other : value, integer_one_node) });
      gimple_seq body = seq ({ c, check (status_var, yes, no), lab (no), ret (-1), lab (yes),
        finally (seq ({ use (value) }), cleanup) });
      if (variant != 2) gimple_seq_add_stmt (&body, use (value));
      prove (fn, body, {{c, value, NULL_TREE}}, variant != 1);
    }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      tree p = keep (create_tmp_var (build_pointer_type (integer_type_node), "alias"));
      tree yes = label (), no = label ();
      gimple *c = invoke (status_var, value);
      gimple_seq body = seq ({ c, check (status_var, yes, no), lab (no), ret (-1), lab (yes) });
      gimple_seq_add_stmt (&body, gimple_build_assign (p, build_fold_addr_expr (value)));
      if (variant == 1) gimple_seq_add_stmt (&body, gimple_build_assign (p, p));
      if (variant < 2) gimple_seq_add_stmt (&body, gimple_build_assign (build_simple_mem_ref (p), integer_one_node));
      gimple_seq_add_stmt (&body, use (value));
      prove (fn, body, {{c, value, NULL_TREE}}, variant == 2);
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      tree left = label (), right = label (), done = label (), yes = label (), no = label ();
      gimple *c = invoke (status_var, value);
      gimple_seq body = seq ({ branch (left, right), lab (left), c, check (status_var, yes, no),
        lab (no), ret (-1), lab (yes), jump (done), lab (right) });
      if (!variant) gimple_seq_add_stmt (&body, gimple_build_assign (value, integer_one_node));
      gimple_seq_add_stmt (&body, lab (done)); gimple_seq_add_stmt (&body, use (value));
      prove (fn, body, {{c, value, NULL_TREE}}, !variant);
    }
  {
    tree yes = label (), no = label ();
    gimple *c = invoke (status_var, value), *r = gimple_build_nop ();
    prove (fn, seq ({ c, check (status_var, yes, no), lab (no), ret (-1), lab (yes), r, use (value) }),
      {{c, value, NULL_TREE}}, false, {r});
  }
  {
    tree yes = label (), no = label ();
    gimple *c = invoke (status_var);
    prove (fn, seq ({ c, check (status_var, yes, no), lab (no), ret (-1), lab (yes), ret (0) }),
      {{c, NULL_TREE, NULL_TREE}}, true);
  }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      tree yes = label (), no = label ();
      tree other = keep (create_tmp_var (integer_type_node, "unrelated"));
      tree p = keep (create_tmp_var (ptr_type_node, "unknown_pointer"));
      gimple *c = invoke (status_var, value);
      gcall *unknown = gimple_build_call (service, 2,
        variant == 0 ? build_fold_addr_expr (value) : variant == 1 ? p : build_fold_addr_expr (other), null_pointer_node);
      prove (fn, seq ({ c, check (status_var, yes, no), lab (no), ret (-1), lab (yes), unknown, use (value) }),
        {{c, value, NULL_TREE}}, variant == 2);
    }
  {
    tree yes = label (), no = label ();
    gimple *c = invoke (status_var, value);
    prove (fn, seq ({ check (integer_one_node, yes, no), lab (yes), c, use (value),
      lab (no), ret (-1) }), {{c, value, NULL_TREE}}, true);
  }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      tree head = label (), shared = label (), local = label (), done = label (), yes = label (), no = label ();
      tree again = label (), exit = label ();
      gimple *c = invoke (status_var, value), *store = gimple_build_assign (value, integer_one_node);
      gimple_seq body = seq ({ lab (head), branch (shared, local), lab (shared), c, check (status_var, yes, no),
        lab (no), ret (-1), lab (yes), jump (done), lab (local), store, lab (done), use (value),
        branch (again, exit), lab (again), jump (head), lab (exit), ret (0) });
      if (variant) prove (fn, body, {{c, value, NULL_TREE}}, true, {}, {{c, store}});
      else prove (fn, body, {{c, value, NULL_TREE}}, false);
    }
  {
    tree yes = label (), no = label ();
    tree later = keep (create_tmp_var (uint32_type_node, "later_status"));
    gimple *a = invoke (status_var, value), *b = invoke (later);
    prove (fn, seq ({ a, b, check (later, yes, no), lab (no), ret (-1), lab (yes), use (value) }),
      {{a, value, NULL_TREE}, {b, NULL_TREE, NULL_TREE}}, false);
  }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      gimple *c = invoke (status_var);
      tree out = keep (create_tmp_var (uint32_type_node, "return_status"));
      prove (fn, seq ({ c, gimple_build_assign (out, variant ? status_var : build_zero_cst (uint32_type_node)),
        gimple_build_return (out) }), {{c, NULL_TREE, NULL_TREE}}, variant);
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      tree yes = label (), no = label ();
      gimple *c = invoke (status_var, value);
      gimple_seq body = seq ({ gimple_build_assign (value, integer_one_node) });
      if (variant) gimple_seq_add_stmt (&body,
        gimple_build_call (service, 2, build_fold_addr_expr (value), null_pointer_node));
      gimple_seq_add_stmt (&body, c); gimple_seq_add_stmt (&body, check (status_var, yes, no));
      gimple_seq_add_stmt (&body, lab (no)); gimple_seq_add_stmt (&body, ret (-1));
      gimple_seq_add_stmt (&body, lab (yes));
      gimple_seq_add_stmt (&body, gimple_build_call (service, 2, null_pointer_node, null_pointer_node));
      gimple_seq_add_stmt (&body, use (value));
      prove (fn, body, {{c, value, NULL_TREE}}, !variant);
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      tree yes = label (), no = label ();
      tree slot = keep (create_tmp_var (newtm_abi_context_type (), "test_slot"));
      tree context = build_zero_cst (newtm_abi_context_type ());
      tree other_status = keep (create_tmp_var (uint32_type_node, "host_status"));
      gimple *c = invoke (status_var, value);
      gimple_seq body = seq ({ c, check (status_var, yes, no), lab (no), ret (-1), lab (yes) });
      if (variant)
        {
          tree args[] = { context, build_fold_addr_expr (value), size_int (sizeof (int)), build_zero_cst (uint32_type_node) };
          gimple_seq_add_stmt (&body, newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER, other_status, 4, args));
        }
      tree args[] = { build_fold_addr_expr (slot) };
      gimple_seq_add_stmt (&body, newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_END, other_status, 1, args));
      gimple_seq_add_stmt (&body, use (value));
      prove (fn, body, {{c, value, NULL_TREE}}, !variant);
    }
  /* Exercise the complete production inspector, not only the dataflow API. */
  for (unsigned variant = 0; variant < 8; ++variant)
    {
      tree ctx[] = { build_zero_cst (newtm_abi_context_type ()) };
      gimple *a = newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS, status_var, 1, ctx);
      tree yes = label (), no = label (), old = keep (create_tmp_var (uint32_type_node, "saved_status"));
      gimple_seq body = seq ({ a });
      if (variant == 3 || variant == 7)
        {
          tree y = label (), n = label ();
          gimple_seq_add_stmt (&body, check (status_var, y, n));
          gimple_seq_add_stmt (&body, lab (n)); gimple_seq_add_stmt (&body, ret (-1));
          gimple_seq_add_stmt (&body, lab (y)); gimple_seq_add_stmt (&body, gimple_build_assign (old, status_var));
          gimple_seq_add_stmt (&body, newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS, status_var, 1, ctx));
        }
      if (variant == 5) gimple_seq_add_stmt (&body, gimple_build_assign (consumer, integer_one_node));
      if (variant == 6)
        {
          gimple_seq_add_stmt (&body, gimple_build_assign (old, status_var));
          gimple_seq_add_stmt (&body, gimple_build_assign (status_var, build_zero_cst (uint32_type_node)));
        }
      if (variant != 1 && variant != 4)
        {
          gimple_seq_add_stmt (&body, check (variant == 3 || variant == 6 ? old : status_var, yes, no,
                                             variant == 2 ? NE_EXPR : EQ_EXPR));
          gimple_seq_add_stmt (&body, lab (no)); gimple_seq_add_stmt (&body, ret (-1));
          gimple_seq_add_stmt (&body, lab (yes));
        }
      if (variant == 4)
        {
          gimple_seq_add_stmt (&body, gimple_build_assign (old, build_zero_cst (uint32_type_node)));
          gimple_seq_add_stmt (&body, gimple_build_return (old));
        }
      else gimple_seq_add_stmt (&body, ret (0));
      sample s (fn, body);
      const char *why = NULL;
      bool actual = newtm_obligation_verify (body, &why);
      bool expected = variant == 0 || variant >= 5;
      if (actual != expected)
        internal_error ("production status case %u: expected %u, observed %u: %s", variant, expected,
                        actual, why ? why : "proved");
      expect (expected ? !why : why && strstr (why, "V05"));
      ++status_cases;
    }
}
namespace {
const pass_data status_data = { GIMPLE_PASS, "newtm_status_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct status_probe : gimple_opt_pass
{
  status_probe (gcc::context *c) : gimple_opt_pass (status_data, c) {}
  unsigned execute (function *fn) override
  {
    if (newtm_clone_original (fn->decl)) expect (newtm_obligation_inspections (fn->decl));
    production += newtm_obligation_flow_nodes (fn->decl);
    if (!done) { done = true; synthetic (fn->decl); }
    return 0;
  }
};
}
static void finish (void *, void *)
{
  expect (done && status_cases >= 60 && (unit_only || production));
  fprintf (stderr, "STATUS scenarios=%u assertions=%u production_nodes=%u\n", status_cases, assertions, production);
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i) if (!strcmp (args->argv[i].key, "unit")) unit_only = true;
  register_pass_info info = { new status_probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &info);
  register_callback (args->base_name, PLUGIN_GGC_MARKING, mark, NULL);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
