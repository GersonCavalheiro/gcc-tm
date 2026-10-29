/* Materialize checked identities from evaluated High-GIMPLE operands.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "gimplify.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "internal-fn.h"
#include "tree-iterator.h"
#include "newtm-occurrence.h"
#include "newtm-descriptor.h"
#include "ggc.h"

static GTY(()) hash_map<tree, tree> *newtm_registration_types;
static GTY(()) hash_map<tree, tree> *newtm_pointer_effects;
static GTY(()) hash_map<gimple *, tree> *newtm_descriptors;

void
newtm_descriptor_note_registration (tree call, tree type)
{
  if (!newtm_registration_types)
    newtm_registration_types = hash_map<tree, tree>::create_ggc ();
  newtm_registration_types->put (call, type);
}

tree
newtm_descriptor_registration_type (tree call)
{
  tree *type = newtm_registration_types ? newtm_registration_types->get (call) : NULL;
  tree result = type ? *type : NULL_TREE;
  if (type) newtm_registration_types->remove (call);
  return result;
}

/* Conservative same-unit summary: direct local/by-value pointer assignments
   do not modify a caller's pointer storage; indirect/global stores can. */
struct effect_scan { tree fn; hash_set<tree> *visited; bool effect; };
static bool pointer_effect (tree, hash_set<tree> &);
static tree
scan_effect (tree *tp, int *walk, void *data)
{
  effect_scan &s = *(effect_scan *) data;
  tree t = *tp;
  if (DECL_P (t) || TYPE_P (t)) { *walk = 0; return NULL_TREE; }
  if (TREE_CODE (t) == CALL_EXPR)
    {
      tree fn = get_callee_fndecl (t);
      if (!fn || pointer_effect (fn, *s.visited)) s.effect = true;
    }
  if (TREE_CODE (t) == MODIFY_EXPR || TREE_CODE (t) == INIT_EXPR
      || TREE_CODE (t) == PREINCREMENT_EXPR || TREE_CODE (t) == POSTINCREMENT_EXPR
      || TREE_CODE (t) == PREDECREMENT_EXPR || TREE_CODE (t) == POSTDECREMENT_EXPR)
    {
      tree lhs = TREE_OPERAND (t, 0);
      if (TREE_TYPE (lhs) && POINTER_TYPE_P (TREE_TYPE (lhs))
          && (!DECL_P (lhs) || DECL_CONTEXT (lhs) != s.fn
              || TREE_STATIC (lhs) || TREE_CODE (TREE_TYPE (lhs)) == REFERENCE_TYPE))
        s.effect = true;
    }
  return NULL_TREE;
}
static bool
pointer_effect (tree fn, hash_set<tree> &visited)
{
  if (visited.add (fn)) return false;
  if (!DECL_SAVED_TREE (fn) || DECL_BUILT_IN_CLASS (fn) != NOT_BUILT_IN) return true;
  effect_scan scan = { fn, &visited, false };
  tree body = DECL_SAVED_TREE (fn);
  walk_tree_without_duplicates (&body, scan_effect, &scan);
  return scan.effect;
}
void
newtm_descriptor_note_function (tree fn)
{
  if (!newtm_pointer_effects) newtm_pointer_effects = hash_map<tree, tree>::create_ggc ();
  if (newtm_pointer_effects->get (fn)) return;
  hash_set<tree> visited;
  newtm_pointer_effects->put (fn, pointer_effect (fn, visited)
                            ? boolean_true_node : boolean_false_node);
}

tree
newtm_descriptor_entries (const gimple *stmt)
{
  tree *v = newtm_descriptors ? newtm_descriptors->get (const_cast<gimple *> (stmt)) : NULL;
  return v ? *v : NULL_TREE;
}
tree
newtm_descriptor_find (const gimple *stmt, newtm_descriptor_role role, unsigned index)
{
  for (tree p = newtm_descriptor_entries (stmt); p; p = TREE_CHAIN (p))
    if (tree_to_uhwi (TREE_VEC_ELT (TREE_PURPOSE (p), 0)) == (unsigned) role
        && tree_to_uhwi (TREE_VEC_ELT (TREE_PURPOSE (p), 1)) == index)
      return TREE_VALUE (p);
  return NULL_TREE;
}

namespace {
struct emitter
{
  gimple_seq seq = NULL;
  location_t loc;
  gbind *scope;
  emitter (location_t l, gbind *b) : loc (l), scope (b) {}
  void add (gimple *s) { gimple_set_location (s, loc); gimple_seq_add_stmt_without_update (&seq, s); }
  tree temp (tree type, const char *name)
  {
    tree t = create_tmp_var (type, name);
    gcc_assert (scope);
    DECL_CHAIN (t) = gimple_bind_vars (scope);
    gimple_bind_set_vars (scope, t);
    return t;
  }
  tree copy (tree value, const char *name)
  {
    tree t = temp (TREE_TYPE (value), name);
    add (gimple_build_assign (t, value));
    return t;
  }
  tree cast (tree type, tree value)
  {
    if (type == TREE_TYPE (value)) return value;
    tree converted = fold_convert (type, value);
    if (is_gimple_val (converted)) return converted;
    tree t = temp (type, "newtm_convert");
    add (gimple_build_assign (t, converted));
    return t;
  }
  tree binary (enum tree_code code, tree type, tree a, tree b)
  {
    tree folded = fold_build2 (code, type, a, b);
    if (is_gimple_val (folded)) return folded;
    tree t = temp (type, "newtm_check");
    add (gimple_build_assign (t, code, a, b));
    return t;
  }
  tree both (tree a, tree b) { return binary (BIT_AND_EXPR, boolean_type_node, a, b); }
  tree compare (enum tree_code code, tree a, tree b)
  { return binary (code, boolean_type_node, a, b); }
  tree checked_into (enum tree_code code, tree type, tree a, tree b, tree &valid)
  {
    if (TREE_CODE (a) == INTEGER_CST && TREE_CODE (b) == INTEGER_CST)
      {
        if (arith_overflowed_p (code, type, a, b)) valid = boolean_false_node;
        tree result = fold_build2 (code, type, fold_convert (type, a), fold_convert (type, b));
        return TREE_OVERFLOW (result) ? drop_tree_overflow (result) : result;
      }
    tree pair = temp (build_complex_type (type), "newtm_arithmetic");
    internal_fn fn = code == MULT_EXPR ? IFN_MUL_OVERFLOW
      : code == PLUS_EXPR ? IFN_ADD_OVERFLOW : IFN_SUB_OVERFLOW;
    gcall *call = gimple_build_call_internal (fn, 2, a, b);
    gimple_call_set_lhs (call, pair); add (call);
    tree result = temp (type, "newtm_offset");
    tree overflow = temp (type, "newtm_overflow");
    add (gimple_build_assign (result, build1 (REALPART_EXPR, type, pair)));
    add (gimple_build_assign (overflow, build1 (IMAGPART_EXPR, type, pair)));
    valid = both (valid, compare (EQ_EXPR, overflow, build_zero_cst (type)));
    return result;
  }
  tree checked (enum tree_code code, tree a, tree b, tree &valid)
  { return checked_into (code, size_type_node, cast (size_type_node, a),
                         cast (size_type_node, b), valid); }
  tree index (tree value, tree &valid)
  {
    /* Check in the original evaluated type, BEFORE narrowing to size_t. */
    tree type = TREE_TYPE (value);
    if (!INTEGRAL_TYPE_P (type)) { valid = boolean_false_node; return size_zero_node; }
    if (!TYPE_UNSIGNED (type))
      valid = both (valid, compare (GE_EXPR, value, build_zero_cst (type)));
    if (TYPE_PRECISION (type) > TYPE_PRECISION (size_type_node))
      valid = both (valid, compare (LE_EXPR, value,
                                    fold_convert (type, TYPE_MAX_VALUE (size_type_node))));
    return cast (size_type_node, value);
  }
};
struct identity
{
  tree root = NULL_TREE, object = NULL_TREE, extent = NULL_TREE;
  tree offset = NULL_TREE, valid = NULL_TREE, kind = NULL_TREE, channel = NULL_TREE;
  newtm_descriptor_state state = NEWTM_DESC_UNKNOWN;
  bool stable = false;
};
struct registration { identity value; bool stable; };
struct analysis
{
  tree fn;
  gbind *scope = NULL;
  bool candidate, control = false, context_control = false;
  unsigned depth = 0;
  hash_map<tree, identity> pointers;
  hash_map<tree, gimple *> definitions;
  hash_map<tree, unsigned> writes;
  hash_set<tree> private_objects;
  auto_vec<registration> registered;
  analysis (tree f) : fn (f), candidate (false) {}
};
static identity pointer (tree, analysis &, emitter &, unsigned = 0);
static identity memory (tree, analysis &, emitter &, bool = false, unsigned = 0);
static tree sz (tree t) { return fold_convert (size_type_node, t); }
static tree zero () { return build_zero_cst (size_type_node); }
static bool concrete (const identity &id)
{ return id.object && id.extent && id.kind; }

static identity
object (tree decl, analysis &s)
{
  identity id;
  tree type = TREE_TYPE (decl);
  if (!type || !TYPE_SIZE_UNIT (type)
      || TREE_CODE (TYPE_SIZE_UNIT (type)) != INTEGER_CST) return id;
  id.root = decl;
  /* Provenance from existing memory operands/registered objects. Do not mark
     additional program variables addressable after gimplification. */
  id.object = fold_convert (ptr_type_node, build1 (ADDR_EXPR, build_pointer_type (type), decl));
  id.extent = sz (TYPE_SIZE_UNIT (type)); id.offset = zero ();
  id.kind = build_int_cst (uint32_type_node,
                           TREE_CODE (type) == ARRAY_TYPE || TREE_CODE (type) == RECORD_TYPE);
  id.valid = boolean_true_node;
  id.state = s.private_objects.contains (decl) ? NEWTM_DESC_PRIVATE : NEWTM_DESC_UNREGISTERED;
  return id;
}

static identity
memory (tree ref, analysis &s, emitter &e, bool one_past, unsigned depth)
{
  if (!ref || depth > 64) return identity ();
  switch (TREE_CODE (ref))
    {
    case VAR_DECL: case PARM_DECL: return object (ref, s);
    case COMPONENT_REF:
      {
        identity id = memory (TREE_OPERAND (ref, 0), s, e, false, depth + 1);
        if (!id.root || DECL_BIT_FIELD (TREE_OPERAND (ref, 1))) return identity ();
        id.offset = e.checked (PLUS_EXPR, id.offset, byte_position (TREE_OPERAND (ref, 1)), id.valid);
        return id;
      }
    case ARRAY_REF:
      {
        identity id = memory (TREE_OPERAND (ref, 0), s, e, false, depth + 1);
        tree type = TREE_TYPE (TREE_OPERAND (ref, 0));
        tree elem = TYPE_SIZE_UNIT (TREE_TYPE (ref));
        if (!id.root || !elem || TREE_CODE (elem) != INTEGER_CST
            || TREE_CODE (type) != ARRAY_TYPE || !TYPE_SIZE_UNIT (type)
            || !integer_zerop (array_ref_low_bound (ref)) || integer_zerop (elem))
          return identity ();
        tree index = e.index (TREE_OPERAND (ref, 1), id.valid);
        tree count = fold_build2 (TRUNC_DIV_EXPR, size_type_node, sz (TYPE_SIZE_UNIT (type)), sz (elem));
        id.valid = e.both (id.valid, e.compare (one_past ? LE_EXPR : LT_EXPR, index, count));
        tree delta = e.checked (MULT_EXPR, index, elem, id.valid);
        id.offset = e.checked (PLUS_EXPR, id.offset, delta, id.valid);
        return id;
      }
    case INDIRECT_REF: case MEM_REF:
      {
        identity id = pointer (TREE_OPERAND (ref, 0), s, e, depth + 1);
        if (id.root && TREE_CODE (ref) == MEM_REF)
          id.offset = e.checked (PLUS_EXPR, id.offset, sz (TREE_OPERAND (ref, 1)), id.valid);
        return id;
      }
    default: return identity ();
    }
}

/* Recover compiler byte scaling from evaluated operands. Overflow intrinsics
   retain signed deltas; no source expression or logical address is reevaluated. */
static tree
delta (tree value, analysis &s, emitter &e, tree &valid, unsigned depth)
{
  if (depth > 16) return NULL_TREE;
  gimple **found = s.definitions.get (value);
  if (found && is_gimple_assign (*found))
    {
      gimple *def = *found;
      enum tree_code code = gimple_assign_rhs_code (def);
      if (code == MULT_EXPR && TREE_CODE (gimple_assign_rhs2 (def)) == INTEGER_CST)
        {
          tree index = gimple_assign_rhs1 (def);
          gimple **conversion = s.definitions.get (index);
          if (conversion && gimple_assign_cast_p (*conversion)
              && INTEGRAL_TYPE_P (TREE_TYPE (gimple_assign_rhs1 (*conversion))))
            index = gimple_assign_rhs1 (*conversion);
          tree type = TYPE_UNSIGNED (TREE_TYPE (index)) ? size_type_node : ptrdiff_type_node;
          return e.checked_into (MULT_EXPR, type, index,
                                 gimple_assign_rhs2 (def), valid);
        }
      if (gimple_assign_cast_p (def)
          && INTEGRAL_TYPE_P (TREE_TYPE (gimple_assign_rhs1 (def))))
        return gimple_assign_rhs1 (def);
      if (code == NEGATE_EXPR)
        {
          tree v = delta (gimple_assign_rhs1 (def), s, e, valid, depth + 1);
          if (!v) return NULL_TREE;
          return e.checked_into (MINUS_EXPR, ptrdiff_type_node,
                                 build_zero_cst (ptrdiff_type_node), v, valid);
        }
    }
  if (!TYPE_UNSIGNED (TREE_TYPE (value))) return value;
  /* A folded modular constant no longer distinguishes p-1 from an enormous
     unsigned advance. Keep it unresolved instead of manufacturing a proof. */
  if (TREE_CODE (value) == INTEGER_CST
      && tree_int_cst_lt (fold_convert (TREE_TYPE (value), TYPE_MAX_VALUE (ptrdiff_type_node)), value))
    return NULL_TREE;
  tree result = e.index (value, valid);
  tree max = fold_convert (size_type_node, TYPE_MAX_VALUE (ptrdiff_type_node));
  valid = e.both (valid, e.compare (LE_EXPR, result, max));
  return result;
}

static identity
pointer (tree value, analysis &s, emitter &e, unsigned depth)
{
  if (!value || depth > 64) return identity ();
  STRIP_NOPS (value);
  if (TREE_CODE (value) == ADDR_EXPR)
    return memory (TREE_OPERAND (value, 0), s, e, true, depth + 1);
  identity *known = s.pointers.get (value);
  if (known) return *known;
  if (VAR_P (value) && TYPE_READONLY (TREE_TYPE (value)) && DECL_INITIAL (value))
    return pointer (DECL_INITIAL (value), s, e, depth + 1);
  return identity ();
}

static identity
select_registration (identity id, tree size, analysis &s, emitter &e)
{
  if (!id.root) return id;
  const registration *choice = NULL;
  for (const registration &r : s.registered)
    if (r.value.root == id.root)
      {
        if (TREE_CODE (id.offset) == INTEGER_CST && TREE_CODE (r.value.offset) == INTEGER_CST
            && TREE_CODE (r.value.extent) == INTEGER_CST)
          {
            if (tree_int_cst_lt (id.offset, r.value.offset)) continue;
            tree relative = fold_build2 (MINUS_EXPR, size_type_node, id.offset, r.value.offset);
            if (tree_int_cst_lt (r.value.extent, relative)) continue;
            tree remaining = fold_build2 (MINUS_EXPR, size_type_node, r.value.extent, relative);
            if (size && TREE_CODE (size) == INTEGER_CST && tree_int_cst_lt (remaining, sz (size))) continue;
          }
        if (choice) return identity (); /* No arbitrary choice of a subobject. */
        choice = &r;
      }
  if (choice)
    {
      id.valid = e.both (id.valid, choice->value.valid);
      id.offset = e.checked (MINUS_EXPR, id.offset, choice->value.offset, id.valid);
      id.object = choice->value.object; id.extent = choice->value.extent;
      id.kind = choice->value.kind; id.state = NEWTM_DESC_REGISTERED;
    }
  return id;
}

static tree
range_valid (identity &id, tree size, emitter &e)
{
  if (!concrete (id)) return NULL_TREE;
  tree extent = sz (id.extent), offset = sz (id.offset);
  tree valid = e.both (id.valid, e.compare (NE_EXPR, extent, zero ()));
  valid = e.both (valid, e.compare (NE_EXPR, id.object, fold_convert (TREE_TYPE (id.object), null_pointer_node)));
  /* Check the last byte, without forming object+extent or extent+offset. */
  tree ptruint = unsigned_type_for (pointer_sized_int_node);
  tree wide = build_nonstandard_integer_type (MAX (TYPE_PRECISION (size_type_node), TYPE_PRECISION (ptruint)), true);
  tree address = e.cast (wide, e.cast (ptruint, id.object));
  tree address_left = e.binary (MINUS_EXPR, wide,
                               fold_convert (wide, TYPE_MAX_VALUE (ptruint)), address);
  tree nonzero_extent = e.binary (MAX_EXPR, size_type_node, extent, sz (size_one_node));
  tree span = e.binary (MINUS_EXPR, size_type_node, nonzero_extent, sz (size_one_node));
  valid = e.both (valid, e.compare (LE_EXPR, e.cast (wide, span), address_left));
  valid = e.both (valid, e.compare (LE_EXPR, offset, extent));
  /* Clamp before subtraction: no underflow even on an invalid offset. */
  tree bounded = e.binary (MIN_EXPR, size_type_node, offset, extent);
  tree left = e.binary (MINUS_EXPR, size_type_node, extent, bounded);
  valid = e.both (valid, e.compare (LE_EXPR, sz (size), left));
  if (!integer_zerop (sz (size)))
    {
      tree aggregate = e.compare (NE_EXPR, id.kind, build_zero_cst (TREE_TYPE (id.kind)));
      tree whole = e.both (e.compare (EQ_EXPR, offset, zero ()),
                           e.compare (EQ_EXPR, sz (size), extent));
      valid = e.both (valid, e.binary (BIT_IOR_EXPR, boolean_type_node, aggregate, whole));
    }
  return valid;
}

static void
record (gimple *stmt, newtm_descriptor_role role, unsigned index,
        identity id, tree size, tree logical, analysis &s, emitter &e, bool registration = false)
{
  if (!registration && role != NEWTM_DESC_ADDRESS)
    id = select_registration (id, size, s, e);
  tree d = make_tree_vec (NEWTM_D_FIELDS);
  tree valid = range_valid (id, size, e);
  TREE_VEC_ELT (d, NEWTM_D_STATE) = build_int_cst (unsigned_type_node, id.state);
  TREE_VEC_ELT (d, NEWTM_D_ORIGIN) = id.root;
  TREE_VEC_ELT (d, NEWTM_D_CHANNEL) = id.channel;
  TREE_VEC_ELT (d, NEWTM_D_KIND_VALUE) = id.kind;
  TREE_VEC_ELT (d, NEWTM_D_EXTENT_VALUE) = id.extent;
  TREE_VEC_ELT (d, NEWTM_D_SIZE) = sz (size);
  TREE_VEC_ELT (d, NEWTM_D_LOGICAL) = logical;
  TREE_VEC_ELT (d, NEWTM_D_LOCAL_VALID) = id.valid;
  if (concrete (id))
    {
      TREE_VEC_ELT (d, NEWTM_D_OBJECT) = e.copy (e.cast (ptr_type_node, id.object), "newtm_object");
      TREE_VEC_ELT (d, NEWTM_D_EXTENT) = e.copy (sz (id.extent), "newtm_extent");
      TREE_VEC_ELT (d, NEWTM_D_OFFSET) = e.copy (sz (id.offset), "newtm_position");
      TREE_VEC_ELT (d, NEWTM_D_KIND) = e.copy (id.kind, "newtm_kind");
      TREE_VEC_ELT (d, NEWTM_D_VALID) = e.copy (valid, "newtm_valid");
    }
  else TREE_VEC_ELT (d, NEWTM_D_OFFSET) = id.offset;
  tree key = make_tree_vec (2);
  TREE_VEC_ELT (key, 0) = build_int_cst (unsigned_type_node, role);
  TREE_VEC_ELT (key, 1) = build_int_cst (unsigned_type_node, index);
  if (!newtm_descriptors) newtm_descriptors = hash_map<gimple *, tree>::create_ggc ();
  tree *previous = newtm_descriptors->get (stmt);
  newtm_descriptors->put (stmt, tree_cons (key, d, previous ? *previous : NULL_TREE));
}

static tree
registration_type (gimple *stmt)
{
  tree b = newtm_occurrence_binding (stmt);
  if (!b) return NULL_TREE;
  for (tree site = TREE_VEC_ELT (b, 10); site; site = TREE_VEC_ELT (site, 1))
    if (TREE_VEC_ELT (site, 7)) return TREE_VEC_ELT (site, 7);
  return NULL_TREE;
}
static tree
count_writes (gimple_stmt_iterator *p, bool *, walk_stmt_info *wi)
{
  analysis &s = *(analysis *) wi->info;
  gimple *stmt = gsi_stmt (*p);
  if (is_gimple_assign (stmt))
    {
      tree lhs = gimple_assign_lhs (stmt);
      unsigned *count = s.writes.get (lhs);
      s.writes.put (lhs, count ? *count + 1 : 1);
    }
  return NULL_TREE;
}
static void
forget_unstable (analysis &s, bool all_mutable)
{
  auto_vec<tree> erased;
  for (auto it = s.pointers.begin (); it != s.pointers.end (); ++it)
    if (all_mutable
        ? TREE_CODE ((*it).first) != SSA_NAME
          && !(DECL_P ((*it).first) && (TYPE_READONLY (TREE_TYPE ((*it).first))
                                       || (DECL_ARTIFICIAL ((*it).first) && !TREE_ADDRESSABLE ((*it).first))))
        : !(*it).second.stable)
      erased.safe_push ((*it).first);
  for (tree t : erased) s.pointers.remove (t);
  if (!all_mutable)
    {
      auto_vec<registration> keep;
      for (registration r : s.registered) if (r.stable) keep.safe_push (r);
      s.registered.truncate (0); s.registered.safe_splice (keep);
    }
}
static void walk_sequence (gimple_seq *, analysis &);
static void
walk_one (gimple *stmt, analysis &s, emitter &e)
{
  tree binding = newtm_occurrence_binding (stmt);
  if (newtm_descriptors) newtm_descriptors->remove (stmt);
  unsigned mode = binding ? tree_to_uhwi (TREE_VEC_ELT (binding, 7)) : 0;
  if (is_gimple_assign (stmt))
    {
      tree lhs = gimple_assign_lhs (stmt);
      if (POINTER_TYPE_P (TREE_TYPE (lhs)))
        {
          identity id;
          enum tree_code code = gimple_assign_rhs_code (stmt);
          if (code == POINTER_PLUS_EXPR)
            {
              id = pointer (gimple_assign_rhs1 (stmt), s, e);
              if (id.root)
                {
                  tree change = delta (gimple_assign_rhs2 (stmt), s, e, id.valid, 0);
                  if (change)
                    id.offset = e.checked_into (PLUS_EXPR, size_type_node, id.offset, change, id.valid);
                  else id = identity ();
                }
              if (binding) record (stmt, NEWTM_DESC_ADDRESS, 0, id, zero (), lhs, s, e);
            }
          else if (gimple_assign_single_p (stmt) || gimple_assign_cast_p (stmt))
            id = pointer (gimple_assign_rhs1 (stmt), s, e);
          unsigned *count = s.writes.get (lhs);
          id.stable = !s.control && (!count || *count == 1);
          s.pointers.put (lhs, id);
        }
      s.definitions.put (lhs, stmt);
      if (mode & 1)
        record (stmt, NEWTM_DESC_READ, 0, memory (gimple_assign_rhs1 (stmt), s, e),
                TYPE_SIZE_UNIT (TREE_TYPE (gimple_assign_rhs1 (stmt))),
                gimple_assign_rhs1 (stmt), s, e);
      if (mode & 2)
        record (stmt, NEWTM_DESC_WRITE, 0, memory (lhs, s, e),
                TYPE_SIZE_UNIT (TREE_TYPE (lhs)), lhs, s, e);
    }
  else if (is_gimple_call (stmt))
    {
      tree fn = gimple_call_fndecl (stmt);
      bool builtin = fn && fndecl_built_in_p (fn, BUILT_IN_NORMAL);
      if (mode & 16)
        {
          tree type = registration_type (stmt);
          identity id = pointer (gimple_call_arg (stmt, 0), s, e);
          /* The listed referent defines its own key/extent/kind, including a
             reference to ONE element of a containing array. */
          if (type && id.root)
            {
              id.object = e.cast (ptr_type_node, gimple_call_arg (stmt, 0));
              id.extent = sz (gimple_call_arg (stmt, 1));
              id.kind = build_int_cst (uint32_type_node,
                  TREE_CODE (type) == ARRAY_TYPE || TREE_CODE (type) == RECORD_TYPE);
              id.state = NEWTM_DESC_REGISTERED;
              identity key = id; key.offset = zero ();
              record (stmt, NEWTM_DESC_REGISTER, 0, key, zero (), gimple_call_arg (stmt, 0), s, e, true);
              bool duplicate = false;
              for (registration r : s.registered)
                if (r.value.root == id.root
                    && operand_equal_p (r.value.offset, id.offset, 0)
                    && operand_equal_p (r.value.extent, id.extent, 0)
                    && operand_equal_p (r.value.kind, id.kind, 0)) duplicate = true;
              if (!duplicate) s.registered.safe_push ({ id, !s.context_control });
            }
          else record (stmt, NEWTM_DESC_REGISTER, 0, identity (), zero (),
                       gimple_call_arg (stmt, 0), s, e, true);
        }
      else if (mode & 8)
        {
          for (unsigned i = 0; i < gimple_call_num_args (stmt); ++i)
            if (POINTER_TYPE_P (TREE_TYPE (gimple_call_arg (stmt, i))))
              record (stmt, NEWTM_DESC_ARGUMENT, i, pointer (gimple_call_arg (stmt, i), s, e),
                      zero (), gimple_call_arg (stmt, i), s, e);
        }
      if (!builtin)
        {
          tree *effect = fn && newtm_pointer_effects ? newtm_pointer_effects->get (fn) : NULL;
          if (!effect || !integer_zerop (*effect)) forget_unstable (s, true);
        }
      tree lhs = gimple_call_lhs (stmt);
      if (lhs && POINTER_TYPE_P (TREE_TYPE (lhs)))
        {
          identity id; id.root = lhs; id.offset = zero (); id.valid = boolean_true_node;
          id.state = NEWTM_DESC_RESULT; id.channel = lhs;
          id.stable = !s.control; s.pointers.put (lhs, id);
        }
    }
  else if (gimple_code (stmt) == GIMPLE_RETURN && binding)
    {
      tree value = gimple_return_retval (as_a<greturn *> (stmt));
      if (value && POINTER_TYPE_P (TREE_TYPE (value)))
        record (stmt, NEWTM_DESC_RETURN, 0, pointer (value, s, e), zero (), value, s, e);
    }
  else if (mode & 32)
    {
      identity id; id.state = NEWTM_DESC_SOURCE_ONLY;
      record (stmt, NEWTM_DESC_SOURCE, 0, id, zero (), NULL_TREE, s, e);
    }
  if (gimple_code (stmt) == GIMPLE_COND || gimple_code (stmt) == GIMPLE_SWITCH
      || gimple_code (stmt) == GIMPLE_LABEL || gimple_code (stmt) == GIMPLE_GOTO)
    {
      s.control = true;
      if (s.depth || s.candidate) s.context_control = true;
      forget_unstable (s, false);
    }
}
static void
walk_sequence (gimple_seq *seq, analysis &s)
{
  for (gimple_stmt_iterator p = gsi_start (*seq); !gsi_end_p (p); gsi_next (&p))
    {
      gimple *stmt = gsi_stmt (p);
      if (gimple_code (stmt) == GIMPLE_BIND)
        {
          if (s.depth || s.candidate)
            for (tree d = gimple_bind_vars (as_a<gbind *> (stmt)); d; d = DECL_CHAIN (d))
              if (VAR_P (d) && !TREE_STATIC (d)) s.private_objects.add (d);
          gbind *previous = s.scope;
          s.scope = as_a<gbind *> (stmt);
          walk_sequence (gimple_bind_body_ptr (s.scope), s);
          s.scope = previous;
        }
      else if (gimple_code (stmt) == GIMPLE_OMP_TRANSACTION)
        {
          bool outer = !s.depth && !s.candidate;
          bool control = s.context_control;
          unsigned registrations = s.registered.length ();
          if (outer) s.context_control = false;
          /* Compiler temporaries must be declared INSIDE the OpenMP region,
             so outlining remaps them along with its ordinary local variables. */
          gbind *scope = gimple_build_bind (NULL_TREE, gimple_omp_body (stmt), NULL_TREE);
          gimple_seq wrapped = NULL;
          gimple_seq_add_stmt_without_update (&wrapped, scope);
          gimple_omp_set_body (stmt, wrapped);
          ++s.depth; walk_sequence (gimple_omp_body_ptr (stmt), s); --s.depth;
          if (outer) { s.registered.truncate (registrations); s.context_control = control; }
        }
      else if (gimple_code (stmt) == GIMPLE_TRY)
        {
          walk_sequence (gimple_try_eval_ptr (as_a<gtry *> (stmt)), s);
          walk_sequence (gimple_try_cleanup_ptr (as_a<gtry *> (stmt)), s);
        }
      else if (gimple_has_substatements (stmt) && is_gimple_omp (stmt))
        walk_sequence (gimple_omp_body_ptr (stmt), s);
      else
        {
          emitter e (gimple_location (stmt), s.scope);
          walk_one (stmt, s, e);
          if (e.seq) gsi_insert_seq_before_without_update (&p, e.seq, GSI_SAME_STMT);
        }
    }
}
} // namespace

static tree
has_binding (gimple_stmt_iterator *p, bool *, walk_stmt_info *wi)
{
  if (newtm_occurrence_binding (gsi_stmt (*p))) *(bool *) wi->info = true;
  return NULL_TREE;
}
void
newtm_descriptor_function (tree fn)
{
  if (!flag_openmp) return;
  bool found = false;
  walk_stmt_info query = {}; query.info = &found;
  walk_gimple_seq (gimple_body (fn), has_binding, NULL, &query);
  if (!found) return;
  analysis s (fn);
  /* A selected candidate's by-value parameters/locals are private to its future
     prepared invocation; pointer/reference VALUES use explicit incoming slots. */
  s.candidate = newtm_pointer_effects && newtm_pointer_effects->get (fn);
  unsigned slot = 0;
  for (tree p = DECL_ARGUMENTS (fn); p; p = DECL_CHAIN (p))
    {
      if (s.candidate && TREE_CODE (TREE_TYPE (p)) != REFERENCE_TYPE) s.private_objects.add (p);
      if (POINTER_TYPE_P (TREE_TYPE (p)))
        {
          identity id; id.root = p; id.offset = zero (); id.valid = boolean_true_node;
          id.state = NEWTM_DESC_INCOMING; id.channel = build_int_cst (unsigned_type_node, slot++);
          id.stable = true; s.pointers.put (p, id);
        }
    }
  walk_stmt_info wi = {}; wi.info = &s;
  walk_gimple_seq (gimple_body (fn), count_writes, NULL, &wi);
  gimple_seq body = gimple_body (fn);
  walk_sequence (&body, s);
  gimple_set_body (fn, body);
}

void
newtm_descriptor_copy (const gimple *original, gimple *copy)
{
  tree entries = newtm_descriptor_entries (original);
  if (!entries) return;
  /* A copy may remap every operand. Old materialized temporaries cannot be
     reused: explicitly invalidate them until the copied body is rebuilt. */
  tree result = NULL_TREE;
  for (tree p = entries; p; p = TREE_CHAIN (p))
    {
      tree d = make_tree_vec (NEWTM_D_FIELDS);
      TREE_VEC_ELT (d, NEWTM_D_STATE) = build_int_cst (unsigned_type_node, NEWTM_DESC_STALE);
      result = tree_cons (TREE_PURPOSE (p), d, result);
    }
  newtm_descriptors->put (copy, result);
}
static tree
retire_descriptor (gimple_stmt_iterator *p, bool *, walk_stmt_info *)
{
  if (newtm_descriptors) newtm_descriptors->remove (gsi_stmt (*p));
  return NULL_TREE;
}
void newtm_descriptor_retire (gimple_seq body)
{
  if (!newtm_descriptors) return;
  walk_stmt_info wi = {};
  walk_gimple_seq (body, retire_descriptor, NULL, &wi);
}
void
newtm_descriptor_reset ()
{
  newtm_descriptors = NULL;
  newtm_registration_types = NULL;
  newtm_pointer_effects = NULL;
}
#include "gt-newtm-descriptor.h"
