#include "flow_probe_common.h"

static void synthetic (tree fn)
{
  x = keep (create_tmp_var (integer_type_node, "flow_value"));
  { sample s (fn, NULL); require (s.graph->summary ().normal_exits == 1); }
  {
    gimple *a = store (1), *b = store (2);
    sample s (fn, seq ({ a, b })); require (s.path (a, b) && !s.path (b, a));
  }
  {
    tree yes = label (), no = label (), end = label ();
    gimple *c = branch (yes, no), *a = store (1), *b = store (2), *join = store (3);
    sample s (fn, seq ({ c, lab (yes), a, jump (end), lab (no), b, lab (end), join }));
    require (s.path (a, join) && s.path (b, join));
    require (!s.path (a, b) && !s.path (b, a));
    for (unsigned i = 0; i < s.graph->node_count (); ++i)
      if (s.graph->node (i).statement == c)
        require (s.graph->node (i).successors.length () == 2
                 && s.graph->node (i).successors[0].kind == NEWTM_FLOW_TRUE
                 && s.graph->node (i).successors[1].kind == NEWTM_FLOW_FALSE);
  }
  {
    tree loop = label (); gimple *l = lab (loop), *dead = store (9);
    sample s (fn, seq ({ l, jump (loop), dead }));
    require (s.graph->summary ().cycle_edges && !s.graph->summary ().normal_exits);
    require (s.instances (l) == 1 && s.graph->contains (dead) && !s.graph->reachable (dead));
  }
  {
    tree a = label (), b = label (), end = label ();
    gimple *join = store (4), *la = lab (a), *lb = lab (b);
    sample s (fn, seq ({ branch (a, b), la, branch (b, end), lb, branch (a, end), lab (end), join }));
    require (s.graph->summary ().cycle_edges && s.graph->reachable (join));
    require (s.instances (la) == 1 && s.instances (lb) == 1);
  }
  {
    tree def = label (), range = label (), single = label (), end = label ();
    tree d = build_case_label (NULL_TREE, NULL_TREE, def);
    auto_vec<tree> cases;
    cases.safe_push (build_case_label (integer_one_node, build_int_cst (integer_type_node, 3), range));
    cases.safe_push (build_case_label (build_int_cst (integer_type_node, 7), NULL_TREE, single));
    gimple *sw = gimple_build_switch (x, d, cases), *a = store (1), *b = store (2), *c = store (3);
    sample s (fn, seq ({ sw, lab (def), a, jump (end), lab (range), b, jump (end), lab (single), c, lab (end) }));
    require (s.path (sw, a) && s.path (sw, b) && s.path (sw, c));
    for (unsigned i = 0; i < s.graph->node_count (); ++i)
      if (s.graph->node (i).statement == sw)
        for (const newtm_flow_edge &e : s.graph->node (i).successors)
          require (e.kind == NEWTM_FLOW_CASE && e.selector && TREE_CODE (e.selector) == CASE_LABEL_EXPR);
  }
  {
    tree def = label (), target = label ();
    tree c = build_case_label (integer_one_node, NULL_TREE, target);
    auto_vec<tree> cases; cases.safe_push (c);
    gswitch *sw = gimple_build_switch (x, build_case_label (NULL_TREE, NULL_TREE, def), cases);
    gimple_seq body = seq ({ sw, lab (target), lab (def) });
    sample s (fn, body); const char *reason = NULL;
    require (newtm_obligation_verify (body, &reason));
    CASE_LOW (c) = build_int_cst (integer_type_node, 2);
    require (!newtm_obligation_verify (body, &reason) && strstr (reason, "operand changed"));
    CASE_LOW (c) = integer_one_node;
    CASE_LABEL (c) = def;
    require (!newtm_obligation_verify (body, &reason) && strstr (reason, "operand changed"));
    CASE_LABEL (c) = target;
  }
  {
    gimple *a = store (1), *cleanup = store (2), *after = store (3);
    sample s (fn, seq ({ finally (seq ({ a }), seq ({ cleanup })), after }));
    require (s.path (a, after) && !s.path (a, after, cleanup));
    require (s.graph->summary ().continuations == 1 && s.graph->summary ().normal_exits == 1);
  }
  {
    sample s (fn, seq ({ finally (NULL, NULL) }));
    require (s.graph->summary ().normal_exits == 1 && s.graph->summary ().continuations == 1);
  }
  {
    gimple *r = ret (1), *cleanup = store (2), *after = store (3);
    sample s (fn, seq ({ finally (seq ({ r }), seq ({ cleanup })), after }));
    require (s.path (r, NULL, NULL, r) && !s.path (r, NULL, cleanup, r));
    require (!s.graph->reachable (after) && !s.path (cleanup, r));
  }
  {
    tree inside = label (), outside = label ();
    gimple *start = store (1), *middle = store (2), *cleanup = store (3), *after = store (4);
    sample s (fn, seq ({ finally (seq ({ start, jump (inside), lab (inside), middle, jump (outside) }), seq ({ cleanup })),
                        lab (outside), after }));
    require (s.path (start, middle, cleanup) && !s.path (middle, after, cleanup));
  }
  {
    gimple *r = ret (1), *inner = store (2), *outer = store (3);
    sample s (fn, seq ({ finally (seq ({ finally (seq ({ r }), seq ({ inner })) }), seq ({ outer })) }));
    require (!s.path (r, outer, inner));
    require (s.path (r, NULL, NULL, r) && !s.path (r, NULL, inner, r) && !s.path (r, NULL, outer, r));
  }
  {
    tree yes = label (), no = label ();
    gimple *a = ret (1), *b = ret (2), *cleanup = store (3);
    sample s (fn, seq ({ finally (seq ({ branch (yes, no), lab (yes), a, lab (no), b }), seq ({ cleanup })) }));
    require (s.instances (cleanup) == 2 && s.graph->summary ().return_exits == 2);
    require (s.path (a, NULL, NULL, a) && !s.path (a, NULL, NULL, b));
    require (s.path (b, NULL, NULL, b) && !s.path (b, NULL, NULL, a));
  }
  {
    gimple *a = ret (1), *b = ret (2);
    sample s (fn, seq ({ finally (seq ({ a }), seq ({ b })) }));
    require (!s.path (a, NULL, NULL, a) && s.path (a, NULL, NULL, b));
    require (s.graph->summary ().return_exits == 1 && !s.graph->summary ().normal_exits);
  }
  {
    tree outside = label (); gimple *r = ret (1), *after = store (2);
    sample s (fn, seq ({ finally (seq ({ r }), seq ({ jump (outside) })), lab (outside), after }));
    require (!s.path (r, NULL, NULL, r) && s.graph->reachable (after));
    require (s.graph->summary ().normal_exits == 1 && !s.graph->summary ().return_exits);
  }
  tree die = keep (build_fn_decl ("flow_die", build_function_type_list (void_type_node, NULL_TREE)));
  TREE_THIS_VOLATILE (die) = 1; TREE_NOTHROW (die) = 1;
  {
    gimple *r = ret (1), *stop = gimple_build_call (die, 0), *after = store (3);
    sample s (fn, seq ({ finally (seq ({ r }), seq ({ stop })), after }));
    require (!s.path (r, NULL, NULL, r) && !s.graph->reachable (after));
    require (s.graph->summary ().terminate_exits == 1 && !s.graph->summary ().return_exits);
  }
  {
    gimple *stop = gimple_build_call (die, 0), *cleanup = store (2);
    sample s (fn, seq ({ finally (seq ({ stop }), seq ({ cleanup })) }));
    require (s.graph->summary ().terminate_exits == 1 && !s.graph->reachable (cleanup));
  }
  {
    tree loop = label (), end = label ();
    gimple *r = ret (1), *cleanup = store (2);
    sample s (fn, seq ({ finally (seq ({ r }), seq ({ lab (loop), cleanup, branch (loop, end), lab (end) })) }));
    require (s.graph->summary ().cycle_edges && s.instances (cleanup) == 1 && s.path (r, NULL, NULL, r));
  }
  {
    tree inside = label (), outside = label ();
    gimple *r = ret (1), *inner = store (2), *tail = store (3), *after = store (4);
    /* Inner cleanup cancels its goto out, while the outer pending return
       remains valid when control stays within the outer cleanup. */
    gimple *inner_try = finally (seq ({ jump (outside) }), seq ({ inner, jump (inside) }));
    sample s (fn, seq ({ finally (seq ({ r }), seq ({ inner_try, lab (inside), tail })), lab (outside), after }));
    require (s.path (r, tail) && s.path (tail, NULL, NULL, r) && !s.graph->reachable (after));
  }
  {
    tree target = label (); gimple *init = store (1), *body = store (2), *after = store (3);
    sample s (fn, seq ({ jump (target), gimple_build_bind (NULL_TREE, seq ({ init, lab (target), body }), NULL_TREE), after }));
    require (!s.graph->reachable (init) && s.path (body, after));
  }
  { sample s (fn, seq ({ jump (label ()) }), "missing, external or computed"); }
  { sample s (fn, seq ({ ret (0), jump (label ()) }), "missing, external or computed"); }
  { tree l = label (); sample s (fn, seq ({ lab (l), lab (l) }), "duplicate flow label"); }
  { sample s (fn, seq ({ jump (x) }), "missing, external or computed"); }
  { tree l = label (); sample s (fn, seq ({ branch (l, NULL_TREE), lab (l) }), "missing, external or computed"); }
  {
    tree l = label (); auto_vec<tree> cases;
    gswitch *sw = gimple_build_switch (x, build_case_label (NULL_TREE, NULL_TREE, l), cases);
    gimple_switch_set_label (sw, 0, build_case_label (integer_one_node, NULL_TREE, l));
    sample s (fn, seq ({ sw, lab (l) }), "malformed flow switch labels");
  }
  {
    tree l = label ();
    sample s (fn, seq ({ jump (l), finally (seq ({ lab (l) }), NULL) }), "structured try/finally entry");
  }
  {
    tree l = label ();
    sample s (fn, seq ({ jump (l), finally (NULL, seq ({ lab (l) })) }), "structured try/finally entry");
  }
  { sample s (fn, seq ({ gimple_build_try (NULL, NULL, GIMPLE_TRY_CATCH) }), "unsupported exception handler"); }
  { sample s (fn, seq ({ ret (0), gimple_build_resx (0) }), "unsupported High GIMPLE"); }
  {
    gcall *call = gimple_build_call (die, 0); gimple_call_set_ctrl_altering (call, true);
    sample s (fn, seq ({ call }), "nonlocal or multiple-return");
  }
  {
    tree twice = keep (build_fn_decl ("flow_twice", build_function_type_list (integer_type_node, NULL_TREE)));
    DECL_IS_RETURNS_TWICE (twice) = 1;
    gcall *call = gimple_build_call (twice, 0); gimple_call_set_lhs (call, x);
    sample s (fn, seq ({ call }), "nonlocal or multiple-return");
  }
  {
    gimple_seq shared = seq ({ store (1) });
    sample s (fn, seq ({ gimple_build_bind (NULL_TREE, shared, NULL_TREE),
                        gimple_build_bind (NULL_TREE, shared, NULL_TREE) }), "repeated instruction instance");
  }
  {
    sample s (fn, seq ({ store (1) })); const char *reason = NULL;
    require (!s.graph->build (NULL, &reason) && strstr (reason, "rebuilt for each inspection"));
  }
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_flow_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass
{
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    unsigned inspected = newtm_obligation_inspections (fn->decl);
    unsigned nodes = newtm_obligation_flow_nodes (fn->decl);
    require (!inspected || nodes >= inspected);
    if (newtm_clone_original (fn->decl)) require (inspected && nodes);
    production += nodes;
    if (!done) { done = true; synthetic (fn->decl); }
    return 0;
  }
};
}
static void finish (void *, void *)
{
  require (done && scenarios >= 28 && production);
  fprintf (stderr, "FLOW scenarios=%u assertions=%u production_nodes=%u\n", scenarios, assertions, production);
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  register_pass_info info = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &info);
  register_callback (args->base_name, PLUGIN_GGC_MARKING, mark, NULL);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
