#include "flow_probe_common.h"
#include "fold-const.h"
#include "internal-fn.h"
#undef require
#include "newtm-guard.h"
#define expect(V) require_at ((V), __LINE__)
static unsigned guard_cases;
static bool unit_only;
static tree compare (tree_code code, tree a, tree b)
{ return keep (fold_build2 (code, boolean_type_node, a, b)); }
static void prove (tree fn, gimple_seq body, gimple *at, tree predicate, bool expected, int mode = 0)
{
  sample s (fn, body);
  newtm_guard_analysis proof (*s.graph);
  ggc_collect (GGC_COLLECT_FORCE);
  const char *why = NULL; gimple *bad = NULL;
  bool actual = mode == 1 ? proof.arithmetic (at, &why, &bad)
    : mode == 2 ? proof.overflow_dependencies (at, predicate, &why, &bad)
    : proof.require (at, predicate, &why, &bad);
  if (actual != expected)
    internal_error ("guard probe case %u: expected %u, observed %u: %s", guard_cases,
                    expected, actual, why ? why : "proved");
  expect (expected ? !why : why && strstr (why, "V03"));
  ggc_collect (GGC_COLLECT_FORCE);
  ++guard_cases;
}
static gimple_seq guarded (gimple *condition, tree yes, tree no, gimple *at)
{ return seq ({ condition, lab (no), ret (-1), lab (yes), at, ret (0) }); }
static void synthetic_guards (tree fn)
{
  x = keep (create_tmp_var (integer_type_node, "guard_choice"));
  tree a = keep (create_tmp_var (unsigned_type_node, "guard_a"));
  tree b = keep (create_tmp_var (unsigned_type_node, "guard_b"));
  tree ten = build_int_cst (unsigned_type_node, 10);
  tree need = compare (LT_EXPR, a, ten);
  for (unsigned variant = 0; variant < 4; ++variant)
    {
      tree index = keep (create_tmp_var (integer_type_node, "signed_range_index"));
      tree ut = variant == 3 ? unsigned_char_type_node : unsigned_type_node;
      tree converted = keep (create_tmp_var (ut, "compact_range"));
      tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
      tree predicate = variant == 1 ? compare (LT_EXPR, fold_convert (size_type_node, index), size_int (4))
        : compare (GE_EXPR, index, integer_zero_node);
      prove (fn, seq ({ gimple_build_assign (converted, NOP_EXPR, index),
        gimple_build_cond (LE_EXPR, converted, variant == 2 ? TYPE_MAX_VALUE (ut) : build_int_cst (ut, 3), yes, no),
        lab (no), ret (-1), lab (yes), at }), at, predicate, variant < 2);
    }
  for (unsigned variant = 0; variant < 3; ++variant)
    {
      tree bits = keep (create_tmp_var (unsigned_type_node, "combined_flags"));
      tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
      prove (fn, seq ({ gimple_build_assign (bits, variant == 1 ? BIT_AND_EXPR : BIT_IOR_EXPR, a, b),
        gimple_build_cond (variant == 2 ? NE_EXPR : EQ_EXPR, bits, build_zero_cst (unsigned_type_node), yes, no),
        lab (no), ret (-1), lab (yes), at }), at,
        compare (variant == 2 ? NE_EXPR : EQ_EXPR, a, build_zero_cst (unsigned_type_node)), variant == 0);
    }
  for (unsigned variant = 0; variant < 5; ++variant)
    {
      tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
      gimple *test = gimple_build_cond (variant == 1 ? GE_EXPR : variant == 3 ? LE_EXPR : LT_EXPR,
                                      variant == 2 ? b : a, variant == 4 ? build_int_cst (unsigned_type_node, 11) : ten, yes, no);
      prove (fn, guarded (test, yes, no, at), at, need, variant == 0);
    }
  { gimple *at = gimple_build_nop (); prove (fn, seq ({ at }), at, need, false); }
  {
    tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
    prove (fn, seq ({ at, gimple_build_cond (LT_EXPR, a, ten, yes, no), lab (yes), ret (0), lab (no), ret (-1) }), at, need, false);
  }
  {
    tree flag = keep (create_tmp_var (boolean_type_node, "old_valid"));
    tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
    prove (fn, seq ({ gimple_build_assign (flag, LT_EXPR, a, ten),
      gimple_build_assign (a, build_int_cst (unsigned_type_node, 11)),
      gimple_build_cond (NE_EXPR, flag, boolean_false_node, yes, no), lab (no), ret (-1), lab (yes), at }), at, need, false);
  }
  {
    tree value = keep (create_tmp_var (unsigned_type_node, "index_before"));
    tree old = keep (create_tmp_var (unsigned_type_node, "index_snapshot"));
    gimple *at = gimple_build_nop ();
    prove (fn, seq ({ gimple_build_assign (value, build_zero_cst (unsigned_type_node)),
      gimple_build_assign (old, value), gimple_build_assign (value, build_int_cst (unsigned_type_node, 1)), at }), at,
      compare (EQ_EXPR, old, build_zero_cst (unsigned_type_node)), true);
  }
  {
    auto_vec<tree> flags; gimple_seq body = NULL;
    for (unsigned i = 0; i <= 70; ++i)
      {
        flags.safe_push (keep (create_tmp_var (boolean_type_node, "deep_capture")));
        if (i) gimple_seq_add_stmt (&body, gimple_build_assign (flags[i], flags[i - 1]));
      }
    for (unsigned i = 0; i < 60; ++i) gimple_seq_add_stmt (&body, gimple_build_assign (flags[i], boolean_false_node));
    tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
    gimple_seq_add_seq (&body, guarded (gimple_build_cond (NE_EXPR, flags[70], boolean_false_node, yes, no), yes, no, at));
    prove (fn, body, at, compare (NE_EXPR, flags[0], boolean_false_node), false);
  }
  {
    tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
    prove (fn, seq ({ gimple_build_cond (LT_EXPR, a, ten, yes, no), lab (no), ret (-1), lab (yes),
      gimple_build_assign (a, build_int_cst (unsigned_type_node, 11)), at }), at, need, false);
  }
  for (unsigned checked = 0; checked < 2; ++checked)
    {
      tree left = label (), right = label (), check = label (), done_label = label (), failure = label ();
      gimple *at = gimple_build_nop ();
      prove (fn, seq ({ branch (left, right), lab (left), gimple_build_cond (LT_EXPR, a, ten, check, failure),
        lab (check), jump (done_label), lab (right), checked ? gimple_build_cond (LT_EXPR, a, ten, done_label, failure) : jump (done_label),
        lab (failure), ret (-1), lab (done_label), at }), at, need, checked);
    }
  {
    tree yes = label (), no = label (), second = label (); gimple *at = gimple_build_nop ();
    tree both = keep (fold_build2 (BIT_AND_EXPR, boolean_type_node, need,
      compare (NE_EXPR, b, build_zero_cst (unsigned_type_node))));
    prove (fn, seq ({ gimple_build_cond (LT_EXPR, a, ten, second, no), lab (second),
      gimple_build_assign (b, build_int_cst (unsigned_type_node, 2)),
      gimple_build_cond (NE_EXPR, b, build_zero_cst (unsigned_type_node), yes, no), lab (no), ret (-1), lab (yes), at }), at, both, true);
  }
  for (unsigned recheck = 0; recheck < 2; ++recheck)
    {
      tree check = label (), body = label (), exit = label (), failure = label ();
      gimple *at = gimple_build_nop ();
      prove (fn, seq ({ lab (check), gimple_build_cond (LT_EXPR, a, ten, body, failure), lab (failure), ret (-1),
        lab (body), at, gimple_build_assign (a, PLUS_EXPR, a, build_int_cst (unsigned_type_node, 1)),
        branch (recheck ? check : body, exit), lab (exit), ret (0) }), at, need, recheck);
    }
  for (unsigned clobber = 0; clobber < 2; ++clobber)
    {
      tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
      prove (fn, seq ({ gimple_build_cond (LT_EXPR, a, ten, yes, no), lab (no), ret (-1), lab (yes),
        finally (seq ({ store (1) }), seq ({ clobber ? gimple_build_assign (a, build_int_cst (unsigned_type_node, 11)) : store (2) })), at }),
        at, need, !clobber);
    }
  tree out = keep (create_tmp_var (unsigned_type_node, "guard_result"));
  for (unsigned checked = 0; checked < 2; ++checked)
    {
      tree yes = label (), no = label ();
      gimple *at = gimple_build_assign (out, MINUS_EXPR, a, b);
      prove (fn, checked ? guarded (gimple_build_cond (LE_EXPR, b, a, yes, no), yes, no, at) : seq ({ at }), at, NULL_TREE, checked, 1);
    }
  for (unsigned checked = 0; checked < 2; ++checked)
    {
      tree room = keep (create_tmp_var (unsigned_type_node, "guard_room"));
      tree yes = label (), no = label (); gimple *at = gimple_build_assign (out, PLUS_EXPR, a, b);
      prove (fn, checked ? seq ({ gimple_build_assign (room, MINUS_EXPR, TYPE_MAX_VALUE (unsigned_type_node), a),
        gimple_build_cond (LE_EXPR, b, room, yes, no), lab (no), ret (-1), lab (yes), at }) : seq ({ at }), at, NULL_TREE, checked, 1);
    }
  for (unsigned checked = 0; checked < 2; ++checked)
    {
      tree pair = keep (create_tmp_var (build_complex_type (unsigned_type_node), "checked_product"));
      tree flag = keep (create_tmp_var (unsigned_type_node, "overflow_flag"));
      gcall *call = gimple_build_call_internal (IFN_MUL_OVERFLOW, 2, a, b); gimple_call_set_lhs (call, pair);
      gimple *at = gimple_build_nop (); tree yes = label (), no = label ();
      gimple_seq body = seq ({ call,
        gimple_build_assign (out, build1 (REALPART_EXPR, unsigned_type_node, pair)),
        gimple_build_assign (flag, build1 (IMAGPART_EXPR, unsigned_type_node, pair)) });
      if (checked) gimple_seq_add_seq (&body, guarded (gimple_build_cond (EQ_EXPR, flag,
        build_zero_cst (unsigned_type_node), yes, no), yes, no, at));
      else gimple_seq_add_stmt (&body, at);
      prove (fn, body, at, out, checked, 2);
    }
  {
    tree signed_index = keep (create_tmp_var (integer_type_node, "signed_index"));
    tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
    prove (fn, guarded (gimple_build_cond (GE_EXPR, signed_index, integer_zero_node, yes, no), yes, no, at), at,
      compare (GE_EXPR, signed_index, integer_zero_node), true);
  }
  {
    tree type = build_nonstandard_integer_type (128, false);
    tree index = keep (create_tmp_var (type, "wide_index"));
    tree narrowed = keep (create_tmp_var (size_type_node, "narrow_index"));
    tree low = label (), yes = label (), no = label (); gimple *at = gimple_build_nop ();
    prove (fn, seq ({ gimple_build_cond (GE_EXPR, index, build_zero_cst (type), low, no), lab (low),
      gimple_build_assign (narrowed, NOP_EXPR, index),
      gimple_build_cond (LT_EXPR, narrowed, size_int (4), yes, no), lab (no), ret (-1), lab (yes), at }), at,
      compare (LE_EXPR, index, fold_convert (type, TYPE_MAX_VALUE (size_type_node))), false);
  }
  {
    tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
    tree escaped = keep (create_tmp_var (unsigned_type_node, "escaped_index")); TREE_ADDRESSABLE (escaped) = 1;
    tree callee = keep (build_fn_decl ("change_index", build_function_type_list (void_type_node,
      build_pointer_type (unsigned_type_node), NULL_TREE)));
    prove (fn, seq ({ gimple_build_cond (LT_EXPR, escaped, ten, yes, no), lab (no), ret (-1), lab (yes),
      gimple_build_call (callee, 1, build_fold_addr_expr (escaped)), at }), at, compare (LT_EXPR, escaped, ten), false);
  }
  {
    tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
    tree value = keep (create_tmp_var (unsigned_type_node, "aliased_index")); TREE_ADDRESSABLE (value) = 1;
    tree pointer = keep (create_tmp_var (build_pointer_type (unsigned_type_node), "index_alias"));
    prove (fn, seq ({ gimple_build_assign (pointer, build_fold_addr_expr (value)),
      gimple_build_cond (LT_EXPR, value, ten, yes, no), lab (no), ret (-1), lab (yes),
      gimple_build_assign (build_simple_mem_ref (pointer), build_int_cst (unsigned_type_node, 11)), at }), at,
      compare (LT_EXPR, value, ten), false);
  }
  {
    tree yes = label (), no = label (); gimple *at = gimple_build_nop ();
    prove (fn, guarded (gimple_build_cond (EQ_EXPR, integer_zero_node, integer_one_node, yes, no), yes, no, at),
      at, boolean_false_node, true);
  }
  { gimple *at = gimple_build_nop (); prove (fn, seq ({ at }), at, NULL_TREE, false); }
}
namespace {
const pass_data guard_data = { GIMPLE_PASS, "newtm_guard_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct guard_probe : gimple_opt_pass
{
  guard_probe (gcc::context *c) : gimple_opt_pass (guard_data, c) {}
  unsigned execute (function *fn) override
  {
    unsigned count = newtm_obligation_inspections (fn->decl);
    if (newtm_clone_original (fn->decl)) expect (count);
    production += newtm_obligation_flow_nodes (fn->decl);
    if (!done) { done = true; synthetic_guards (fn->decl); }
    return 0;
  }
};
}
static void finish_guards (void *, void *)
{
  expect (done && guard_cases >= 25 && (production || unit_only));
  fprintf (stderr, "GUARDS scenarios=%u assertions=%u production_nodes=%u\n", guard_cases, assertions, production);
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i)
    if (!strcmp (args->argv[i].key, "unit")) unit_only = true;
  register_pass_info info = { new guard_probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &info);
  register_callback (args->base_name, PLUGIN_GGC_MARKING, mark, NULL);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish_guards, NULL);
  return 0;
}
