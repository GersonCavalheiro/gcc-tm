/* Local proof composition is inspected in fresh, nonexecuted IR windows. */
#include "flow_probe_common.h"
#include "newtm-abi.h"
static tree array, scalar, status_var, global;
static unsigned cases, approved, refused, dependencies;
static bool unit_only, dependency_done, reported;
static const char *diagnostic;
static tree element () { return build4 (ARRAY_REF, integer_type_node, array, size_zero_node, NULL_TREE, NULL_TREE); }
static gimple *load () { return gimple_build_assign (scalar, element ()); }
static gimple_seq bound (gimple_seq body) { return seq ({ gimple_build_bind (array, body, NULL_TREE) }); }
static unsigned HOST_WIDE_INT record_for (gimple *g, newtm_obligation_kind kind)
{
  for (unsigned HOST_WIDE_INT i = newtm_obligation_count (); i; --i)
    {
      newtm_obligation_view v; require (newtm_obligation_get (i, &v));
      if (v.kind == kind && newtm_obligation_has_anchor (i)) return i;
    }
  internal_error ("composition probe has no obligation for %s", gimple_code_name[gimple_code (g)]);
}
static newtm_local_view prove (gimple_seq body, bool expected, unsigned rule = 0)
{
  const char *why = NULL; gimple *bad = NULL;
  bool actual = newtm_obligation_verify (body, &why, &bad);
  if (actual != expected) internal_error ("composition case %u expected %u observed %u: %s", cases, expected, actual, why ? why : "proved");
  newtm_local_view result;
  require (newtm_obligation_local_get (newtm_obligation_local_count (), &result));
  require (!result.closed && result.state == (expected ? NEWTM_LOCAL_APPROVED : NEWTM_LOCAL_REFUSED));
  if (!expected && result.rule != rule) internal_error ("composition case %u expected V0%u observed V0%u: %s", cases, rule, result.rule, result.reason);
  require (expected ? result.required == result.proved && !result.reason : !result.proved && result.reason && result.rule == rule);
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_count (); ++i)
    {
      newtm_obligation_view v; require (newtm_obligation_get (i, &v));
      if (v.scope != result.scope) continue;
      require (v.local_result == result.id && v.local_state == result.state && v.state != NEWTM_OB_COVERED);
      require (expected ? v.required == v.proved && v.generation == result.generation : !v.proved && v.reason);
    }
  if (diagnostic && !reported && !expected
      && ((!strcmp (diagnostic, "derived") && result.ancestor && result.ancestor != result.obligation)
          || (diagnostic[0] >= '1' && diagnostic[0] <= '8' && result.rule == unsigned (diagnostic[0] - '0'))))
    { reported = true; newtm_obligation_report (DECL_SOURCE_LOCATION (current_function_decl)); }
  ggc_collect (GGC_COLLECT_FORCE); ++cases;
  return result;
}
static void check_invalid (unsigned HOST_WIDE_INT id)
{
  newtm_local_view v; require (newtm_obligation_local_get (id, &v));
  require (v.state == NEWTM_LOCAL_INVALID && !v.proved);
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_count (); ++i)
    {
      newtm_obligation_view r; newtm_obligation_get (i, &r);
      if (r.scope == v.scope && r.local_result == id) require (r.local_state != NEWTM_LOCAL_APPROVED && !r.proved);
    }
}
static gimple_seq checked (gimple *call, bool valid = true)
{
  tree yes = label (), no = label ();
  gimple_seq body = seq ({ call });
  if (valid) gimple_seq_add_stmt (&body, gimple_build_cond (EQ_EXPR, status_var,
    build_zero_cst (uint32_type_node), yes, no));
  gimple_seq_add_stmt (&body, lab (no));
  if (valid) gimple_seq_add_stmt (&body, gimple_build_return (status_var));
  gimple_seq_add_stmt (&body, lab (yes)); gimple_seq_add_stmt (&body, ret (0));
  return body;
}
static gimple *status ()
{
  tree args[] = { build_zero_cst (newtm_abi_context_type ()) };
  return newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS, status_var, 1, args);
}
static void synthetic (tree fn)
{
  array = keep (create_tmp_var (build_array_type_nelts (integer_type_node, 2), "private_array"));
  TREE_ADDRESSABLE (array) = 1; DECL_CHAIN (array) = NULL_TREE;
  scalar = keep (create_tmp_var (integer_type_node, "value"));
  status_var = keep (create_tmp_var (uint32_type_node, "status"));
  global = keep (build_decl (UNKNOWN_LOCATION, VAR_DECL, NULL_TREE, integer_type_node));
  TREE_STATIC (global) = 1;
  for (unsigned n = 0; n < 2; ++n)
    {
      gimple *g = n ? gimple_build_assign (element (), integer_one_node) : load ();
      sample s (fn, bound (seq ({g})));
      unsigned HOST_WIDE_INT first = newtm_obligation_count ();
      newtm_obligation_view before; newtm_obligation_get (first, &before);
      require (before.local_state == NEWTM_LOCAL_UNVERIFIED && !before.proved && !before.local_result);
      newtm_local_view a = prove (s.body, true), b = prove (s.body, true);
      require (a.id != b.id && a.scope == b.scope && a.generation == b.generation); check_invalid (a.id);
      /* The new revision replaces every member's approval, not the old result. */
      newtm_obligation_view current; newtm_obligation_get (first, &current);
      require (current.local_state == NEWTM_LOCAL_APPROVED && current.local_result == b.id);
    }
  {
    sample s (fn, seq ({gimple_build_assign (scalar, global)}));
    newtm_local_view r = prove (s.body, false, 4); require (r.kind == NEWTM_OB_READ && r.obligation && r.owner == fn);
  }
  for (unsigned v = 0; v < 3; ++v)
    {
      gimple *g = load (); sample s (fn, bound (seq ({g})));
      unsigned HOST_WIDE_INT id = record_for (g, NEWTM_OB_READ);
      if (v == 2) prove (s.body, true);
      newtm_obligation_forget (g);
      gimple_seq actual = seq ({gimple_build_nop ()});
      if (v == 1) newtm_obligation_emitted (actual, g);
      newtm_local_view r = prove (actual, false, 1); require (r.obligation == id && r.kind == NEWTM_OB_READ);
    }
  for (unsigned v = 0; v < 2; ++v)
    {
      gimple *old = load (); sample s (fn, bound (seq ({old})));
      gimple *fresh = v ? gimple_build_assign (scalar, build4 (ARRAY_REF, integer_type_node, array, size_one_node, NULL_TREE, NULL_TREE)) : load ();
      gimple_seq actual = bound (seq ({fresh}));
      newtm_obligation_declare (old, NEWTM_OB_READ, NEWTM_OB_PRIVATE);
      newtm_obligation_emitted (actual, old); newtm_obligation_forget (old);
      prove (actual, !v, v ? 1 : 0);
    }
  {
    gimple *g = load (); sample s (fn, bound (seq ({g})));
    newtm_local_view a = prove (s.body, true);
    auto_vec<unsigned char> prior;
    for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_local_count (); ++i)
      { newtm_local_view r; require (newtm_obligation_local_get (i, &r)); prior.safe_push (r.graph_invalid); }
    gimple *copy = gimple_copy (g); (void) copy;
    for (unsigned i = 0; i < prior.length (); ++i)
      { newtm_local_view r; require (newtm_obligation_local_get (i + 1, &r));
        if (r.scope_kind != NEWTM_OB_COPY) require (r.graph_invalid == bool (prior[i])); }
    check_invalid (a.id);
    prove (s.body, false, 8);
  }
  {
    gimple *g = load (); sample s (fn, bound (seq ({g})));
    unsigned HOST_WIDE_INT id = record_for (g, NEWTM_OB_READ);
    newtm_local_view a = prove (s.body, true);
    newtm_obligation_invalidate (id); check_invalid (a.id);
    prove (s.body, false, 8);
  }
  for (unsigned v = 0; v < 2; ++v)
    {
      gimple *old = gimple_build_assign (element (), integer_one_node);
      sample s (fn, bound (seq ({old})));
      unsigned HOST_WIDE_INT parent = record_for (old, NEWTM_OB_WRITE);
      gimple *child = gimple_build_assign (gimple_assign_lhs (old), integer_one_node);
      newtm_obligation_derive_store (old, child);
      unsigned HOST_WIDE_INT id = record_for (child, NEWTM_OB_WRITE);
      gimple_seq actual = bound (seq ({child})); newtm_obligation_emitted (actual, old); newtm_obligation_forget (old);
      prove (actual, true);
      newtm_obligation_view p; newtm_obligation_get (parent, &p);
      require (p.state == NEWTM_OB_DERIVED && p.children == 1 && p.local_state == NEWTM_LOCAL_APPROVED);
      if (v)
        { newtm_obligation_invalidate (id); newtm_local_view r = prove (actual, false, 8);
          require (r.obligation == id && r.ancestor == parent); }
    }
  for (unsigned v = 0; v < 2; ++v)
    {
      sample s (fn, checked (status (), !v)); prove (s.body, !v, v ? 5 : 0);
    }
  /* A lifecycle label alone cannot replace BEGIN with an unrelated STATUS. */
  for (unsigned v = 0; v < 2; ++v)
    {
      gimple *source = gimple_build_call (builtin_decl_explicit (BUILT_IN_GOMP_TRANSACTION_BEGIN), 0);
      sample s (fn, seq ({source}));
      newtm_obligation_declare_all (source, NEWTM_OB_LIFECYCLE);
      tree args[] = { build_zero_cst (newtm_abi_context_type ()) };
      gimple *call = newtm_abi_build_call (v ? BUILT_IN_GOMP_TRANSACTION_STM_V1_STATUS
                                          : BUILT_IN_GOMP_TRANSACTION_STM_V1_NEST_BEGIN,
                                          status_var, 1, args);
      gimple_seq actual = checked (call);
      newtm_obligation_emitted (actual, source); newtm_obligation_forget (source);
      prove (actual, !v, v ? 1 : 0);
    }
  {
    gimple *g = load (); sample s (fn, bound (seq ({g})));
    newtm_local_view a = prove (s.body, true);
    tree old = gimple_assign_rhs1 (g); gimple_assign_set_rhs1 (g, global);
    prove (s.body, false, 8); check_invalid (a.id);
    gimple_assign_set_rhs1 (g, old);
    prove (s.body, false, 8); /* A failed window cannot silently recover. */
  }
  {
    sample s (fn, bound (seq ({load ()}))); prove (s.body, true);
  }
  {
    sample s (fn, seq ({gimple_build_nop ()})); newtm_obligation_reject ("deliberately refused preflight");
    const char *why = NULL; require (newtm_obligation_verify (s.body, &why) && !why);
    newtm_local_view r; newtm_obligation_local_get (newtm_obligation_local_count (), &r);
    require (r.state == NEWTM_LOCAL_REFUSED && !r.proved && r.reason); ++cases;
  }
  const char *why = NULL;
  require (!newtm_obligation_verify (NULL, &why) && strstr (why, "outside descriptor window"));
  newtm_local_view outside; newtm_obligation_local_get (newtm_obligation_local_count (), &outside);
  require (outside.closed && outside.state == NEWTM_LOCAL_REFUSED && outside.rule == 8); ++cases;
}
static void prepared_dependency (tree fn, tree callee)
{
  gcall *source = gimple_build_call (newtm_clone_original (callee), 0);
  sample s (fn, seq ({source}));
  newtm_obligation_declare_all (source, NEWTM_OB_PREPARED_CALL, callee);
  tree incoming = build_zero_cst (TREE_TYPE (DECL_CHAIN (DECL_ARGUMENTS (callee))));
  gcall *call = gimple_build_call (callee, 2, build_zero_cst (newtm_abi_context_type ()), incoming);
  gimple_call_set_lhs (call, status_var);
  gimple_seq actual = checked (call);
  newtm_obligation_emitted (actual, source); newtm_obligation_forget (source);
  newtm_local_view r = prove (actual, true);
  require (r.dependencies && TREE_VALUE (r.dependencies) == callee);
  unsigned HOST_WIDE_INT id = record_for (call, NEWTM_OB_CALL);
  newtm_obligation_view member; newtm_obligation_get (id, &member);
  require (member.dependencies && member.state == NEWTM_OB_PENDING);
  newtm_obligation_invalidate (id); check_invalid (r.id); prove (actual, false, 8);
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_composition_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass
{
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    production += newtm_obligation_flow_nodes (fn->decl);
    for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_local_count (); ++i)
      {
        newtm_local_view v; require (newtm_obligation_local_get (i, &v));
        if (v.scope_kind == NEWTM_OB_COPY) continue;
        require (v.closed);
        if (v.state == NEWTM_LOCAL_APPROVED) { require (v.required == v.proved && !v.reason); ++approved; dependencies += v.dependencies != NULL_TREE; }
        else if (v.state == NEWTM_LOCAL_REFUSED) { require (!v.proved && v.reason); ++refused; }
      }
    require (newtm_obligation_consistent (fn->decl));
    if (!done) { done = true; synthetic (fn->decl); }
    if (!dependency_done && newtm_clone_original (fn->decl) && !newtm_clone_region_owner (fn->decl)
        && list_length (DECL_ARGUMENTS (fn->decl)) == 2 && !newtm_clone_channel (fn->decl, 3))
      { dependency_done = true; prepared_dependency (newtm_clone_original (fn->decl), fn->decl); }
    return 0;
  }
};
}
static void finish (void *, void *)
{
  if (seen_error ()) return;
  require (done && cases >= 20 && (unit_only || (production && approved)));
  ggc_collect (GGC_COLLECT_FORCE);
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_local_count (); ++i)
    { newtm_local_view v; require (newtm_obligation_local_get (i, &v) && v.closed); }
  fprintf (stderr, "COMPOSITION scenarios=%u assertions=%u production_nodes=%u approved=%u refused=%u dependencies=%u\n",
    cases, assertions, production, approved, refused, dependencies);
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i)
    { if (!strcmp (args->argv[i].key, "unit")) unit_only = true;
      if (!strcmp (args->argv[i].key, "diagnostic")) diagnostic = args->argv[i].value; }
  register_pass_info info = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &info);
  register_callback (args->base_name, PLUGIN_GGC_MARKING, mark, NULL);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
