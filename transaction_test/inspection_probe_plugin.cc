/* Negative IR tests use the production inspector, without inserting test IR
   into executable functions or extending descriptor lifetimes. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "context.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "ggc.h"
#include "diagnostic-core.h"
#include "newtm-obligation.h"
#include "newtm-clone.h"
#include "newtm-abi.h"
#include "internal-fn.h"
int plugin_is_GPL_compatible;
static unsigned checks, inspected;
static bool done;
static tree roots;
static tree keep (tree t)
{ roots = tree_cons (NULL_TREE, t, roots); return t; }
static void mark (void *, void *) { gt_ggc_mx (roots); }
static void require_at (bool value, unsigned line)
{ if (!value) internal_error ("transaction inspection probe invariant failed at line %u", line); }
#define require(V) require_at ((V), __LINE__)
static gimple_seq sequence (gimple *stmt)
{ gimple_seq seq = NULL; gimple_seq_add_stmt (&seq, stmt); return seq; }
static void test (tree fn, gimple_seq seq, bool expected, const char *fragment = NULL,
                  bool claim = false, bool emitted = false)
{
  require (newtm_obligation_begin (fn, seq, NEWTM_OB_COPY));
  if (claim) newtm_obligation_declare_all (gimple_seq_first_stmt (seq), NEWTM_OB_PRIVATE);
  if (emitted) newtm_obligation_emitted (seq);
  ggc_collect (GGC_COLLECT_FORCE);
  const char *reason = NULL;
  bool actual = newtm_obligation_verify (seq, &reason);
  if (actual != expected)
    internal_error ("inspection probe case %u: expected %u, observed %u: %s", checks, expected, actual,
                    reason ? reason : "accepted");
  if (expected ? reason != NULL : !reason || (fragment && !strstr (reason, fragment)))
    internal_error ("inspection probe case %u: expected diagnostic %s, observed %s", checks,
                    fragment ? fragment : "none", reason ? reason : "none");
  newtm_obligation_end (); ++checks;
}
static void synthetic (tree fn)
{
  tree x = keep (create_tmp_var (integer_type_node, "probe_value"));
  tree local = keep (create_tmp_var (integer_type_node, "probe_local"));
  TREE_ADDRESSABLE (local) = 1;
  tree ptr = create_tmp_var (build_pointer_type (integer_type_node), "newtm_private_buffer");
  tree unknown = keep (build_simple_mem_ref (ptr));
  test (fn, sequence (gimple_build_assign (x, local)), true);
  test (fn, sequence (gimple_build_assign (local, x)), true);
  for (unsigned disguise = 0; disguise < 3; ++disguise)
    {
      test (fn, sequence (gimple_build_assign (x, unknown)), false, "direct transaction read", disguise == 1, disguise == 2);
      test (fn, sequence (gimple_build_assign (unknown, x)), false, "direct transaction write", disguise == 1, disguise == 2);
    }
  tree global = keep (build_decl (UNKNOWN_LOCATION, VAR_DECL, NULL_TREE, integer_type_node));
  TREE_STATIC (global) = 1; DECL_ARTIFICIAL (global) = 1;
  test (fn, sequence (gimple_build_assign (x, global)), false, "direct transaction read");
  test (fn, sequence (gimple_build_assign (global, x)), false, "direct transaction write");
  tree callee = keep (build_fn_decl ("probe_unprepared", build_function_type_list (integer_type_node, NULL_TREE)));
  gcall *call = gimple_build_call (callee, 0); gimple_call_set_lhs (call, x);
  test (fn, sequence (call), false, "unprepared transaction call", false, true);
  tree fp = create_tmp_var (build_pointer_type (TREE_TYPE (callee)), "probe_indirect");
  call = gimple_build_call (fp, 0); gimple_call_set_lhs (call, x);
  test (fn, sequence (call), false, "indirect transaction call");
  call = gimple_build_call (builtin_decl_explicit (BUILT_IN_ABS), 1, x);
  gimple_call_set_lhs (call, x);
  test (fn, sequence (call), false, "unclassified builtin");
  tree status = keep (create_tmp_var (uint32_type_node, "probe_status"));
  tree args[] = { build_zero_cst (newtm_abi_context_type ()), build_fold_addr_expr (local),
    size_zero_node, build_fold_addr_expr (local), size_int (4) };
  call = newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_READ, status, 5, args);
  test (fn, sequence (call), false, "no matching source identity", false, true);
  tree registration_args[] = { build_zero_cst (newtm_abi_context_type ()), build_fold_addr_expr (local),
    size_int (4), build_zero_cst (uint32_type_node) };
  call = newtm_abi_build_call (BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER, status, 4, registration_args);
  test (fn, sequence (call), false, "no matching registration identity", false, true);
  call = gimple_build_call_internal (IFN_ADD_OVERFLOW, 2, x, x);
  gimple_call_set_lhs (call, x);
  test (fn, sequence (call), false, "unclassified internal");
  /* Mutations are made after evidence capture, including in-place changes
     to a shared tree, rather than only replacing a top-level operand. */
  gassign *load = gimple_build_assign (x, local);
  gimple_seq seq = sequence (load);
  require (newtm_obligation_begin (fn, seq, NEWTM_OB_COPY));
  gimple_assign_set_rhs1 (load, global);
  const char *reason = NULL;
  require (!newtm_obligation_verify (seq, &reason) && strstr (reason, "operand changed"));
  newtm_obligation_end (); ++checks;
  tree array = create_tmp_var (build_array_type_nelts (integer_type_node, 2), "probe_array");
  tree element = build4 (ARRAY_REF, integer_type_node, array, size_zero_node, NULL_TREE, NULL_TREE);
  load = gimple_build_assign (x, element); seq = sequence (load);
  require (newtm_obligation_begin (fn, seq, NEWTM_OB_COPY));
  TREE_OPERAND (element, 1) = size_one_node;
  require (!newtm_obligation_verify (seq, &reason) && strstr (reason, "operand changed"));
  newtm_obligation_end (); ++checks;
  element = build4 (ARRAY_REF, integer_type_node, array, size_int (99), NULL_TREE, NULL_TREE);
  test (fn, sequence (gimple_build_assign (x, element)), false, "direct transaction read");
  /* Merely being in the enclosing function does not make a controller's
     preexisting local part of its newly created attempt-private storage. */
  seq = sequence (gimple_build_assign (x, local));
  require (newtm_obligation_begin (fn, seq, NEWTM_OB_CONTROLLER));
  require (!newtm_obligation_verify (seq, &reason) && strstr (reason, "direct transaction read"));
  newtm_obligation_end (); ++checks;
  load = gimple_build_assign (x, local); seq = sequence (load);
  require (newtm_obligation_begin (fn, seq, NEWTM_OB_COPY));
  gimple_seq_add_stmt (&seq, gimple_build_assign (local, x));
  require (!newtm_obligation_verify (seq, &reason) && strstr (reason, "no current structural evidence"));
  newtm_obligation_end (); ++checks;
  load = gimple_build_assign (x, local); seq = sequence (load);
  require (newtm_obligation_begin (fn, seq, NEWTM_OB_COPY));
  gimple_seq copied = sequence (gimple_copy (load));
  require (!newtm_obligation_verify (copied, &reason));
  newtm_obligation_end (); ++checks;
  require (!newtm_obligation_verify (seq, &reason) && strstr (reason, "outside descriptor window")); ++checks;
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_inspection_probe", OPTGROUP_NONE,
  TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass
{
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    unsigned count = newtm_obligation_inspections (fn->decl);
    if (newtm_clone_original (fn->decl)) require (count);
    inspected += count;
    if (!done) { done = true; synthetic (fn->decl); }
    return 0;
  }
};
}
static void finish (void *, void *)
{
  require (done && inspected && checks >= 16);
  fprintf (stderr, "INSPECTION negative_and_local=%u scopes=%u\n", checks, inspected);
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  register_pass_info info = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &info);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  register_callback (args->base_name, PLUGIN_GGC_MARKING, mark, NULL);
  return 0;
}
