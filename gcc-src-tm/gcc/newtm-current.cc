/* Compose current proofs atomically and revoke stale graph results.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-ssa.h"
#include "gimple-iterator.h"
#include "tree-cfg.h"
#include "tree-eh.h"
#include "options.h"
#include "real.h"
#include "fixed-value.h"
#include "diagnostic-core.h"
#include "newtm-clone.h"
#include "newtm-current.h"
#include "newtm-lto.h"

/* A live declaration is sufficient for diagnostic ancestry.  No instruction,
   descriptor or source expression is retained here. */
static tree diagnostic_root;
location_t newtm_current_diagnostic_location (tree fn, gimple *stmt)
{
  location_t loc = stmt ? gimple_location (stmt) : UNKNOWN_LOCATION;
  if (loc == UNKNOWN_LOCATION && fn) loc = DECL_SOURCE_LOCATION (fn);
  return loc;
}
void newtm_current_diagnostic_context (tree fn, gimple *stmt, const char *rule,
                                      const char *operation)
{
  location_t loc = newtm_current_diagnostic_location (fn, stmt);
  if (!operation)
    operation = !stmt ? "proof contract" : is_gimple_call (stmt) ? "call"
      : is_gimple_assign (stmt) ? "assignment"
      : gimple_code (stmt) == GIMPLE_COND ? "branch" : "control";
  if (fn)
    {
      tree original = newtm_clone_original (fn);
      inform (loc, "transaction rule %s, operation %s, function %qD",
              rule, operation, original ? original : fn);
      if (original) inform (loc, "prepared scope %qD", fn);
    }
  if (diagnostic_root)
    {
      tree owner = newtm_clone_region_owner (diagnostic_root);
      inform (loc, "while verifying transaction root %qD", diagnostic_root);
      if (owner) inform (DECL_SOURCE_LOCATION (owner), "transaction controller %qD", owner);
      /* Reconstruct a call path from the current CFGs.  This is diagnostic
         context, not a saved approval or a replay of source expressions. */
      if (fn && fn != diagnostic_root)
        {
          auto_vec<tree> work; auto_vec<unsigned> parent;
          hash_set<tree> seen;
          work.safe_push (diagnostic_root); parent.safe_push (0);
          seen.add (diagnostic_root);
          for (unsigned at = 0; at < work.length (); ++at)
            {
              if (work[at] == fn)
                {
                  auto_vec<unsigned> path;
                  for (unsigned i = at; i; i = parent[i]) path.safe_push (i);
                  while (!path.is_empty ())
                    {
                      unsigned i = path.pop ();
                      inform (loc, "transaction call from %qD to %qD",
                              work[parent[i]], work[i]);
                    }
                  break;
                }
              function *body = DECL_STRUCT_FUNCTION (work[at]);
              if (!body || !body->cfg) continue;
              basic_block bb;
              FOR_EACH_BB_FN (bb, body)
                for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
                  if (is_gimple_call (gsi_stmt (it)))
                    if (tree callee = gimple_call_fndecl (gsi_stmt (it)))
                      if (newtm_clone_original (callee) && !seen.add (callee))
                        { work.safe_push (callee); parent.safe_push (at); }
            }
        }
    }
}
void newtm_current_diagnose (tree fn, gimple *stmt, const char *rule,
                            const char *component, const char *reason,
                            const char *operation)
{
  const char *cause = reason ? reason : "unresolved proof";
  /* Shared analyses can report a more precise rule than their component. */
  char precise[] = "V00";
  if (cause[0] == 'V' && cause[1] == '0'
      && cause[2] >= '1' && cause[2] <= '8' && cause[3] == ':')
    { precise[2] = cause[2]; rule = precise; }
  error_at (newtm_current_diagnostic_location (fn, stmt),
            "transaction transformed %s proof failed: %s", component, cause);
  newtm_current_diagnostic_context (fn, stmt, rule, operation);
}

namespace {
/* Byte equality, not a hash: no collision can authorize a stale result.
   All object references below are scalar local indices plus declaration/type
   UIDs. No instruction, expression, SSA_NAME or basic-block pointer survives. */
struct snapshot
{
  auto_vec<unsigned HOST_WIDE_INT> words;
  auto_vec<tree> objects;
  hash_map<tree, unsigned> indices;
  bool valid = true;
  void add (unsigned HOST_WIDE_INT n)
  { if (words.length () < 4 * 1024 * 1024) words.safe_push (n); else valid = false; }
  void bytes (const void *p, size_t size)
  {
    add (size);
    const unsigned char *b = (const unsigned char *) p;
    for (size_t i = 0; i < size; ++i) add (b[i]);
  }
  void reference (tree t)
  {
    if (unsigned *i = indices.get (t)) { add (*i); return; }
    unsigned id = objects.length () + 1; indices.put (t, id);
    objects.safe_push (t); add (id);
  }
  void value (tree t, unsigned depth = 0)
  {
    if (!valid) return;
    if (!t) { add (0); return; }
    if (depth > 128) { valid = false; return; }
    add (TREE_CODE (t) + 1);
    if (DECL_P (t) || TYPE_P (t) || TREE_CODE (t) == BLOCK)
      { reference (t); return; }
    if (TREE_CODE (t) == IDENTIFIER_NODE)
      { bytes (IDENTIFIER_POINTER (t), IDENTIFIER_LENGTH (t)); return; }
    if (TREE_CODE (t) == ERROR_MARK) return;
    /* Option nodes have only tree_base, not tree_typed. TREE_TYPE would
       interpret an options/cache pointer as an executable tree. */
    if (TREE_CODE (t) == OPTIMIZATION_NODE)
      { bytes (TREE_OPTIMIZATION (t), sizeof (*TREE_OPTIMIZATION (t))); return; }
    if (TREE_CODE (t) == TARGET_OPTION_NODE)
      { bytes (TREE_TARGET_OPTION (t), sizeof (*TREE_TARGET_OPTION (t))); return; }
    if (TREE_CODE (t) == TREE_LIST)
      { value (TREE_PURPOSE (t), depth + 1); value (TREE_VALUE (t), depth + 1);
        value (TREE_CHAIN (t), depth + 1); return; }
    if (TREE_CODE (t) == TREE_VEC)
      { add (TREE_VEC_LENGTH (t));
        for (int i = 0; i < TREE_VEC_LENGTH (t); ++i) value (TREE_VEC_ELT (t, i), depth + 1);
        return; }
    value (TREE_TYPE (t), depth + 1);
    add (TREE_SIDE_EFFECTS (t)); add (TREE_THIS_VOLATILE (t));
    add (TREE_READONLY (t)); add (TREE_CONSTANT (t));
    if (TREE_CODE (t) == MEM_REF || TREE_CODE (t) == BIT_FIELD_REF)
      add (REF_REVERSE_STORAGE_ORDER (t));
    if (CONSTANT_CLASS_P (t) && TREE_CODE (t) != STRING_CST) add (TREE_OVERFLOW (t));
    switch (TREE_CODE (t))
      {
      case SSA_NAME:
        add (SSA_NAME_VERSION (t)); value (SSA_NAME_VAR (t), depth + 1);
        add (SSA_NAME_IS_DEFAULT_DEF (t)); break;
      case INTEGER_CST:
        add (TREE_INT_CST_NUNITS (t));
        for (unsigned i = 0; i < TREE_INT_CST_NUNITS (t); ++i) add (TREE_INT_CST_ELT (t, i));
        break;
      case REAL_CST:
        bytes (TREE_REAL_CST_PTR (t), sizeof (REAL_VALUE_TYPE)); break;
      case FIXED_CST:
        bytes (TREE_FIXED_CST_PTR (t), sizeof (FIXED_VALUE_TYPE)); break;
      case STRING_CST:
        bytes (TREE_STRING_POINTER (t), TREE_STRING_LENGTH (t)); break;
      case RAW_DATA_CST:
        bytes (RAW_DATA_POINTER (t), RAW_DATA_LENGTH (t)); break;
      case COMPLEX_CST:
        value (TREE_REALPART (t), depth + 1); value (TREE_IMAGPART (t), depth + 1); break;
      case VECTOR_CST:
        add (VECTOR_CST_NPATTERNS (t)); add (VECTOR_CST_NELTS_PER_PATTERN (t));
        for (unsigned i = 0; i < vector_cst_encoded_nelts (t); ++i) value (VECTOR_CST_ELT (t, i), depth + 1);
        break;
      case POLY_INT_CST:
        for (unsigned i = 0; i < NUM_POLY_INT_COEFFS; ++i) value (POLY_INT_CST_COEFF (t, i), depth + 1);
        break;
      case CONSTRUCTOR:
        {
          add (CONSTRUCTOR_NO_CLEARING (t)); add (TREE_CLOBBER_P (t));
          if (TREE_CLOBBER_P (t)) add (CLOBBER_KIND (t));
          add (CONSTRUCTOR_NELTS (t)); unsigned HOST_WIDE_INT i; tree index, v;
          FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (t), i, index, v)
            { value (index, depth + 1); value (v, depth + 1); }
          break;
        }
      case OPTIMIZATION_NODE:
        bytes (TREE_OPTIMIZATION (t), sizeof (*TREE_OPTIMIZATION (t))); break;
      case TARGET_OPTION_NODE:
        bytes (TREE_TARGET_OPTION (t), sizeof (*TREE_TARGET_OPTION (t))); break;
      case ERROR_MARK: break;
      default:
        if (!EXPR_P (t))
          {
            if (flag_transaction_debug) fprintf (stderr, "NEWTM snapshot unsupported tree: %s\n", get_tree_code_name (TREE_CODE (t)));
            valid = false; break;
          }
        add (TREE_OPERAND_LENGTH (t));
        for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i) value (TREE_OPERAND (t, i), depth + 1);
        break;
      }
  }
  void chain (tree d)
  {
    auto_vec<tree> list; hash_set<tree> seen;
    for (; d; d = TREE_CODE (d) == BLOCK ? BLOCK_CHAIN (d) : TREE_CHAIN (d))
      { if (seen.add (d)) { valid = false; return; } list.safe_push (d); }
    add (list.length ()); for (tree p : list) value (p);
  }
  void object (tree t)
  {
    add (TREE_CODE (t));
    if (TYPE_P (t))
      {
        add (TYPE_UID (t)); add (TYPE_QUALS (t)); add (TYPE_ALIGN (t));
        add (TYPE_PRECISION_RAW (t)); add (TYPE_UNSIGNED (t)); add (TYPE_MODE_RAW (t));
        add (TYPE_PACKED (t));
        if (AGGREGATE_TYPE_P (t)) add (TYPE_REVERSE_STORAGE_ORDER (t));
        if (TREE_CODE (t) == FIXED_POINT_TYPE) add (TYPE_SATURATING (t));
        value (TYPE_SIZE (t)); value (TYPE_SIZE_UNIT (t)); value (TYPE_ATTRIBUTES (t));
        value (TYPE_MAIN_VARIANT (t));
        switch (TREE_CODE (t))
          {
          case RECORD_TYPE: case UNION_TYPE: case QUAL_UNION_TYPE: chain (TYPE_FIELDS (t)); break;
          case FUNCTION_TYPE: case METHOD_TYPE:
            value (TREE_TYPE (t)); value (TYPE_ARG_TYPES (t));
            if (TREE_CODE (t) == METHOD_TYPE) value (TYPE_METHOD_BASETYPE (t));
            break;
          case ARRAY_TYPE: value (TREE_TYPE (t)); value (TYPE_DOMAIN (t)); break;
          case INTEGER_TYPE: case ENUMERAL_TYPE: case BOOLEAN_TYPE:
            value (TYPE_MIN_VALUE (t)); value (TYPE_MAX_VALUE (t)); break;
          default: value (TREE_TYPE (t)); break;
          }
        return;
      }
    if (TREE_CODE (t) == BLOCK)
      {
        value (BLOCK_SUPERCONTEXT (t)); chain (BLOCK_VARS (t));
        chain (BLOCK_SUBBLOCKS (t)); return;
      }
    add (DECL_UID (t)); value (DECL_NAME (t));
    switch (TREE_CODE (t))
      {
      case VAR_DECL: case PARM_DECL: case RESULT_DECL: case FIELD_DECL:
      case FUNCTION_DECL: case TYPE_DECL: case CONST_DECL: case LABEL_DECL: break;
      default: return; /* Namespace/translation-unit identity, no storage. */
      }
    value (TREE_TYPE (t)); value (DECL_CONTEXT (t));
    add (TREE_STATIC (t)); add (TREE_PUBLIC (t)); add (TREE_ADDRESSABLE (t));
    add (TREE_READONLY (t)); add (TREE_THIS_VOLATILE (t));
    add (DECL_EXTERNAL (t)); add (DECL_ALIGN (t));
    add (DECL_NONLOCAL (t));
    value (DECL_SIZE (t)); value (DECL_SIZE_UNIT (t)); value (DECL_ATTRIBUTES (t));
    if ((VAR_P (t) && (TREE_STATIC (t) || DECL_EXTERNAL (t))) || TREE_CODE (t) == CONST_DECL)
      value (DECL_INITIAL (t));
    if (TREE_CODE (t) == FUNCTION_DECL)
      {
        chain (DECL_ARGUMENTS (t)); value (DECL_RESULT (t));
        add (TREE_NOTHROW (t)); add (DECL_STATIC_CHAIN (t));
        add (DECL_PURE_P (t));
        add (DECL_BUILT_IN_CLASS (t));
        if (fndecl_built_in_p (t)) add (DECL_FUNCTION_CODE (t));
        value (DECL_FUNCTION_SPECIFIC_TARGET (t)); value (DECL_FUNCTION_SPECIFIC_OPTIMIZATION (t));
        value (DECL_FUNCTION_PERSONALITY (t));
      }
    if (TREE_CODE (t) == FIELD_DECL)
      { value (DECL_FIELD_OFFSET (t)); value (DECL_FIELD_BIT_OFFSET (t)); add (DECL_NONADDRESSABLE_P (t)); }
    if (TREE_CODE (t) == LABEL_DECL) add (LABEL_DECL_UID (t));
    if (TREE_CODE (t) == PARM_DECL) value (DECL_ARG_TYPE (t));
    if (VAR_P (t) || TREE_CODE (t) == PARM_DECL || TREE_CODE (t) == RESULT_DECL) add (DECL_BY_REFERENCE (t));
    if (VAR_P (t)) add (DECL_HARD_REGISTER (t));
    if (VAR_P (t) || TREE_CODE (t) == PARM_DECL || TREE_CODE (t) == RESULT_DECL)
      {
        add (DECL_HAS_VALUE_EXPR_P (t));
        if (DECL_HAS_VALUE_EXPR_P (t)) value (DECL_VALUE_EXPR (t));
      }
    if (VAR_P (t) || TREE_CODE (t) == FUNCTION_DECL)
      value (DECL_ASSEMBLER_NAME_SET_P (t) ? DECL_ASSEMBLER_NAME_RAW (t) : NULL_TREE);
  }
  tree function (tree fn)
  {
    struct function *body = fn ? DECL_STRUCT_FUNCTION (fn) : NULL;
    if (!body || !body->cfg) return NULL_TREE;
    value (fn); value (body->static_chain_decl);
    basic_block bb;
    add (last_basic_block_for_fn (body));
    FOR_ALL_BB_FN (bb, body)
      {
        add (bb->index);
        for (gphi_iterator it = gsi_start_phis (bb); !gsi_end_p (it); gsi_next (&it))
          {
            gphi *phi = it.phi (); add (1); value (gimple_phi_result (phi));
            add (gimple_phi_num_args (phi));
            for (unsigned i = 0; i < gimple_phi_num_args (phi); ++i)
              { value (gimple_phi_arg_def (phi, i)); add (gimple_phi_arg_edge (phi, i)->src->index); }
          }
        add (0);
        if (bb != ENTRY_BLOCK_PTR_FOR_FN (body) && bb != EXIT_BLOCK_PTR_FOR_FN (body))
          for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
            {
              gimple *s = gsi_stmt (it);
              if (is_gimple_debug (s)) continue;
              add (1); add (gimple_code (s)); add (s->subcode);
              add (gimple_has_volatile_ops (s)); add (gimple_has_side_effects (s));
              value (gimple_block (s)); add (gimple_num_ops (s));
              if (gimple_has_mem_ops (s)) { value (gimple_vuse (s)); value (gimple_vdef (s)); }
              add (lookup_stmt_eh_lp_fn (body, s));
              if (gimple_code (s) == GIMPLE_RESX) add (gimple_resx_region (as_a<gresx *> (s)));
              if (gimple_code (s) == GIMPLE_EH_DISPATCH) add (gimple_eh_dispatch_region (as_a<geh_dispatch *> (s)));
              for (unsigned i = 0; i < gimple_num_ops (s); ++i) value (gimple_op (s, i));
              if (is_gimple_call (s))
                {
                  add (gimple_call_flags (s));
                  if (gimple_call_internal_p (s)) add (gimple_call_internal_fn (s));
                  else { value (gimple_call_chain (s)); value (gimple_call_fntype (s)); }
                }
              if (gimple_code (s) == GIMPLE_ASM)
                { const char *str = gimple_asm_string (as_a<gasm *> (s)); bytes (str, strlen (str)); }
            }
        add (0); add (EDGE_COUNT (bb->succs)); edge e; edge_iterator ei;
        FOR_EACH_EDGE (e, ei, bb->succs) { add (e->dest->index); add (e->flags); }
      }
    add (vec_safe_length (body->cfg->x_label_to_block_map));
    for (unsigned i = 0; i < vec_safe_length (body->cfg->x_label_to_block_map); ++i)
      { basic_block b = (*body->cfg->x_label_to_block_map)[i]; add (b ? b->index + 1 : 0); }
    for (unsigned i = 0; valid && i < objects.length (); ++i) object (objects[i]);
    if (!valid || words.length () > INT_MAX / sizeof (unsigned HOST_WIDE_INT)) return NULL_TREE;
    return build_string (words.length () * sizeof (unsigned HOST_WIDE_INT), (const char *) words.address ());
  }
};
enum field { ROOT, CONTROLLER, FRAMES, STATE, REASON, FIELDS };
static bool equal (tree a, tree b)
{ return a && b && TREE_CODE (a) == STRING_CST && TREE_CODE (b) == STRING_CST
    && TREE_STRING_LENGTH (a) == TREE_STRING_LENGTH (b)
    && !memcmp (TREE_STRING_POINTER (a), TREE_STRING_POINTER (b), TREE_STRING_LENGTH (a)); }
}
static GTY(()) vec<tree, va_gc> *current_results;
static bool frontend_closed;
static hash_set<tree> *checking_components;
static unsigned HOST_WIDE_INT invalidation_epoch;
tree newtm_current_ir (tree fn)
{ if (tree image = newtm_lto_image (fn)) return image; snapshot s; return s.function (fn); }
unsigned HOST_WIDE_INT newtm_current_count () { return vec_safe_length (current_results); }
void newtm_current_invalidate (tree fn)
{
  ++invalidation_epoch;
  for (unsigned i = 0; i < vec_safe_length (current_results); ++i)
    {
      tree g = (*current_results)[i];
      if (tree_to_uhwi (TREE_VEC_ELT (g, STATE)) != NEWTM_CURRENT_APPROVED) continue;
      bool affected = !fn || TREE_VEC_ELT (g, ROOT) == fn;
      for (tree p = TREE_VEC_ELT (g, FRAMES); !affected && p; p = TREE_CHAIN (p))
        affected = TREE_VEC_ELT (TREE_VALUE (p), 0) == fn;
      if (affected)
        { TREE_VEC_ELT (g, STATE) = build_int_cst (unsigned_type_node, NEWTM_CURRENT_INVALID);
          TREE_VEC_ELT (g, REASON) = build_string (sizeof ("current graph invalidated"), "current graph invalidated"); }
    }
}
void newtm_current_component_begin (tree fn)
{
  if (checking_components) checking_components->add (fn);
  else newtm_current_invalidate (fn);
}
void newtm_current_end_frontend ()
{ newtm_lto_prepare (); newtm_current_invalidate (NULL_TREE); frontend_closed = true; newtm_lto_finish (); }
bool newtm_current_verify (tree root)
{
  if (!root || TREE_CODE (root) != FUNCTION_DECL || checking_components) return false;
  diagnostic_root = root;
  newtm_current_invalidate (root);
  unsigned HOST_WIDE_INT epoch = invalidation_epoch;
  hash_set<tree> touched;
  checking_components = &touched;
  tree record = make_tree_vec (FIELDS), frames = NULL_TREE, controller = NULL_TREE;
  TREE_VEC_ELT (record, ROOT) = root;
  bool valid = (!in_lto_p || newtm_lto_window_p ()) && !frontend_closed && !seen_error ();
  if (valid && in_lto_p) valid = newtm_lto_projection_begin ();
  if (valid) valid = newtm_clone_verify_current (root, &frames, &controller);
  if (flag_transaction_debug && in_lto_p)
    fprintf (stderr, "NEWTM LTO staged valid=%u frames=%u\n", valid, list_length (frames));
  if (in_lto_p) newtm_lto_projection_end ();
  unsigned proved = 0, pending = 0;
  hash_set<tree> functions;
  for (tree p = frames; valid && p; p = TREE_CHAIN (p))
    {
      tree f = TREE_VALUE (p), fn = TREE_VEC_ELT (f, 0);
      unsigned mask = tree_to_uhwi (TREE_VEC_ELT (f, 1));
      unsigned required = newtm_clone_original (fn)
        ? NEWTM_CURRENT_ALL & ~NEWTM_CURRENT_OWNER
        : fn == controller ? NEWTM_CURRENT_ALL : 0;
      valid = !functions.add (fn) && mask == required
        && equal (TREE_VEC_ELT (f, 3), newtm_current_ir (fn));
      if (!valid && flag_transaction_debug)
        fprintf (stderr, "NEWTM current frame freshness failed: %s mask=%u required=%u\n", IDENTIFIER_POINTER (DECL_NAME (fn)), mask, required);
      proved |= mask; pending |= tree_to_uhwi (TREE_VEC_ELT (f, 2));
    }
  valid &= controller && frames && functions.contains (root) && functions.contains (controller)
    && proved == NEWTM_CURRENT_ALL && !(pending & ~proved) && !seen_error ()
    && epoch == invalidation_epoch;
  checking_components = NULL;
  if (!valid)
    for (tree fn : touched) newtm_current_invalidate (fn);
  else
    for (tree p = frames; p; p = TREE_CHAIN (p))
      {
        tree f = TREE_VALUE (p), fn = TREE_VEC_ELT (f, 0);
        bool stale = false;
        for (unsigned i = 0; i < vec_safe_length (current_results); ++i)
          {
            tree old = (*current_results)[i];
            if (tree_to_uhwi (TREE_VEC_ELT (old, STATE)) != NEWTM_CURRENT_APPROVED) continue;
            for (tree q = TREE_VEC_ELT (old, FRAMES); q; q = TREE_CHAIN (q))
              if (TREE_VEC_ELT (TREE_VALUE (q), 0) == fn
                  && !equal (TREE_VEC_ELT (TREE_VALUE (q), 3), TREE_VEC_ELT (f, 3))) stale = true;
          }
        if (stale) newtm_current_invalidate (fn);
      }
  TREE_VEC_ELT (record, CONTROLLER) = controller;
  TREE_VEC_ELT (record, STATE) = build_int_cst (unsigned_type_node, valid ? NEWTM_CURRENT_APPROVED : NEWTM_CURRENT_REFUSED);
  if (valid) TREE_VEC_ELT (record, FRAMES) = frames;
  else TREE_VEC_ELT (record, REASON) = build_string (sizeof ("current graph composition refused"), "current graph composition refused");
  vec_safe_push (current_results, record);
  if (!valid && !seen_error ())
    {
      error_at (DECL_SOURCE_LOCATION (root), "transaction current graph composition refused");
      inform (DECL_SOURCE_LOCATION (root),
              "transaction rule V08, operation graph composition, function %qD", root);
    }
  diagnostic_root = NULL_TREE;
  return valid;
}
bool newtm_current_get (unsigned HOST_WIDE_INT id, newtm_current_view *out)
{
  if (!out || !id || id > vec_safe_length (current_results)) return false;
  tree g = (*current_results)[id - 1];
  if (tree_to_uhwi (TREE_VEC_ELT (g, STATE)) == NEWTM_CURRENT_APPROVED)
    {
      if ((in_lto_p && !newtm_lto_window_p ()) || frontend_closed) newtm_current_invalidate (NULL_TREE);
      else for (tree p = TREE_VEC_ELT (g, FRAMES); p; p = TREE_CHAIN (p))
        {
          tree f = TREE_VALUE (p), fn = TREE_VEC_ELT (f, 0);
          if (!equal (TREE_VEC_ELT (f, 3), newtm_current_ir (fn)))
            newtm_current_invalidate (fn);
        }
    }
  out->id = id; out->root = TREE_VEC_ELT (g, ROOT); out->controller = TREE_VEC_ELT (g, CONTROLLER);
  out->state = (newtm_current_state) tree_to_uhwi (TREE_VEC_ELT (g, STATE));
  out->current_approved = out->state == NEWTM_CURRENT_APPROVED;
  out->members = list_length (TREE_VEC_ELT (g, FRAMES));
  out->required = NEWTM_CURRENT_ALL; out->proved = out->current_approved ? NEWTM_CURRENT_ALL : 0;
  out->dependencies = out->current_approved ? 0 : NEWTM_CURRENT_ALL;
  tree reason = TREE_VEC_ELT (g, REASON); out->reason = reason ? TREE_STRING_POINTER (reason) : NULL;
  return true;
}
bool newtm_current_member (unsigned HOST_WIDE_INT id, unsigned index, tree *fn, unsigned *mask)
{
  if (!id || id > vec_safe_length (current_results) || !fn || !mask) return false;
  newtm_current_view view;
  if (!newtm_current_get (id, &view)) return false;
  tree p = TREE_VEC_ELT ((*current_results)[id - 1], FRAMES);
  while (p && index--) p = TREE_CHAIN (p);
  if (!p) return false;
  *fn = TREE_VEC_ELT (TREE_VALUE (p), 0);
  *mask = view.current_approved ? tree_to_uhwi (TREE_VEC_ELT (TREE_VALUE (p), 1)) : 0; return true;
}
#include "gt-newtm-current.h"
