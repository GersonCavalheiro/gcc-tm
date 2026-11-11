/* Synthetic protocol mutations stay in temporary, nonexecuted IR windows. */
#include "flow_probe_common.h"
#include "newtm-owner.h"
#include "newtm-abi.h"
static tree slot, other, outer, count_var, status_var, ctx, saved, packet, scalar, alias, helper;
static bool unit_only, complete_done;
static unsigned cases, complete_cases;
static gimple_seq *active;
static tree num (unsigned n) { return build_int_cst (uint32_type_node, n); }
static gimple *set (tree t, tree v) { return gimple_build_assign (t, v); }
static gimple *service (built_in_function op, tree a, tree b = NULL_TREE)
{
  tree args[] = { a, b };
  for (unsigned i = 0; i < (b ? 2u : 1u); ++i)
    if (!is_gimple_val (args[i]))
      {
        tree t = keep (create_tmp_var (TREE_TYPE (args[i]), "loaded_argument"));
        gimple_seq_add_stmt (active, set (t, args[i])); args[i] = t;
      }
  return newtm_abi_build_call (op, op == BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL ? NULL_TREE : status_var,
                               b ? 2 : 1, args);
}
static gimple *condition (tree a, tree b, tree yes, tree no, tree_code code = EQ_EXPR)
{ return gimple_build_cond (code, a, b, yes, no); }
static gimple *check (tree yes, tree no) { return condition (status_var, num (0), yes, no); }
static gcall *invoke (bool complete)
{
  tree incoming = complete ? build_zero_cst (TREE_TYPE (DECL_CHAIN (DECL_ARGUMENTS (helper))))
    : build_fold_addr_expr (build4 (ARRAY_REF, integer_type_node, packet, size_zero_node, NULL_TREE, NULL_TREE));
  gcall *g = gimple_build_call (helper, 2, ctx, incoming); gimple_call_set_lhs (g, status_var); return g;
}
struct program { gimple_seq body; gimple *guard; };
static program body (unsigned v, bool complete)
{
  tree retry = label (), entered = label (), prep = label (), run = label (), body_ok = label ();
  tree checked = label (), commit_ok = label (), bad = label (), clean = label (), fatal = label ();
  tree inc = label (), saturated = label (), empty = label (), done_label = label (), invalid_label = label ();
  tree escaped = label ();
  program p = { NULL, NULL }; gimple_seq &s = p.body;
  active = &s;
  auto add = [&] (gimple *g) { gimple_seq_add_stmt (&s, g); };
  if (v != 1) add (set (slot, build_int_cst (TREE_TYPE (slot), v == 2 ? 1 : 0)));
  if (v != 3) add (set (count_var, size_zero_node));
  if (v == 45) add (set (scalar, integer_one_node));
  if (v == 50) { tree spin = label (); add (lab (spin)); add (jump (spin)); }
  add (lab (retry));
  if (v == 4) add (set (count_var, size_zero_node));
  if (v == 6) add (jump (prep));
  add (service (BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN,
    build_fold_addr_expr (v == 5 ? other : slot), build_fold_addr_expr (outer)));
  add (check (entered, bad)); add (lab (entered));
  if (v == 52) add (set (build_simple_mem_ref (build_fold_addr_expr (outer)), num (1)));
  if (v == 8) add (set (outer, num (1)));
  if (v != 7)
    {
      if (v == 47)
        {
          tree flag = keep (create_tmp_var (boolean_type_node, "outer_proof"));
          add (gimple_build_assign (flag, EQ_EXPR, outer, num (1)));
          p.guard = condition (flag, boolean_false_node, prep, invalid_label, NE_EXPR);
        }
      else p.guard = condition (outer, num (1), prep, invalid_label, v == 9 ? NE_EXPR : EQ_EXPR);
      add (p.guard);
    }
  else add (jump (prep));
  add (lab (invalid_label)); add (set (status_var, num (2))); add (jump (bad));
  add (lab (prep));
  if (v == 59) add (jump (escaped));
  if (v == 60) add (set (slot, build_clobber (TREE_TYPE (slot), CLOBBER_STORAGE_END)));
  if (v == 42) add (set (alias, build_fold_addr_expr (slot)));
  add (set (ctx, v == 12 ? build_zero_cst (TREE_TYPE (ctx)) : slot));
  if (v == 51) add (set (build_simple_mem_ref (build_fold_addr_expr (slot)), build_zero_cst (TREE_TYPE (slot))));
  if (v == 54) add (set (alias, build_fold_addr_expr (ctx)));
  if (v == 55)
    { tree address = keep (create_tmp_var (build_pointer_type (size_type_node), "escaped_counter"));
      add (set (address, build_fold_addr_expr (count_var))); }
  if (!complete)
    {
      if (v == 56)
        {
          vec<constructor_elt, va_gc> *elements = NULL;
          CONSTRUCTOR_APPEND_ELT (elements, size_zero_node, integer_one_node);
          add (set (packet, build_constructor (TREE_TYPE (packet), elements)));
        }
      else if (v == 44) add (branch (run, run));
      else add (set (build4 (ARRAY_REF, integer_type_node, packet, size_zero_node, NULL_TREE, NULL_TREE),
                     v == 45 ? scalar : integer_one_node));
    }
  add (lab (run));
  if (v == 43) add (set (build_simple_mem_ref (build_fold_addr_expr (other)), ctx));
  if (v != 10) { add (invoke (complete)); add (check (body_ok, bad)); }
  else add (jump (body_ok));
  add (lab (body_ok));
  if (v == 11) add (invoke (complete));
  if (v != 13)
    { add (service (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS,
                   v == 14 ? build_zero_cst (TREE_TYPE (ctx)) : ctx)); add (check (checked, bad)); }
  else add (jump (checked));
  add (lab (checked));
  if (v == 17) add (jump (done_label));
  add (service (BUILT_IN_GOMP_TRANSACTION_STM_V1_END, build_fold_addr_expr (v == 16 ? other : slot)));
  if (v != 18) add (check (commit_ok, bad));
  add (lab (commit_ok));
  if (v == 41) add (set (other, ctx));
  if (v == 57) add (condition (ctx, build_zero_cst (TREE_TYPE (ctx)), done_label, fatal));
  add (jump (done_label));
  add (lab (bad));
  if (v == 40) { tree spin = label (); add (lab (spin)); add (jump (spin)); }
  if (v == 37) add (jump (fatal));
  add (condition (slot, build_zero_cst (TREE_TYPE (slot)), v == 38 ? clean : fatal, clean));
  add (lab (clean));
  if (v == 46) add (set (saved, status_var));
  if (v != 19)
    add (service (BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL, slot,
      v == 20 ? num (1) : v == 21 ? num (2) : v == 46 ? saved : status_var));
  if (v == 22) add (service (BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL, slot, status_var));
  if (v == 33) add (set (saved, status_var));
  if (v == 26) add (jump (inc));
  add (condition (count_var, TYPE_MAX_VALUE (size_type_node), inc, saturated,
                 v == 29 ? LE_EXPR : v == 48 ? NE_EXPR : LT_EXPR));
  add (lab (inc));
  add (gimple_build_assign (count_var, PLUS_EXPR, count_var, build_int_cst (size_type_node, v == 30 ? 2 : 1)));
  if (v == 27) add (gimple_build_assign (count_var, PLUS_EXPR, count_var, size_one_node));
  add (lab (saturated));
  if (v == 28) add (set (count_var, size_zero_node));
  if (v == 36) add (set (slot, build_zero_cst (TREE_TYPE (slot))));
  if (v == 53) add (set (build_simple_mem_ref (build_fold_addr_expr (count_var)), size_zero_node));
  if (v != 23)
    add (service (BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE, build_fold_addr_expr (v == 25 ? other : slot),
                  v == 31 ? size_one_node : count_var));
  if (v == 24) add (service (BUILT_IN_GOMP_TRANSACTION_STM_V1_AFTER_FAILURE, build_fold_addr_expr (slot), count_var));
  if (v == 39) add (jump (done_label));
  if (v == 49) add (set (saved, status_var));
  if (v != 32) add (condition (v == 33 || v == 49 ? saved : status_var, num (v == 34 ? 2 : 1), empty, fatal));
  else add (jump (empty));
  add (lab (empty));
  add (condition (slot, build_zero_cst (TREE_TYPE (slot)), v == 35 ? prep : retry, fatal));
  add (lab (fatal)); add (service (BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL, status_var));
  add (lab (done_label)); add (gimple_build_nop ());
  if (v == 58 || v == 59 || v == 61)
    {
      DECL_CHAIN (slot) = v == 61 ? NULL_TREE : outer;
      DECL_CHAIN (outer) = count_var; DECL_CHAIN (count_var) = NULL_TREE;
      gbind *scope = gimple_build_bind (slot, s, NULL_TREE);
      s = seq ({ scope });
      if (v == 59) { add (lab (escaped)); add (gimple_build_nop ()); }
    }
  return p;
}
static void initialize ()
{
  x = keep (create_tmp_var (integer_type_node, "choice"));
  slot = keep (create_tmp_var (newtm_abi_context_type (), "slot"));
  other = keep (create_tmp_var (TREE_TYPE (slot), "copy"));
  ctx = keep (create_tmp_var (TREE_TYPE (slot), "borrow"));
  outer = keep (create_tmp_var (uint32_type_node, "outer"));
  status_var = keep (create_tmp_var (uint32_type_node, "status"));
  saved = keep (create_tmp_var (uint32_type_node, "saved"));
  count_var = keep (create_tmp_var (size_type_node, "conflicts"));
  scalar = keep (create_tmp_var (integer_type_node, "stale_definition"));
  packet = keep (create_tmp_var (build_array_type_nelts (integer_type_node, 1), "packet"));
  alias = keep (create_tmp_var (build_pointer_type (TREE_TYPE (slot)), "slot_escape"));
  TREE_ADDRESSABLE (slot) = TREE_ADDRESSABLE (other) = TREE_ADDRESSABLE (outer) = TREE_ADDRESSABLE (packet) = 1;
  helper = keep (build_fn_decl ("classified_helper", build_function_type_list (uint32_type_node,
    newtm_abi_context_type (), build_pointer_type (integer_type_node), NULL_TREE)));
}
static bool expected (unsigned v) { return v == 0 || v == 46 || v == 47 || v == 48 || v == 49 || v == 56 || v == 58; }
static void prove (tree fn, unsigned v, bool complete)
{
  program p = body (v, complete);
  sample s (fn, p.body);
  const char *why = NULL; gimple *bad = NULL; bool actual;
  if (complete)
    {
      /* Freshly reconstruct the synthetic window; no descriptors or copied
         approval are consulted. Positive full inspection is the real emission. */
      delete s.graph; s.graph = NULL; newtm_obligation_end ();
      gcall *source = gimple_build_call (helper, 2, build_zero_cst (TREE_TYPE (ctx)),
        build_zero_cst (TREE_TYPE (DECL_CHAIN (DECL_ARGUMENTS (helper)))));
      require (newtm_obligation_begin (fn, NULL, NEWTM_OB_CONTROLLER, source));
      newtm_obligation_declare_all (source, NEWTM_OB_DELEGATED, helper);
      newtm_obligation_emitted (p.body, source);
      s.graph = new newtm_flow_graph; require (s.graph->build (p.body, &why, &bad));
      if (p.guard) newtm_obligation_abort (p.guard, gimple_cond_false_label (as_a<gcond *> (p.guard)));
      tree locals = NULL_TREE;
      for (tree t : { slot, other, outer, count_var, status_var, ctx, saved, packet, scalar, alias })
        { DECL_CHAIN (t) = locals; locals = t; }
      ggc_collect (GGC_COLLECT_FORCE);
      actual = newtm_obligation_verify (p.body, &why, &bad, locals);
      ++complete_cases;
    }
  else
    {
      newtm_owner_analysis proof (*s.graph, helper);
      if (p.guard) proof.invalid (p.guard);
      ggc_collect (GGC_COLLECT_FORCE); actual = proof.verify (&why, &bad);
    }
  if (actual != expected (v))
    internal_error ("owner case %u full %u: expected %u observed %u: %s", v, complete,
      expected (v), actual, why ? why : "proved");
  require (expected (v) ? !why : why && (strstr (why, "V07")
    || (complete && (strstr (why, "V05") || strstr (why, "V04") || strstr (why, "V06")))));
  if (v == 59) require (strstr (why, "lifetime"));
  ++cases;
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_owner_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass
{
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    production += newtm_obligation_flow_nodes (fn->decl);
    if (!done)
      { done = true; initialize (); for (unsigned v = 0; v <= 61; ++v) if (v != 15) prove (fn->decl, v, false); }
    if (!complete_done && newtm_clone_original (fn->decl) && !newtm_clone_region_owner (fn->decl)
        && list_length (DECL_ARGUMENTS (fn->decl)) == 2)
      {
        complete_done = true; helper = fn->decl;
        for (unsigned v : {7u, 8u, 10u, 11u, 12u, 13u, 16u, 17u, 19u, 20u, 21u, 22u,
                          23u, 24u, 25u, 26u, 27u, 28u, 32u, 33u, 35u, 36u, 37u, 39u, 40u, 41u, 42u})
          prove (newtm_clone_original (fn->decl), v, true);
      }
    return 0;
  }
};
}
static void finish (void *, void *)
{
  require (done && cases >= 50 && (unit_only || (production && complete_done)));
  fprintf (stderr, "OWNER scenarios=%u assertions=%u production_nodes=%u full=%u\n", cases, assertions, production, complete_cases);
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
