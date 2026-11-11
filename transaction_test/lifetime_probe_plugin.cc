/* No synthetic body is executed or installed in an executable function. */
#include "flow_probe_common.h"
#undef require
#include "newtm-lifetime.h"
#include "newtm-abi.h"
#define expect(V) require_at ((V), __LINE__)
static unsigned cases;
static bool unit_only;
struct fixture
{
  tree packet, copy, array, scalar, pointer, alias, indirect, value_out, binding_out, service;
  fixture (tree fn, bool pointer_return = false)
  {
    packet = keep (create_tmp_var (newtm_abi_clone_binding_type (), "packet"));
    copy = keep (create_tmp_var (TREE_TYPE (packet), "copy"));
    array = keep (create_tmp_var (build_array_type_nelts (TREE_TYPE (packet), 2), "packets"));
    scalar = keep (create_tmp_var (uint32_type_node, "local"));
    pointer = keep (create_tmp_var (build_pointer_type (uint32_type_node), "pointer"));
    alias = keep (create_tmp_var (build_pointer_type (TREE_TYPE (packet)), "alias"));
    indirect = keep (create_tmp_var (build_pointer_type (TREE_TYPE (pointer)), "indirect"));
    value_out = keep (build_decl (UNKNOWN_LOCATION, PARM_DECL, NULL_TREE,
      build_pointer_type (pointer_return ? TREE_TYPE (pointer) : TREE_TYPE (scalar))));
    binding_out = keep (build_decl (UNKNOWN_LOCATION, PARM_DECL, NULL_TREE, build_pointer_type (TREE_TYPE (packet))));
    DECL_CONTEXT (value_out) = DECL_CONTEXT (binding_out) = fn;
    service = keep (build_fn_decl ("borrow", build_function_type_list (uint32_type_node,
      ptr_type_node, ptr_type_node, ptr_type_node, ptr_type_node, NULL_TREE)));
  }
  tree field (unsigned n, tree record = NULL_TREE)
  {
    if (!record) record = packet;
    return n == 4 ? newtm_clone_storage (record) : newtm_clone_field (newtm_clone_identity (record), n);
  }
  gimple_seq fill (int missing = -1, tree record = NULL_TREE)
  {
    gimple_seq body = NULL;
    for (unsigned i = 0; i < 5; ++i) if (int (i) != missing)
      { tree f = field (i, record); gimple_seq_add_stmt (&body, gimple_build_assign (f, build_zero_cst (TREE_TYPE (f)))); }
    return body;
  }
  gimple *call (tree data = NULL_TREE, tree result = NULL_TREE)
  { return gimple_build_call (service, 4, null_pointer_node, build_fold_addr_expr (packet),
      data ? data : null_pointer_node, result ? build_fold_addr_expr (result) : null_pointer_node); }
  gimple *publish_value (tree v = NULL_TREE)
  { return gimple_build_assign (build_simple_mem_ref (value_out), v ? v : build_zero_cst (TREE_TYPE (scalar))); }
  gimple *publish_binding () { return gimple_build_assign (build_simple_mem_ref (binding_out), packet); }
  gimple_seq bind (gimple_seq body)
  {
    tree vars[] = {packet, copy, array, scalar, pointer, alias, indirect};
    for (unsigned i = 0; i + 1 < ARRAY_SIZE (vars); ++i) DECL_CHAIN (vars[i]) = vars[i + 1];
    DECL_CHAIN (indirect) = NULL_TREE;
    return seq ({ gimple_build_bind (packet, body, NULL_TREE) });
  }
};
struct output_spec { gimple *call; tree value; bool pointer; unsigned input_end; };
static void prove (tree fn, fixture &f, gimple_seq body, bool expected, unsigned publication = 0,
                   std::initializer_list<gimple *> inputs = {},
                   std::initializer_list<output_spec> outputs = {}, bool production = false,
                   std::initializer_list<gimple *> mutations = {},
                   std::initializer_list<gimple *> array_inputs = {})
{
  sample sample (fn, f.bind (body));
  newtm_lifetime_analysis proof (*sample.graph, fn);
  proof.watch (f.packet); proof.watch (f.copy); proof.watch (f.array);
  if (publication)
    { proof.publication (f.value_out, publication == 3 ? f.binding_out : NULL_TREE);
      proof.borrowed (f.value_out); proof.borrowed (f.binding_out); }
  for (gimple *g : inputs) { proof.input (g, f.packet); proof.borrow (g, 1); }
  for (const output_spec &o : outputs)
    { proof.output (o.call, o.value, o.pointer, false, o.input_end); proof.borrow (o.call, 3); proof.borrow (o.call, 1); }
  for (gimple *g : mutations) proof.effects (g, 2, 3);
  for (gimple *g : array_inputs)
    {
      tree first = build4 (ARRAY_REF, TREE_TYPE (f.packet), f.array, size_zero_node, NULL_TREE, NULL_TREE);
      proof.input (g, first, tree_to_shwi (TYPE_SIZE_UNIT (TREE_TYPE (f.array)))); proof.borrow (g, 1);
    }
  ggc_collect (GGC_COLLECT_FORCE);
  const char *why = NULL; gimple *bad = NULL;
  bool actual = production ? newtm_obligation_verify (sample.body, &why, &bad) : proof.verify (&why, &bad);
  if (actual != expected)
    internal_error ("lifetime case %u: expected %u, observed %u: %s", cases, expected, actual, why ? why : "proved");
  expect (expected ? !why : why && (strstr (why, "V04") || strstr (why, "V05")));
  ggc_collect (GGC_COLLECT_FORCE); ++cases;
}
static void append (gimple_seq &a, gimple_seq b) { gimple_seq_add_seq (&a, b); }
static void synthetic (tree fn)
{
  x = keep (create_tmp_var (integer_type_node, "choice"));
  for (int missing = -1; missing < 5; ++missing)
    {
      fixture f (fn); gimple *call = f.call (); gimple_seq b = f.fill (missing);
      gimple_seq_add_stmt (&b, call); prove (fn, f, b, missing < 0, 0, {call});
    }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      fixture f (fn); tree l = label (), r = label (), end = label ();
      gimple *call = f.call (); gimple_seq b = seq ({ branch (l, r), lab (l) });
      append (b, f.fill ()); append (b, seq ({ jump (end), lab (r) }));
      append (b, f.fill (variant == 1 ? 3 : -1));
      append (b, seq ({ lab (end), call }));
      if (variant == 2) append (b, f.fill ());
      prove (fn, f, b, variant != 1, 0, {call});
    }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      fixture f (fn); gimple *call = f.call (); gimple_seq b = NULL;
      if (variant == 0) append (b, seq ({ gimple_build_assign (f.packet, build_constructor (TREE_TYPE (f.packet), NULL)) }));
      if (variant == 1) append (b, f.fill ());
      append (b, seq ({ gimple_build_assign (f.copy, f.packet), call }));
      prove (fn, f, b, variant < 2, 0, {call});
    }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      fixture f (fn); gimple *call = f.call (); gimple_seq b = f.fill ();
      append (b, seq ({ gimple_build_assign (f.packet, build_clobber (TREE_TYPE (f.packet), CLOBBER_STORAGE_END)) }));
      if (variant)
        { append (b, seq ({ gimple_build_assign (f.packet, build_clobber (TREE_TYPE (f.packet), CLOBBER_STORAGE_BEGIN)) }));
          if (variant == 1) append (b, f.fill ()); }
      gimple_seq_add_stmt (&b, call);
      prove (fn, f, b, variant == 1, 0, {call});
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn); gimple_seq b = f.fill ();
      tree slot = build4 (ARRAY_REF, TREE_TYPE (f.packet), f.array, size_zero_node, NULL_TREE, NULL_TREE);
      append (b, f.fill (-1, slot));
      if (variant)
        { slot = build4 (ARRAY_REF, TREE_TYPE (f.packet), f.array, size_one_node, NULL_TREE, NULL_TREE); append (b, f.fill (-1, slot)); }
      append (b, seq ({ gimple_build_assign (f.copy, build4 (ARRAY_REF, TREE_TYPE (f.packet), f.array,
        size_one_node, NULL_TREE, NULL_TREE)) }));
      prove (fn, f, b, variant);
    }
  for (unsigned variant = 0; variant < 6; ++variant)
    {
      fixture f (fn); gimple_seq b = f.fill ();
      if (variant != 1) gimple_seq_add_stmt (&b, f.publish_value ());
      if (variant == 2) gimple_seq_add_stmt (&b, f.publish_value ());
      if (variant == 3) gimple_seq_add_stmt (&b, f.call ());
      if (variant == 4)
        append (b, seq ({ gimple_build_assign (f.scalar, build_int_cst (TREE_TYPE (f.scalar), 0)), gimple_build_return (f.scalar) }));
      else gimple_seq_add_stmt (&b, ret (variant == 5 ? 2 : 0));
      prove (fn, f, b, variant == 0 || variant == 4, 1);
    }
  for (unsigned variant = 0; variant < 4; ++variant)
    {
      fixture f (fn); gimple_seq b = f.fill ();
      if (variant != 1) gimple_seq_add_stmt (&b, f.publish_binding ());
      if (variant != 2) gimple_seq_add_stmt (&b, f.publish_value ());
      gimple_seq_add_stmt (&b, ret (variant == 3 ? 2 : 0));
      prove (fn, f, b, !variant, 3);
    }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      fixture f (fn, true); gimple_seq b = f.fill ();
      tree p = variant == 0 ? build_fold_addr_expr (f.scalar) : build_zero_cst (TREE_TYPE (f.pointer));
      append (b, seq ({ gimple_build_assign (f.pointer, p) }));
      if (variant == 2) append (b, seq ({ gimple_build_assign (f.pointer, NOP_EXPR, f.value_out) }));
      append (b, seq ({ f.publish_value (f.pointer), ret (0) }));
      prove (fn, f, b, variant == 1, 1);
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn, true); gimple_seq b = f.fill ();
      gimple *call = f.call (variant ? build_zero_cst (TREE_TYPE (f.pointer)) : build_fold_addr_expr (f.scalar), f.pointer);
      append (b, seq ({ call, f.publish_value (f.pointer), ret (0) }));
      prove (fn, f, b, variant, 1, {}, {{call, f.pointer, true, 3}});
    }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      fixture f (fn); gimple_seq b = f.fill (); gimple *call = f.call ();
      append (b, seq ({ gimple_build_assign (f.alias, build_fold_addr_expr (f.packet)) }));
      if (variant == 0)
        append (b, seq ({ gimple_build_assign (f.packet, build_clobber (TREE_TYPE (f.packet), CLOBBER_STORAGE_END)) }));
      if (variant == 1)
        append (b, seq ({ gimple_build_assign (f.copy, build_simple_mem_ref (f.alias)) }));
      else gimple_seq_add_stmt (&b, call);
      prove (fn, f, b, variant != 0, 0, variant == 1 ? std::initializer_list<gimple *> {} : std::initializer_list<gimple *> {call});
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn); tree head = label (), end = label (), again = label ();
      gimple *call = f.call (); gimple_seq b = NULL;
      if (variant) append (b, f.fill ());
      append (b, seq ({ lab (head), call, branch (again, end), lab (again) }));
      if (!variant) append (b, f.fill ());
      append (b, seq ({ jump (head), lab (end) }));
      prove (fn, f, b, variant, 0, {call});
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn); tree inner = keep (create_tmp_var (uint32_type_node, "inner"));
      gimple *call = f.call (); gimple_seq b = f.fill ();
      gimple_seq nested = seq ({ gimple_build_assign (inner, build_zero_cst (TREE_TYPE (inner))),
        gimple_build_assign (f.field (0), build_fold_addr_expr (inner)) });
      if (variant) gimple_seq_add_stmt (&nested, call);
      gimple_seq_add_stmt (&b, gimple_build_bind (inner, nested, NULL_TREE));
      if (!variant) gimple_seq_add_stmt (&b, call);
      prove (fn, f, b, variant, 0, {call});
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn, true); tree l = label (), r = label (), end = label ();
      gimple_seq b = f.fill ();
      append (b, seq ({ gimple_build_assign (f.pointer, variant ? build_zero_cst (TREE_TYPE (f.pointer)) : build_fold_addr_expr (f.scalar)),
        branch (l, r), lab (l), gimple_build_assign (f.indirect, build_fold_addr_expr (f.pointer)), jump (end),
        lab (r), gimple_build_assign (f.indirect, build_fold_addr_expr (f.pointer)), lab (end),
        gimple_build_assign (f.pointer, build_simple_mem_ref (f.indirect)), f.publish_value (f.pointer), ret (0) }));
      prove (fn, f, b, variant, 1);
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn, true); gimple_seq b = f.fill ();
      gimple *call = f.call (build_fold_addr_expr (variant ? f.scalar : f.pointer));
      append (b, seq ({ gimple_build_assign (f.pointer, build_zero_cst (TREE_TYPE (f.pointer))), call,
        f.publish_value (f.pointer), ret (0) }));
      prove (fn, f, b, variant, 1, {call}, {}, false, {call});
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn); gimple *call = f.call (null_pointer_node, variant ? f.copy : f.packet);
      gimple_seq b = f.fill (); gimple_seq_add_stmt (&b, call);
      prove (fn, f, b, variant, 0, {call}, {{call, variant ? f.copy : f.packet, false, 3}});
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn); tree ctor = keep (build_constructor (TREE_TYPE (f.packet), NULL));
      CONSTRUCTOR_NO_CLEARING (ctor) = variant;
      gimple_seq b = seq ({ gimple_build_assign (f.packet, ctor), gimple_build_assign (f.copy, f.packet) });
      prove (fn, f, b, !variant);
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn); vec<constructor_elt, va_gc> *identity = NULL, *outer = NULL;
      tree type = newtm_abi_binding_type ();
      for (tree field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field))
        if (!variant || field == TYPE_FIELDS (type))
          CONSTRUCTOR_APPEND_ELT (identity, field, build_zero_cst (TREE_TYPE (field)));
      tree inner = keep (build_constructor (type, identity)); CONSTRUCTOR_NO_CLEARING (inner) = 1;
      tree field = TYPE_FIELDS (TREE_TYPE (f.packet));
      CONSTRUCTOR_APPEND_ELT (outer, field, inner);
      CONSTRUCTOR_APPEND_ELT (outer, DECL_CHAIN (field), build_zero_cst (uint32_type_node));
      tree ctor = keep (build_constructor (TREE_TYPE (f.packet), outer)); CONSTRUCTOR_NO_CLEARING (ctor) = 1;
      prove (fn, f, seq ({ gimple_build_assign (f.packet, ctor), gimple_build_assign (f.copy, f.packet) }), !variant);
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn); vec<constructor_elt, va_gc> *identity = NULL, *outer = NULL;
      CONSTRUCTOR_APPEND_ELT (identity, TYPE_FIELDS (newtm_abi_binding_type ()), f.field (0));
      tree inner = keep (build_constructor (newtm_abi_binding_type (), identity));
      CONSTRUCTOR_APPEND_ELT (outer, TYPE_FIELDS (TREE_TYPE (f.packet)), inner);
      tree ctor = keep (build_constructor (TREE_TYPE (f.packet), outer));
      gimple_seq b = variant ? f.fill () : NULL;
      append (b, seq ({ gimple_build_assign (f.copy, ctor) }));
      prove (fn, f, b, variant);
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn); gimple_seq b = f.fill ();
      if (variant) gimple_seq_add_stmt (&b, gimple_build_assign (f.scalar, build_zero_cst (TREE_TYPE (f.scalar))));
      append (b, seq ({ gimple_build_assign (f.field (3), f.scalar), gimple_build_assign (f.copy, f.packet) }));
      prove (fn, f, b, variant);
    }
  for (unsigned variant = 0; variant < 2; ++variant)
    {
      fixture f (fn); tree inner = keep (create_tmp_var (uint32_type_node, "second_object"));
      tree first = build4 (ARRAY_REF, TREE_TYPE (f.packet), f.array, size_zero_node, NULL_TREE, NULL_TREE);
      tree second = build4 (ARRAY_REF, TREE_TYPE (f.packet), f.array, size_one_node, NULL_TREE, NULL_TREE);
      gimple_seq b = f.fill (-1, first); append (b, f.fill (-1, second));
      gimple *call = f.call (); gimple_call_set_arg (call, 1, build_fold_addr_expr (first));
      gimple_seq nested = seq ({ gimple_build_assign (f.field (0, second), build_fold_addr_expr (inner)) });
      if (variant) gimple_seq_add_stmt (&nested, call);
      gimple_seq_add_stmt (&b, gimple_build_bind (inner, nested, NULL_TREE));
      if (!variant) gimple_seq_add_stmt (&b, call);
      prove (fn, f, b, variant, 0, {}, {}, false, {}, {call});
    }
  /* Complete inspector: ABI-shaped automatic objects receive no name exemption. */
  for (int missing = -1; missing < 5; ++missing)
    {
      fixture f (fn); gimple_seq b = f.fill (missing);
      gimple_seq_add_stmt (&b, gimple_build_assign (f.copy, f.packet));
      prove (fn, f, b, missing < 0, 0, {}, {}, true);
    }
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_lifetime_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass
{
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (newtm_clone_original (fn->decl)) expect (newtm_obligation_inspections (fn->decl));
    production += newtm_obligation_flow_nodes (fn->decl);
    if (!done && !newtm_clone_original (fn->decl)) { done = true; synthetic (fn->decl); }
    return 0;
  }
};
}
static void finish (void *, void *)
{ expect (done && cases >= 61 && (unit_only || production));
  fprintf (stderr, "LIFETIME scenarios=%u assertions=%u production_nodes=%u\n", cases, assertions, production); }
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
