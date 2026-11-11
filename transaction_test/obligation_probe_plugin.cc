/* Inspect historical facts; synthetic IR exercises inventory without execution. */
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
#include "newtm-obligation.h"
#include "newtm-occurrence.h"
#include "newtm-clone.h"
#include "diagnostic-core.h"
int plugin_is_GPL_compatible;
static bool synthetic_done, real_copy;
static void require (bool condition)
{ if (!condition) internal_error ("obligation probe invariant failed"); }

static unsigned HOST_WIDE_INT find_record (unsigned HOST_WIDE_INT first, newtm_obligation_kind kind,
                                          unsigned HOST_WIDE_INT parent = 0)
{
  for (unsigned HOST_WIDE_INT id = first; id <= newtm_obligation_count (); ++id)
    {
      newtm_obligation_view v;
      require (newtm_obligation_get (id, &v));
      if (v.kind == kind && v.parent == parent) return id;
    }
  return 0;
}
static void synthetic (tree fn)
{
  tree ptr = create_tmp_var (build_pointer_type (integer_type_node), "probe_pointer");
  tree x = create_tmp_var (integer_type_node, "probe_value");
  tree lhs = build_simple_mem_ref (ptr);
  gassign *load = gimple_build_assign (x, lhs);
  gassign *both = gimple_build_assign (lhs, build_simple_mem_ref (ptr));
  gimple_seq seq = NULL;
  gimple_seq_add_stmt (&seq, load); gimple_seq_add_stmt (&seq, both);
  unsigned HOST_WIDE_INT first = newtm_obligation_count () + 1;
  require (newtm_obligation_begin (fn, seq, NEWTM_OB_COPY));
  require (!newtm_obligation_begin (fn, seq, NEWTM_OB_COPY));
  unsigned HOST_WIDE_INT read = find_record (first, NEWTM_OB_READ);
  unsigned HOST_WIDE_INT write = find_record (first, NEWTM_OB_WRITE);
  require (read && write && newtm_obligation_has_anchor (read));
  newtm_obligation_view v;
  require (newtm_obligation_get (read, &v) && !v.source && !v.operation);
  require (v.state == NEWTM_OB_PENDING && v.treatment == NEWTM_OB_UNDECLARED);
  newtm_obligation_declare (load, NEWTM_OB_READ, NEWTM_OB_PRIVATE);
  require (newtm_obligation_get (read, &v) && v.state == NEWTM_OB_PENDING && v.treatment == NEWTM_OB_PRIVATE);
  unsigned HOST_WIDE_INT copied = newtm_obligation_count () + 1;
  gimple *copy = gimple_copy (load);
  require (newtm_obligation_count () >= copied && newtm_obligation_get (copied, &v));
  require (v.copied_from == read && v.state == NEWTM_OB_PENDING
              && v.evidence == NEWTM_OB_INVALIDATED && v.treatment == NEWTM_OB_UNDECLARED);
  newtm_obligation_declare (copy, NEWTM_OB_READ, NEWTM_OB_STM);
  require (newtm_obligation_get (copied, &v) && v.treatment == NEWTM_OB_UNDECLARED);
  ggc_collect (GGC_COLLECT_FORCE);
  require (newtm_obligation_has_anchor (copied));
  gassign *store = gimple_build_assign (lhs, x);
  unsigned HOST_WIDE_INT derived = newtm_obligation_count () + 1;
  newtm_obligation_derive_store (both, store);
  unsigned HOST_WIDE_INT child = find_record (derived, NEWTM_OB_WRITE, write);
  require (child && newtm_obligation_get (write, &v) && v.state == NEWTM_OB_DERIVED && v.children == 1);
  newtm_obligation_invalidate (child);
  require (newtm_obligation_get (write, &v) && v.evidence == NEWTM_OB_INVALIDATED && v.state == NEWTM_OB_PENDING);
  unsigned HOST_WIDE_INT paired_read = find_record (write + 1, NEWTM_OB_READ);
  require (newtm_obligation_get (paired_read, &v) && v.instance == write
           && v.evidence == NEWTM_OB_INVALIDATED);
  newtm_obligation_forget (load);
  require (!newtm_obligation_has_anchor (read) && newtm_obligation_get (read, &v));
  require (v.state == NEWTM_OB_PENDING && v.evidence == NEWTM_OB_RETIRED);
  newtm_obligation_reject ("synthetic refusal");
  newtm_obligation_invalidate (read);
  require (newtm_obligation_get (read, &v) && v.state == NEWTM_OB_REJECTED && v.reason);
  newtm_obligation_end ();
  ggc_collect (GGC_COLLECT_FORCE);
  require (!newtm_obligation_has_anchor (child) && newtm_obligation_consistent (fn));
}
static tree copy_source (gimple_stmt_iterator *it, bool *, walk_stmt_info *)
{
  if (real_copy) return NULL_TREE;
  gimple *stmt = gsi_stmt (*it);
  if (!is_gimple_assign (stmt)) return NULL_TREE;
  tree b = newtm_occurrence_binding (stmt);
  if (!b) return NULL_TREE;
  unsigned HOST_WIDE_INT original = 0;
  for (unsigned HOST_WIDE_INT id = 1; id <= newtm_obligation_count (); ++id)
    {
      newtm_obligation_view v;
      newtm_obligation_get (id, &v);
      if (v.owner == current_function_decl && v.operation == tree_to_uhwi (TREE_VEC_ELT (b, 0)))
        { original = id; break; }
    }
  if (!original) return NULL_TREE;
  unsigned HOST_WIDE_INT first = newtm_obligation_count () + 1;
  gimple *copy = gimple_copy (stmt);
  require (newtm_obligation_count () >= first);
  newtm_obligation_view v;
  require (newtm_obligation_get (first, &v) && v.copied_from && v.copied_operation);
  require (v.scope_kind == NEWTM_OB_COPY && v.evidence == NEWTM_OB_INVALIDATED
              && v.treatment == NEWTM_OB_UNDECLARED && v.state == NEWTM_OB_PENDING);
  require (!newtm_obligation_has_anchor (first));
  newtm_occurrence_forget (copy);
  real_copy = true;
  return NULL_TREE;
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_obligation_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
class probe : public gimple_opt_pass {
public:
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *) final override {
    ggc_collect (GGC_COLLECT_FORCE);
    require (newtm_obligation_consistent (current_function_decl));
    for (unsigned HOST_WIDE_INT id = 1; id <= newtm_obligation_count (); ++id)
      {
        newtm_obligation_view v;
        newtm_obligation_get (id, &v);
        require (!newtm_obligation_has_anchor (id) && v.evidence != NEWTM_OB_CURRENT
                    && v.state != NEWTM_OB_COVERED);
      }
    if (!synthetic_done) { synthetic (current_function_decl); synthetic_done = true; }
    walk_stmt_info wi = {};
    walk_gimple_seq (gimple_body (current_function_decl), copy_source, NULL, &wi);
    return 0;
  }
};
}
static void finish (void *, void *)
{
  if (seen_error ()) return;
  unsigned kinds = 0, source = 0, unbound = 0, split = 0, reconciled = 0, rejected = 0;
  unsigned private_ops = 0, dynamic_ops = 0, stm_ops = 0, delegated = 0, publications = 0;
  for (unsigned HOST_WIDE_INT id = 1; id <= newtm_obligation_count (); ++id)
    {
      newtm_obligation_view v;
      newtm_obligation_get (id, &v);
      if (v.scope_kind == NEWTM_OB_COPY) continue;
      kinds |= 1u << v.kind;
      source += v.source; unbound += !v.source;
      split += v.treatment == NEWTM_OB_SPLIT_STORE;
      reconciled += v.treatment == NEWTM_OB_RECONCILED;
      rejected += v.state == NEWTM_OB_REJECTED;
      private_ops += v.treatment == NEWTM_OB_PRIVATE;
      dynamic_ops += v.treatment == NEWTM_OB_DYNAMIC;
      stm_ops += v.treatment == NEWTM_OB_STM;
      delegated += v.treatment == NEWTM_OB_DELEGATED;
      publications += v.treatment == NEWTM_OB_PUBLICATION;
      if (v.treatment == NEWTM_OB_PREPARED_CALL && v.kind == NEWTM_OB_CALL)
        require (v.callee && v.declared_callee == newtm_clone_for (v.callee));
      if (v.treatment == NEWTM_OB_PRIVATE
          && (v.kind == NEWTM_OB_READ || v.kind == NEWTM_OB_WRITE))
        require (v.storage == 1);
      require (v.state != NEWTM_OB_COVERED && !newtm_obligation_has_anchor (id));
    }
  if (!synthetic_done || !real_copy || kinds != 127 || !source || !unbound
      || !private_ops || !dynamic_ops || !stm_ops || !publications)
    error ("obligation probe incomplete: kinds=%u source=%u unbound=%u split=%u private=%u dynamic=%u stm=%u publication=%u copy=%u",
           kinds, source, unbound, split, private_ops, dynamic_ops, stm_ops, publications, real_copy);
  fprintf (stderr, "OBLIGATION kinds=%u source=%u unbound=%u split=%u reconciled=%u rejected=%u delegated=%u\n",
           kinds, source, unbound, split, reconciled, rejected, delegated);
}
int plugin_init (plugin_name_args *info, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  register_pass_info p = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (info->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
