/* Mutated IR is proved in a temporary window, never executed. */
#include "flow_probe_common.h"
#include "stringpool.h"
#include "newtm-failure.h"
#include "newtm-abi.h"
static unsigned cases, full_cases;
static bool unit_only, full_done;
static tree ctx, status_var, cause, effective, mock;
static tree number (unsigned n) { return build_int_cst (uint32_type_node, n); }
static gimple *call (built_in_function service, tree result, tree context, tree why = NULL_TREE)
{
  tree args[] = { context, why };
  return newtm_abi_build_call (service, result, why ? 2 : 1, args);
}
static gimple *status (built_in_function op = BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS)
{ return call (op, status_var, ctx); }
static gimple *fail (tree why)
{ return call (BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL, effective, ctx, why); }
static gimple *check (tree yes, tree no)
{ return gimple_build_cond (EQ_EXPR, status_var, number (0), yes, no); }
static gimple *result (tree v) { return gimple_build_return (v); }
static void prove (tree fn, gimple_seq body, bool expected, gimple *guard = NULL,
                   bool complete = false, bool missing = false)
{
  sample s (fn, body);
  const char *why = NULL; gimple *bad = NULL; bool actual;
  if (complete)
    {
      if (guard) newtm_obligation_abort (guard, gimple_cond_false_label (as_a<gcond *> (guard)));
      if (missing) newtm_obligation_forget (guard);
      actual = newtm_obligation_verify (body, &why, &bad);
      ++full_cases;
    }
  else
    {
      newtm_failure_analysis proof (*s.graph, ctx);
      if (guard) proof.invalid (guard);
      ggc_collect (GGC_COLLECT_FORCE);
      actual = proof.verify (&why, &bad);
    }
  if (actual != expected)
    internal_error ("failure probe %u full %u: expected %u observed %u: %s",
                    cases, full_cases, expected, actual, why ? why : "proved");
  if (!(expected ? !why : why && (strstr (why, "V07")
        || (complete && strstr (why, "V05")) || (missing && strstr (why, "V01")))))
    internal_error ("failure probe refusal %u full %u: %s", cases, full_cases, why ? why : "none");
  ++cases;
}
static gimple_seq checked (gimple *operation, unsigned variant)
{
  tree yes = label (), no = label ();
  gimple_seq body = seq ({ operation, check (yes, no), lab (no) });
  if (variant == 7) gimple_seq_add_stmt (&body, status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END));
  if (variant == 8) gimple_seq_add_stmt (&body, status ());
  if (variant == 9)
    { tree loop = label (); gimple_seq_add_stmt (&body, lab (loop)); gimple_seq_add_stmt (&body, jump (loop)); }
  gimple_seq_add_stmt (&body, gimple_build_assign (cause, variant == 1 ? number (1)
    : variant == 2 ? number (2) : status_var));
  if (variant != 3) gimple_seq_add_stmt (&body, fail (cause));
  if (variant == 6) gimple_seq_add_stmt (&body, gimple_build_assign (effective, number (2)));
  gimple_seq_add_stmt (&body, result (variant == 3 || variant == 4 ? cause : variant == 5 ? number (0) : effective));
  gimple_seq_add_stmt (&body, lab (yes)); gimple_seq_add_stmt (&body, result (number (0)));
  return body;
}
static void synthetic (tree fn)
{
  x = keep (create_tmp_var (integer_type_node, "choice"));
  ctx = keep (build_decl (UNKNOWN_LOCATION, PARM_DECL, get_identifier ("borrowed"), newtm_abi_context_type ()));
  status_var = keep (create_tmp_var (uint32_type_node, "status"));
  cause = keep (create_tmp_var (uint32_type_node, "cause"));
  effective = keep (create_tmp_var (uint32_type_node, "effective"));
  mock = keep (build_fn_decl ("classified_prepared", build_function_type_list (uint32_type_node, newtm_abi_context_type (), NULL_TREE)));
  for (unsigned v = 0; v < 10; ++v) prove (fn, checked (status (), v), v == 0);
  for (unsigned v = 0; v < 4; ++v)
    {
      gcall *c = gimple_build_call (mock, 1, ctx); gimple_call_set_lhs (c, status_var);
      prove (fn, checked (c, v), v == 0);
    }
  for (unsigned n : {0u, 1u, 2u, 99u})
    for (unsigned raw = 0; raw < 2; ++raw)
      prove (fn, seq ({ fail (number (n)), result (raw ? number (n) : effective) }), !raw && n != 1);
  for (unsigned v = 0; v < 5; ++v)
    {
      tree yes = label (), no = label ();
      gimple *guard = branch (yes, no);
      prove (fn, seq ({ guard, lab (no), fail (number (v == 1 ? 1 : v == 2 ? 0 : 2)),
        result (v == 3 ? number (0) : effective), lab (yes), result (number (0)) }),
        v != 1 && v != 3, guard);
    }
  /* Each successful entry opens one relative level; failure abandons all. */
  for (unsigned v = 0; v < 5; ++v)
    {
      tree entered = label (), closed = label (), bad = label ();
      gimple_seq body = seq ({ status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN),
        check (entered, bad), lab (entered) });
      if (v != 1)
        { gimple_seq_add_stmt (&body, status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END));
          gimple_seq_add_stmt (&body, check (closed, bad)); gimple_seq_add_stmt (&body, lab (closed)); }
      if (v == 2) gimple_seq_add_stmt (&body, status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END));
      if (v == 3) gimple_seq_add_stmt (&body, status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN));
      gimple_seq_add_stmt (&body, result (number (0)));
      gimple_seq_add_stmt (&body, lab (bad));
      if (v == 4) gimple_seq_add_stmt (&body, status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END));
      gimple_seq_add_stmt (&body, fail (status_var)); gimple_seq_add_stmt (&body, result (effective));
      prove (fn, body, v == 0);
    }
  prove (fn, checked (status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END), 0), false);
  for (unsigned v = 0; v < 3; ++v)
    {
      tree head = label (), entered = label (), bad = label (), again = label (), done_label = label ();
      gimple_seq body = seq ({ lab (head), status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN),
        check (entered, bad), lab (entered) });
      if (v != 1)
        { tree closed = label (); gimple_seq_add_stmt (&body, status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END));
          gimple_seq_add_stmt (&body, check (closed, bad)); gimple_seq_add_stmt (&body, lab (closed)); }
      gimple_seq_add_stmt (&body, branch (again, done_label)); gimple_seq_add_stmt (&body, lab (again));
      gimple_seq_add_stmt (&body, jump (head)); gimple_seq_add_stmt (&body, lab (done_label));
      gimple_seq_add_stmt (&body, result (number (0))); gimple_seq_add_stmt (&body, lab (bad));
      if (v == 2)
        { tree spin = label (); gimple_seq_add_stmt (&body, lab (spin)); gimple_seq_add_stmt (&body, jump (spin)); }
      gimple_seq_add_stmt (&body, fail (status_var)); gimple_seq_add_stmt (&body, result (effective));
      prove (fn, body, v == 0);
    }
  for (unsigned v = 0; v < 4; ++v)
    {
      tree copy = keep (create_tmp_var (newtm_abi_context_type (), "context_copy"));
      tree other = keep (create_tmp_var (newtm_abi_context_type (), "other_context"));
      tree yes = label (), no = label ();
      gimple_seq body = seq ({ gimple_build_assign (copy, ctx) });
      if (v == 1) gimple_seq_add_stmt (&body, gimple_build_assign (ctx, other));
      if (v == 2) gimple_seq_add_stmt (&body, gimple_build_assign (copy, other));
      if (v == 3)
        { tree p = keep (create_tmp_var (build_pointer_type (TREE_TYPE (ctx)), "escape"));
          gimple_seq_add_stmt (&body, gimple_build_assign (p, build_fold_addr_expr (ctx))); }
      gimple_seq_add_stmt (&body, call (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS, status_var, copy));
      gimple_seq_add_stmt (&body, check (yes, no)); gimple_seq_add_stmt (&body, lab (no));
      gimple_seq_add_stmt (&body, fail (status_var)); gimple_seq_add_stmt (&body, result (effective));
      gimple_seq_add_stmt (&body, lab (yes)); gimple_seq_add_stmt (&body, result (number (0)));
      prove (fn, body, v == 0);
    }
  for (unsigned v = 0; v < 3; ++v)
    prove (fn, seq ({ fail (number (2)), finally (seq ({ result (v == 2 ? number (2) : effective) }),
      seq ({ gimple_build_assign (v == 1 ? effective : cause, number (0)) })) }), v != 2);
  /* A raw cause captured by RETURN cannot be repaired in its cleanup. */
  prove (fn, seq ({ finally (seq ({ result (number (2)) }), seq ({ fail (number (2)) })) }), false);
  for (built_in_function op : { BUILT_IN_GOMP_TRANSACTION_STM_V1_END,
         BUILT_IN_GOMP_TRANSACTION_STM_V1_DISCARD, BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE,
         BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL })
    prove (fn, seq ({ call (op, op == BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL ? NULL_TREE : status_var,
      ctx, op == BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE ? number (2) : NULL_TREE),
      result (number (0)) }), false);
  /* Distinct call definitions may join only through a preserved cause. */
  for (unsigned v = 0; v < 2; ++v)
    {
      tree left = label (), right = label (), good = label (), bad = label (), join = label ();
      tree a = keep (create_tmp_var (uint32_type_node, "left_status"));
      tree b = keep (create_tmp_var (uint32_type_node, "right_status"));
      tree args[] = { ctx };
      prove (fn, seq ({ branch (left, right), lab (left),
        newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS, a, 1, args),
        gimple_build_cond (EQ_EXPR, a, number (0), good, bad), lab (bad),
        gimple_build_assign (cause, a), jump (join), lab (right),
        newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS, b, 1, args),
        gimple_build_cond (EQ_EXPR, b, number (0), good, join), lab (join),
        fail (v ? a : cause), result (effective), lab (good), result (number (0)) }), false);
      /* Define the common carrier on both failing predecessors. */
      tree l = label (), r = label (), ok = label (), al = label (), bl = label (), end = label ();
      prove (fn, seq ({ branch (l, r), lab (l),
        newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS, a, 1, args),
        gimple_build_cond (EQ_EXPR, a, number (0), ok, al), lab (al),
        gimple_build_assign (cause, a), jump (end), lab (r),
        newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS, b, 1, args),
        gimple_build_cond (EQ_EXPR, b, number (0), ok, bl), lab (bl),
        gimple_build_assign (cause, b), lab (end), fail (v ? a : cause), result (effective),
        lab (ok), result (number (0)) }), !v);
    }
  for (unsigned v = 0; v < 2; ++v)
    {
      tree left = label (), right = label (), entered = label (), join = label (), bad = label ();
      gimple_seq body = seq ({ branch (left, right), lab (left),
        status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN), check (entered, bad),
        lab (entered), jump (join), lab (right), jump (join), lab (join) });
      if (v) gimple_seq_add_stmt (&body, status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END));
      gimple_seq_add_stmt (&body, result (number (0))); gimple_seq_add_stmt (&body, lab (bad));
      gimple_seq_add_stmt (&body, fail (status_var)); gimple_seq_add_stmt (&body, result (effective));
      prove (fn, body, false);
    }
  {
    tree bad = label (); gimple_seq body = NULL;
    for (unsigned i = 0; i < 4; ++i)
      { tree next = label ();
        gimple_seq_add_stmt (&body, status (i < 2 ? BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN
                                                : BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END));
        gimple_seq_add_stmt (&body, check (next, bad)); gimple_seq_add_stmt (&body, lab (next)); }
    gimple_seq_add_stmt (&body, result (number (0))); gimple_seq_add_stmt (&body, lab (bad));
    gimple_seq_add_stmt (&body, fail (status_var)); gimple_seq_add_stmt (&body, result (effective));
    prove (fn, body, true);
  }
  {
    tree spin = label ();
    prove (fn, seq ({ fail (number (2)), lab (spin), jump (spin) }), false);
  }
}
static void complete (tree fn)
{
  ctx = newtm_clone_context (fn);
  for (unsigned v : {0u, 1u, 2u, 3u, 4u, 5u, 6u})
    prove (fn, checked (status (), v), v == 0, NULL, true);
  prove (fn, seq ({ fail (number (2)), result (effective) }), true, NULL, true);
  prove (fn, checked (status (BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_END), 0), false, NULL, true);
  for (unsigned v = 0; v < 3; ++v)
    {
      tree yes = label (), no = label ();
      gimple *guard = branch (yes, no);
      gimple_seq body = v == 2 ? seq ({ result (number (0)) })
        : seq ({ guard, lab (no), fail (number (v ? 1 : 2)), result (effective), lab (yes), result (number (0)) });
      prove (fn, body, !v, guard, true, v == 2);
    }
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_failure_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass
{
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (newtm_clone_original (fn->decl)) require (newtm_obligation_inspections (fn->decl));
    production += newtm_obligation_flow_nodes (fn->decl);
    if (!done) { done = true; synthetic (fn->decl); }
    if (!full_done && newtm_clone_context (fn->decl) && !newtm_clone_channel (fn->decl, 3))
      { full_done = true; complete (fn->decl); }
    return 0;
  }
};
}
static void finish (void *, void *)
{
  require (done && cases >= 48 && (unit_only || production));
  fprintf (stderr, "FAILURES scenarios=%u assertions=%u production_nodes=%u full=%u\n", cases, assertions, production, full_cases);
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i) if (!strcmp (args->argv[i].key, "unit")) unit_only = true;
  register_pass_info info = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &info);
  register_callback (args->base_name, PLUGIN_GGC_MARKING, mark, NULL);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
