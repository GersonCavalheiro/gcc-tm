/* Contracts transported across LTO; approval must be reconstructed.
   Copyright (C) 2026 Free Software Foundation, Inc.
   Distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "ssa.h"
#include "tree-phinodes.h"
#include "tree-ssanames.h"
#include "tree-ssa.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "cgraph.h"
#include "options.h"
#include "stringpool.h"
#include "attribs.h"
#include "diagnostic-core.h"
#include "newtm-current.h"
#include "newtm-abi.h"
#include "newtm-clone.h"
#include "newtm-access.h"
#include "newtm-lto.h"

/* Diagnostic context uses declarations only, never expired operands. */
static void lto_refuse (tree fn, const char *message, const char *rule = "V08",
                        gimple *stmt = NULL)
{
  location_t loc = stmt ? gimple_location (stmt) : UNKNOWN_LOCATION;
  if (loc == UNKNOWN_LOCATION && fn) loc = DECL_SOURCE_LOCATION (fn);
  error_at (loc, "%s", message);
  if (fn)
    inform (loc, "transaction rule %s, operation LTO reconstruction, function %qD",
            rule, fn);
}

static GTY(()) tree outgoing_contracts;
static GTY(()) tree contract_unit;
static GTY(()) hash_map<tree, tree> *source_ssa_origins;
tree newtm_lto_source_ssa (tree var)
{ tree *p = source_ssa_origins ? source_ssa_origins->get (var) : NULL; return p ? *p : NULL_TREE; }
static GTY(()) hash_map<tree, tree> *incoming_contracts;
static const char marker[] = "newtm lto contracts";
static const char identity[] = "newtm local identity";
bool newtm_lto_protected_p (tree fn)
{ return fn && lookup_attribute (marker, DECL_ATTRIBUTES (fn)); }
namespace {
bool managed_entry (tree fn)
{
  tree attr = lookup_attribute (marker, DECL_ATTRIBUTES (fn));
  tree args = attr ? TREE_VALUE (attr) : NULL_TREE;
  if (!args || TREE_CODE (args) != TREE_LIST) return false;
  tree settings = TREE_VALUE (args);
  if (!settings || TREE_CODE (settings) != TREE_LIST) return false;
  tree tail = TREE_CHAIN (settings);
  tree mode = tail && TREE_CODE (tail) == TREE_LIST ? TREE_PURPOSE (tail) : NULL_TREE;
  return mode && TREE_CODE (mode) == INTEGER_CST && integer_onep (mode);
}
}
namespace {
void mark (tree t, hash_set<tree> &seen)
{
  if (!t || seen.add (t)) return;
  if (DECL_P (t))
    {
      if (decl_function_context (t) && TREE_CODE (t) != FUNCTION_DECL
          && !lookup_attribute (identity, DECL_ATTRIBUTES (t)))
        DECL_ATTRIBUTES (t) = tree_cons (get_identifier (identity),
          tree_cons (contract_unit, build_int_cst (long_unsigned_type_node, DECL_UID (t)), NULL_TREE),
          DECL_ATTRIBUTES (t));
      return;
    }
  if (TYPE_P (t)) return;
  if (TREE_CODE (t) == TREE_VEC)
    for (int i = 0; i < TREE_VEC_LENGTH (t); ++i) mark (TREE_VEC_ELT (t, i), seen);
  else if (TREE_CODE (t) == TREE_LIST)
    { mark (TREE_PURPOSE (t), seen); mark (TREE_VALUE (t), seen); mark (TREE_CHAIN (t), seen); }
  else if (EXPR_P (t))
    for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i) mark (TREE_OPERAND (t, i), seen);
}
}
void newtm_lto_prepare ()
{
  if (in_lto_p || !flag_generate_lto) return;
  auto_vec<tree> functions;
  hash_set<tree> seen;
  tree roots = NULL_TREE;
  /* Collect first: changing a function attribute invalidates the old window. */
  for (unsigned HOST_WIDE_INT i = 1; i <= newtm_current_count (); ++i)
    {
      newtm_current_view v;
      if (!newtm_current_get (i, &v) || !v.current_approved) continue;
      roots = tree_cons (NULL_TREE, v.root, roots);
      for (unsigned j = 0; j < v.members; ++j)
        { tree fn; unsigned mask; newtm_current_member (i, j, &fn, &mask);
          if (!seen.add (fn)) functions.safe_push (fn); }
    }
  /* GCC also preserves prepared auxiliaries which have no active external
     controller (for example the unused external version of a nested region).
     Transport their contracts without treating them as graph roots or
     granting them authority. They can still be materialized for emission. */
  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    if (newtm_access_prepared_p (node->decl) && DECL_STRUCT_FUNCTION (node->decl)
        && DECL_STRUCT_FUNCTION (node->decl)->cfg && !seen.add (node->decl))
      functions.safe_push (node->decl);
  if (functions.is_empty ()) return;
  outgoing_contracts = make_tree_vec (6);
  TREE_VEC_ELT (outgoing_contracts, 0) = build_int_cst (unsigned_type_node, 1);
  TREE_VEC_ELT (outgoing_contracts, 1) = roots;
  TREE_VEC_ELT (outgoing_contracts, 2) = newtm_clone_export ();
  TREE_VEC_ELT (outgoing_contracts, 3) = newtm_memory_export ();
  TREE_VEC_ELT (outgoing_contracts, 4) = newtm_access_export ();
  contract_unit = build_translation_unit_decl (get_identifier ("newtm LTO contracts"));
  hash_set<tree> marked;
  mark (outgoing_contracts, marked);
  for (tree fn : functions)
    {
      /* Keep the first LTO proof domain faithful to scalar source identities.
         Optimizations after the proof window do not inherit its authority. */
      tree original = DECL_FUNCTION_SPECIFIC_OPTIMIZATION (fn);
      if (!original) original = build_optimization_node (&global_options, &global_options_set);
      tree settings = tree_cons (original, DECL_ATTRIBUTES (fn),
        tree_cons (build_int_cst (unsigned_type_node, flag_openmp_transaction_stm != 0),
                   build_int_cst (unsigned_type_node, DECL_UNINLINABLE (fn)), NULL_TREE));
      DECL_ATTRIBUTES (fn) = tree_cons (get_identifier (marker),
        tree_cons (NULL_TREE, settings, NULL_TREE), DECL_ATTRIBUTES (fn));
      for (const char *name : { "noipa", "noinline", "noclone", "no_icf" })
        if (!lookup_attribute (name, DECL_ATTRIBUTES (fn)))
          DECL_ATTRIBUTES (fn) = tree_cons (get_identifier (name), NULL_TREE, DECL_ATTRIBUTES (fn));
      DECL_UNINLINABLE (fn) = true;
      gcc_options opts = global_options, sets = global_options_set;
      if (original) cl_optimization_restore (&opts, &sets, TREE_OPTIMIZATION (original));
      opts.x_optimize = 0; opts.x_optimize_size = 0; opts.x_optimize_debug = 0;
      DECL_FUNCTION_SPECIFIC_OPTIMIZATION (fn) = build_optimization_node (&opts, &sets);
      for (tree p = DECL_ARGUMENTS (fn); p; p = DECL_CHAIN (p)) mark (p, marked);
      mark (DECL_RESULT (fn), marked);
      /* Some pre-SSA gimplifier temporaries are already SSA names. Their
         scalar version is a source identity, not the renumbered LTO version. */
      unsigned index; tree name;
      if (DECL_STRUCT_FUNCTION (fn)->gimple_df)
      FOR_EACH_SSA_NAME (index, name, DECL_STRUCT_FUNCTION (fn))
        if (!virtual_operand_p (name))
          {
            tree origin = make_tree_vec (3);
            TREE_VEC_ELT (origin, 0) = SSA_NAME_VAR (name);
            TREE_VEC_ELT (origin, 1) = build_int_cst (unsigned_type_node, SSA_NAME_VERSION (name));
            tree var = build_decl (UNKNOWN_LOCATION, VAR_DECL, NULL_TREE, TREE_TYPE (name));
            DECL_CONTEXT (var) = fn; DECL_ARTIFICIAL (var) = true; DECL_IGNORED_P (var) = true;
            if (!source_ssa_origins) source_ssa_origins = hash_map<tree, tree>::create_ggc ();
            source_ssa_origins->put (var, origin); SET_SSA_NAME_VAR_OR_IDENTIFIER (name, var);
            mark (var, marked);
          }
      basic_block bb;
      FOR_EACH_BB_FN (bb, DECL_STRUCT_FUNCTION (fn))
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          for (unsigned i = 0; i < gimple_num_ops (gsi_stmt (it)); ++i)
            mark (gimple_op (gsi_stmt (it), i), marked);
    }
  TREE_VEC_ELT (outgoing_contracts, 5) = newtm_lto_pack (source_ssa_origins);
}
void newtm_lto_finish (bool native)
{
  if ((!in_lto_p && !native) || flag_wpa || flag_incremental_link == INCREMENTAL_LINK_LTO) return;
  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    if (tree attr = lookup_attribute (marker, DECL_ATTRIBUTES (node->decl)))
      {
        tree settings = TREE_VALUE (TREE_VALUE (attr));
        if (!settings || TREE_CODE (settings) != TREE_LIST || !TREE_CHAIN (settings)
            || TREE_CODE (TREE_CHAIN (settings)) != TREE_LIST)
          { lto_refuse (node->decl, "transaction LTO optimization envelope is invalid"); continue; }
        DECL_FUNCTION_SPECIFIC_OPTIMIZATION (node->decl) = TREE_PURPOSE (settings);
        DECL_ATTRIBUTES (node->decl) = TREE_VALUE (settings);
        DECL_UNINLINABLE (node->decl) = !integer_zerop (TREE_VALUE (TREE_CHAIN (settings)));
      }
}
tree newtm_lto_payload (tree fn)
{
  if (!newtm_lto_protected_p (fn)) return NULL_TREE;
  if (!in_lto_p) return outgoing_contracts;
  tree *p = incoming_contracts ? incoming_contracts->get (fn) : NULL;
  return p ? *p : NULL_TREE;
}
namespace {
bool valid_map (tree list)
{
  hash_set<tree> seen;
  for (tree p = list; p; p = TREE_CHAIN (p))
    if (TREE_CODE (p) != TREE_LIST || seen.add (p) || seen.elements () > 1048576
        || !TREE_PURPOSE (p) || !(DECL_P (TREE_PURPOSE (p)) || TYPE_P (TREE_PURPOSE (p))))
      return false;
  return true;
}
bool valid_settings (tree fn)
{
  tree attr = lookup_attribute (marker, DECL_ATTRIBUTES (fn));
  tree args = attr ? TREE_VALUE (attr) : NULL_TREE;
  if (!args || TREE_CODE (args) != TREE_LIST || TREE_CHAIN (args)) return false;
  tree settings = TREE_VALUE (args);
  if (!settings || TREE_CODE (settings) != TREE_LIST || !TREE_PURPOSE (settings)
      || TREE_CODE (TREE_PURPOSE (settings)) != OPTIMIZATION_NODE
      || (TREE_VALUE (settings) && TREE_CODE (TREE_VALUE (settings)) != TREE_LIST)) return false;
  tree tail = TREE_CHAIN (settings);
  return tail && TREE_CODE (tail) == TREE_LIST && !TREE_CHAIN (tail) && TREE_VALUE (tail)
    && TREE_PURPOSE (tail) && TREE_CODE (TREE_PURPOSE (tail)) == INTEGER_CST
    && (integer_zerop (TREE_PURPOSE (tail)) || integer_onep (TREE_PURPOSE (tail)))
    && TREE_CODE (TREE_VALUE (tail)) == INTEGER_CST
    && (integer_zerop (TREE_VALUE (tail)) || integer_onep (TREE_VALUE (tail)));
}
}
void newtm_lto_receive (tree fn, tree data)
{
  if (!data && !newtm_lto_protected_p (fn)) return;
  if (!valid_settings (fn) || !data || TREE_CODE (data) != TREE_VEC || TREE_VEC_LENGTH (data) != 6
      || !TREE_VEC_ELT (data, 0) || TREE_CODE (TREE_VEC_ELT (data, 0)) != INTEGER_CST
      || !integer_onep (TREE_VEC_ELT (data, 0)))
    { lto_refuse (fn, "transaction LTO contract version or envelope is invalid"); return; }
  /* Legacy ABI probes and explicit runtime drivers have no compiler-owned
     controller. They transport body facts, never graph approval. */
  if (!managed_entry (fn) && TREE_VEC_ELT (data, 1))
    { lto_refuse (fn, "transaction LTO manual ABI entry cannot publish graph roots"); return; }
  const int sizes[] = {17, 11, 1};
  for (unsigned i = 0; i < 3; ++i)
    if (!TREE_VEC_ELT (data, i + 2) || TREE_CODE (TREE_VEC_ELT (data, i + 2)) != TREE_VEC
        || TREE_VEC_LENGTH (TREE_VEC_ELT (data, i + 2)) != sizes[i])
      { lto_refuse (fn, "transaction LTO contract section is invalid"); return; }
  for (unsigned i = 0; i < 3; ++i)
    for (int j = 0; j < sizes[i]; ++j)
      if (!valid_map (TREE_VEC_ELT (TREE_VEC_ELT (data, i + 2), j)))
        { lto_refuse (fn, "transaction LTO contract map is invalid"); return; }
  if (!valid_map (TREE_VEC_ELT (data, 5)))
    { lto_refuse (fn, "transaction LTO source identity map is invalid"); return; }
  for (tree p = TREE_VEC_ELT (data, 5); p; p = TREE_CHAIN (p))
    {
      tree origin = TREE_VALUE (p);
      if (!VAR_P (TREE_PURPOSE (p)) || !origin || TREE_CODE (origin) != TREE_VEC
          || TREE_VEC_LENGTH (origin) != 3 || !TREE_VEC_ELT (origin, 1)
          || TREE_CODE (TREE_VEC_ELT (origin, 1)) != INTEGER_CST
          || !tree_fits_uhwi_p (TREE_VEC_ELT (origin, 1))
          || tree_to_uhwi (TREE_VEC_ELT (origin, 1)) > UINT_MAX
          || (TREE_VEC_ELT (origin, 0) && !DECL_P (TREE_VEC_ELT (origin, 0)))
          || TREE_VEC_ELT (origin, 2))
        { lto_refuse (fn, "transaction LTO source identity record is invalid"); return; }
    }
  if (!incoming_contracts) incoming_contracts = hash_map<tree, tree>::create_ggc ();
  incoming_contracts->put (fn, data);
}


namespace {
/* Function context plus a source-unit scalar id canonicalizes local copies
   read from separate body streams. Actual operands take priority over facts. */
struct local_registry
{
  hash_map<tree, tree> entries;
  tree local (tree d)
  {
    if (!d || !DECL_P (d) || TREE_CODE (d) == FUNCTION_DECL) return d;
    tree attr = lookup_attribute (identity, DECL_ATTRIBUTES (d));
    if (!attr || !TREE_VALUE (attr)) return d;
    tree id = TREE_VALUE (TREE_VALUE (attr)), fn = TREE_PURPOSE (TREE_VALUE (attr));
    if (!fn || TREE_CODE (fn) != TRANSLATION_UNIT_DECL || !id || TREE_CODE (id) != INTEGER_CST)
      { lto_refuse (d, "transaction LTO local identity is invalid"); return d; }
    tree *list = entries.get (fn);
    if (list)
      for (tree p = *list; p; p = TREE_CHAIN (p))
        if (tree_int_cst_equal (TREE_PURPOSE (p), id)) return TREE_VALUE (p);
    entries.put (fn, tree_cons (id, d, list ? *list : NULL_TREE));
    return d;
  }
  tree rewrite (tree t, hash_set<tree> &seen)
  {
    if (!t) return t;
    if (DECL_P (t)) return local (t);
    if (TYPE_P (t) || seen.add (t)) return t;
    if (TREE_CODE (t) == TREE_VEC)
      for (int i = 0; i < TREE_VEC_LENGTH (t); ++i) TREE_VEC_ELT (t, i) = rewrite (TREE_VEC_ELT (t, i), seen);
    else if (TREE_CODE (t) == TREE_LIST)
      { TREE_PURPOSE (t) = rewrite (TREE_PURPOSE (t), seen);
        TREE_VALUE (t) = rewrite (TREE_VALUE (t), seen);
        TREE_CHAIN (t) = rewrite (TREE_CHAIN (t), seen); }
    else if (EXPR_P (t))
      for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i) TREE_OPERAND (t, i) = rewrite (TREE_OPERAND (t, i), seen);
    return t;
  }
  void operand (tree t)
  {
    if (!t) return;
    if (TREE_CODE (t) == SSA_NAME) { local (SSA_NAME_VAR (t)); return; }
    if (DECL_P (t)) { local (t); return; }
    if (EXPR_P (t))
      for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i) operand (TREE_OPERAND (t, i));
  }
};
/* Check that erasing SSA renaming is semantics preserving, before presenting
   a temporary declaration CFG to the existing independent proof engines.
   Every SSA use must be the reaching version of its declaration. Each PHI
   must select exactly that declaration's reaching version on its own edge.
   No edge, operation or executable assignment is synthesized by this view. */
struct projection
{
  function *fn;
  auto_vec<tree> vars, defaults;
  hash_map<tree, unsigned> indices;
  hash_map<tree, tree> anonymous;
  tree symbol (tree n)
  {
    if (tree var = SSA_NAME_VAR (n)) return var;
    if (tree *p = anonymous.get (n)) return *p;
    tree var = build_decl (UNKNOWN_LOCATION, VAR_DECL, NULL_TREE, TREE_TYPE (n));
    DECL_CONTEXT (var) = fn->decl; DECL_ARTIFICIAL (var) = true;
    anonymous.put (n, var); return var;
  }
  auto_vec<auto_vec<tree> *> in, out;
  struct replacement { tree *where; tree old; };
  struct phis { basic_block bb; gimple_seq seq; };
  auto_vec<replacement> replaced;
  auto_vec<phis> hidden;
  bool applied = false;
  projection (function *f) : fn (f) {}
  ~projection ()
  {
    for (replacement r : replaced) *r.where = r.old;
    for (phis p : hidden) set_phi_nodes (p.bb, p.seq);
    if (applied) fn->gimple_df->in_ssa_p = true;
    for (auto_vec<tree> *v : in) delete v;
    for (auto_vec<tree> *v : out) delete v;
  }
  bool scalar (tree n) { return n && TREE_CODE (n) == SSA_NAME && !virtual_operand_p (n); }
  bool declare (tree n)
  {
    if (!scalar (n)) return true;
    tree var = symbol (n);
    if (!var || (TREE_CODE (var) != VAR_DECL && TREE_CODE (var) != PARM_DECL))
      { if (flag_transaction_debug) fprintf (stderr, "NEWTM SSA has no scalar declaration: version=%u\n", SSA_NAME_VERSION (n)); return false; }
    if (!indices.get (var))
      { unsigned i = vars.length (); indices.put (var, i); vars.safe_push (var); defaults.safe_push (NULL_TREE); }
    if (SSA_NAME_IS_DEFAULT_DEF (n)) defaults[*indices.get (var)] = n;
    return true;
  }
  bool scan (tree t)
  {
    if (!t) return true;
    if (TREE_CODE (t) == SSA_NAME) return declare (t);
    if (EXPR_P (t))
      for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i) if (!scan (TREE_OPERAND (t, i))) return false;
    return true;
  }
  bool uses (tree t, const auto_vec<tree> &state)
  {
    if (!t) return true;
    if (scalar (t))
      {
        tree expected = state[*indices.get (symbol (t))];
        if (expected == t) return true;
        if (flag_transaction_debug) fprintf (stderr, "NEWTM SSA stale use: version=%u expected=%d\n", SSA_NAME_VERSION (t), expected && expected != error_mark_node ? (int) SSA_NAME_VERSION (expected) : -1);
        return false;
      }
    if (EXPR_P (t))
      for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i) if (!uses (TREE_OPERAND (t, i), state)) return false;
    return true;
  }
  void erase (tree *p)
  {
    tree t = *p;
    if (!t) return;
    if (scalar (t)) { replacement r = {p, t}; replaced.safe_push (r); *p = symbol (t); }
    else if (EXPR_P (t)) for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i) erase (&TREE_OPERAND (t, i));
  }
  bool build ()
  {
    if (!fn || !fn->cfg || !gimple_in_ssa_p (fn)) return false;
    basic_block bb;
    FOR_EACH_BB_FN (bb, fn)
      {
        for (gphi_iterator it = gsi_start_phis (bb); !gsi_end_p (it); gsi_next (&it))
          {
            if (!declare (gimple_phi_result (it.phi ()))) return false;
            for (unsigned i = 0; i < gimple_phi_num_args (it.phi ()); ++i)
              if (!scan (gimple_phi_arg_def (it.phi (), i))) return false;
          }
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          for (unsigned i = 0; i < gimple_num_ops (gsi_stmt (it)); ++i)
            if (!scan (gimple_op (gsi_stmt (it), i))) return false;
      }
    unsigned blocks = last_basic_block_for_fn (fn);
    in.safe_grow_cleared (blocks); out.safe_grow_cleared (blocks);
    FOR_ALL_BB_FN (bb, fn)
      {
        in[bb->index] = new auto_vec<tree>; out[bb->index] = new auto_vec<tree>;
        in[bb->index]->safe_grow_cleared (vars.length ()); out[bb->index]->safe_grow_cleared (vars.length ());
      }
    for (unsigned i = 0; i < vars.length (); ++i) (*out[ENTRY_BLOCK_PTR_FOR_FN (fn)->index])[i] = defaults[i];
    bool changed = true; unsigned iterations = 0;
    while (changed)
      {
        if (++iterations > 4096) return false;
        changed = false;
        FOR_EACH_BB_FN (bb, fn)
          {
            auto_vec<tree> state; state.safe_grow_cleared (vars.length ());
            for (unsigned i = 0; i < vars.length (); ++i)
              {
                tree merged = NULL_TREE; edge e; edge_iterator ei;
                FOR_EACH_EDGE (e, ei, bb->preds)
                  { tree v = (*out[e->src->index])[i];
                    if (!merged) merged = v; else if (v && merged != v) merged = error_mark_node; }
                state[i] = merged;
              }
            for (gphi_iterator it = gsi_start_phis (bb); !gsi_end_p (it); gsi_next (&it))
              { tree n = gimple_phi_result (it.phi ());
                if (scalar (n)) state[*indices.get (symbol (n))] = n; }
            for (unsigned i = 0; i < vars.length (); ++i) (*in[bb->index])[i] = state[i];
            for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
              { tree lhs = gimple_get_lhs (gsi_stmt (it));
                if (scalar (lhs)) state[*indices.get (symbol (lhs))] = lhs; }
            for (unsigned i = 0; i < vars.length (); ++i)
              if ((*out[bb->index])[i] != state[i]) { (*out[bb->index])[i] = state[i]; changed = true; }
          }
      }
    FOR_EACH_BB_FN (bb, fn)
      {
        for (gphi_iterator it = gsi_start_phis (bb); !gsi_end_p (it); gsi_next (&it))
          {
            gphi *phi = it.phi (); tree n = gimple_phi_result (phi);
            if (!scalar (n)) continue;
            tree var = symbol (n); unsigned index = *indices.get (var);
            for (unsigned i = 0; i < gimple_phi_num_args (phi); ++i)
              { tree arg = gimple_phi_arg_def (phi, i);
                if (!scalar (arg) || symbol (arg) != var
                    || (*out[gimple_phi_arg_edge (phi, i)->src->index])[index] != arg)
                  { if (flag_transaction_debug) fprintf (stderr, "NEWTM SSA phi mismatch: result=%u arg=%u block=%d\n", SSA_NAME_VERSION (n), i, bb->index); return false; } }
          }
        auto_vec<tree> state; state.safe_grow_cleared (vars.length ());
        for (unsigned i = 0; i < vars.length (); ++i) state[i] = (*in[bb->index])[i];
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          {
            gimple *s = gsi_stmt (it); if (is_gimple_debug (s)) continue;
            tree lhs = gimple_get_lhs (s);
            for (unsigned i = 0; i < gimple_num_ops (s); ++i)
              if (!(i == 0 && scalar (lhs) && gimple_op (s, i) == lhs) && !uses (gimple_op (s, i), state)) return false;
            if (scalar (lhs)) state[*indices.get (symbol (lhs))] = lhs;
          }
      }
    /* No proof has seen a changed operand until every SSA rule succeeded. */
    FOR_EACH_BB_FN (bb, fn)
      {
        phis p = {bb, phi_nodes (bb)}; hidden.safe_push (p); set_phi_nodes (bb, NULL);
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          if (!is_gimple_debug (gsi_stmt (it)))
            for (unsigned i = 0; i < gimple_num_ops (gsi_stmt (it)); ++i) erase (gimple_op_ptr (gsi_stmt (it), i));
      }
    fn->gimple_df->in_ssa_p = false; applied = true; return true;
  }
};
static auto_vec<projection *> *active_projection;
static hash_map<tree, tree> *native_images;
static bool lto_window;
}
bool newtm_lto_window_p () { return lto_window; }
tree newtm_lto_image (tree fn)
{ tree *p = native_images ? native_images->get (fn) : NULL; return p ? *p : NULL_TREE; }
void newtm_lto_projection_end ()
{
  if (active_projection)
    { for (projection *p : *active_projection) delete p; delete active_projection; active_projection = NULL; }
  delete native_images; native_images = NULL;
}
bool newtm_lto_projection_begin ()
{
  if (!lto_window || active_projection) return false;
  active_projection = new auto_vec<projection *>;
  native_images = new hash_map<tree, tree>;
  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    if (newtm_lto_protected_p (node->decl) && DECL_STRUCT_FUNCTION (node->decl)
        && DECL_STRUCT_FUNCTION (node->decl)->cfg)
      {
        tree image = newtm_current_ir (node->decl);
        if (!image) { lto_refuse (node->decl, "transaction LTO current IR cannot be represented"); return false; }
        native_images->put (node->decl, image);
        projection *p = new projection (DECL_STRUCT_FUNCTION (node->decl)); active_projection->safe_push (p);
        if (!p->build ())
          { lto_refuse (node->decl, "transaction LTO SSA correspondence cannot be reconstructed"); return false; }
      }
  return true;
}
void newtm_lto_rebuild_unit ()
{
  if (!in_lto_p || lto_window || seen_error ()) return;
  auto_vec<cgraph_node *> functions;
  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    if (newtm_lto_protected_p (node->decl)) functions.safe_push (node);
  if (functions.is_empty ()) return;
  for (cgraph_node *n : functions)
    {
      if (n->clone_of || n->alias || n->thunk)
        { lto_refuse (n->decl, "transaction LTO member was transformed before reconstruction"); return; }
      n->get_untransformed_body ();
      if (!DECL_STRUCT_FUNCTION (n->decl) || !DECL_STRUCT_FUNCTION (n->decl)->cfg)
        { lto_refuse (n->decl, "transaction LTO proof member has no loaded body"); return; }
    }
  local_registry registry;
  for (cgraph_node *n : functions)
    {
      tree fn = n->decl;
      for (tree p = DECL_ARGUMENTS (fn); p; p = DECL_CHAIN (p)) registry.local (p);
      registry.local (DECL_RESULT (fn));
      basic_block bb;
      FOR_EACH_BB_FN (bb, DECL_STRUCT_FUNCTION (fn))
        {
          for (gphi_iterator it = gsi_start_phis (bb); !gsi_end_p (it); gsi_next (&it))
            { registry.operand (gimple_phi_result (it.phi ()));
              for (unsigned i = 0; i < gimple_phi_num_args (it.phi ()); ++i) registry.operand (gimple_phi_arg_def (it.phi (), i)); }
          for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
            for (unsigned i = 0; i < gimple_num_ops (gsi_stmt (it)); ++i) registry.operand (gimple_op (gsi_stmt (it), i));
        }
    }
  hash_set<tree> rewritten, roots;
  for (cgraph_node *n : functions)
    {
      tree data = newtm_lto_payload (n->decl);
      if (!data) { lto_refuse (n->decl, "transaction LTO member lost its contracts"); return; }
      registry.rewrite (data, rewritten);
      newtm_lto_unpack (source_ssa_origins, TREE_VEC_ELT (data, 5));
      newtm_clone_import (TREE_VEC_ELT (data, 2));
      newtm_memory_import (TREE_VEC_ELT (data, 3));
      newtm_access_import (TREE_VEC_ELT (data, 4));
      hash_set<tree> root_list;
      for (tree p = TREE_VEC_ELT (data, 1); p; p = TREE_CHAIN (p))
        {
          if (TREE_CODE (p) != TREE_LIST || root_list.add (p) || !TREE_VALUE (p)
              || TREE_CODE (TREE_VALUE (p)) != FUNCTION_DECL)
            { lto_refuse (n->decl, "transaction LTO graph root list is invalid"); return; }
          roots.add (TREE_VALUE (p));
        }
    }
  for (cgraph_node *n : functions)
    if (newtm_clone_original (n->decl))
      {
        tree context = newtm_clone_context (n->decl), bindings = newtm_clone_channel (n->decl, 2);
        if (!context || !bindings || !newtm_abi_load_types (TREE_TYPE (context), TREE_TYPE (bindings)))
          { lto_refuse (n->decl, "transaction LTO signature has an incompatible ABI layout", "V06"); return; }
        break;
      }
  /* Reconstruct active entries from current calls, not from the manifest's
     assertion that a source root existed. Preserved unused prepared versions
     have contracts but grant no authority and need no fictional controller. */
  /* A private prepared entry cannot acquire an uninspected caller outside
     the transported proof unit. Check incoming cgraph edges too: otherwise
     a caller without a loaded CFG could escape the statement traversal. */
  for (cgraph_node *n : functions)
    if (newtm_clone_original (n->decl) && managed_entry (n->decl))
      for (cgraph_edge *e = n->callers; e; e = e->next_caller)
        if (!newtm_lto_protected_p (e->caller->decl))
          { lto_refuse (e->caller->decl, "transaction LTO prepared entry has an unprotected caller", "V06"); return; }
  hash_set<tree> active_roots;
  for (tree root : roots)
    {
      tree owner = newtm_clone_region_owner (root);
      if (!owner || TREE_CODE (owner) != FUNCTION_DECL)
        { lto_refuse (root, "transaction LTO root lost its controller identity"); return; }
      cgraph_node *source = cgraph_node::get (owner);
      if (source && source->definition)
        {
          if (!newtm_lto_protected_p (owner))
            { lto_refuse (owner, "transaction LTO controller lost its contracts"); return; }
          /* A live owner must not silently lose a region when its invocation
             is removed/replaced. The full proof requires the real unique
             invocation; the manifest alone still cannot approve anything. */
          active_roots.add (root);
        }
    }
  for (cgraph_node *n : functions)
    {
      if (newtm_access_prepared_p (n->decl) && !newtm_clone_original (n->decl))
        { lto_refuse (n->decl, "transaction LTO prepared member lost its signature identity"); return; }
      if (newtm_clone_original (n->decl)) continue;
      {
        unsigned admissions = 0, entries = 0, endings = 0;
        basic_block bb;
        FOR_EACH_BB_FN (bb, DECL_STRUCT_FUNCTION (n->decl))
          for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
            {
              gimple *s = gsi_stmt (it);
              tree callee = is_gimple_call (s) ? gimple_call_fndecl (s) : NULL_TREE;
              if (callee && fndecl_built_in_p (callee, BUILT_IN_NORMAL))
                {
                  if (DECL_FUNCTION_CODE (callee) == BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN) ++admissions;
                  if (DECL_FUNCTION_CODE (callee) == BUILT_IN_GOMP_TRANSACTION_STM_V1_END) ++endings;
                }
              if (!callee || !newtm_clone_original (callee)) continue;
              ++entries;
              if (!roots.contains (callee))
                { lto_refuse (n->decl, "transaction LTO current entry has no graph root contract", "V01", s); return; }
              active_roots.add (callee);
            }
        if (admissions != entries || (endings && !admissions))
          { lto_refuse (n->decl, "transaction LTO controller lost its prepared invocation", "V07"); return; }
      }
    }
  lto_window = true;
  for (tree root : active_roots) newtm_current_verify (root);
}
void newtm_lto_validate_body (function *body)
{
  if (seen_error ()) return;
  basic_block bb;
  FOR_EACH_BB_FN (bb, body)
    for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
      {
        gimple *s = gsi_stmt (it);
        tree fn = is_gimple_call (s) ? gimple_call_fndecl (s) : NULL_TREE;
        if (fn && fndecl_built_in_p (fn, BUILT_IN_NORMAL)
            && DECL_FUNCTION_CODE (fn) >= BUILT_IN_GOMP_TRANSACTION_STM_V1_BEGIN
            && DECL_FUNCTION_CODE (fn) <= BUILT_IN_GOMP_TRANSACTION_STM_V1_FATAL
            && !newtm_lto_payload (body->decl))
          { lto_refuse (body->decl, "transaction LTO body has no reconstruction contracts", "V08", s); return; }
      }
}

#include "gt-newtm-lto.h"
