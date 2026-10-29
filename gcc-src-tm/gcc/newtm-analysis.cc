/* Subset validation and unevaluated canonical access plans for experimental
   OpenMP transactions.  This does not generate the functional runtime ABI.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "cgraph.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "tree-iterator.h"
#include "langhooks.h"
#include "omp-general.h"
#include "gimplify.h"
#include "ggc.h"
#include "newtm-occurrence.h"

/* C++ folds non-ODR reads and constexpr calls before unit finalization.  Keep
   source bodies independently so folded reads cannot evade registration. */
static GTY(()) hash_map<tree, tree> *newtm_sources;
static GTY(()) hash_map<tree, tree> *newtm_types;
static GTY(()) hash_map<tree, tree> *newtm_initializers;
static GTY(()) hash_map<tree, tree> *newtm_regions;
static GTY(()) hash_map<tree, tree> *newtm_markers;

static void ensure_sources ()
{
  if (newtm_sources) return;
  newtm_sources = hash_map<tree, tree>::create_ggc ();
  newtm_types = hash_map<tree, tree>::create_ggc ();
  newtm_initializers = hash_map<tree, tree>::create_ggc ();
  newtm_regions = hash_map<tree, tree>::create_ggc ();
  newtm_markers = hash_map<tree, tree>::create_ggc ();
}

tree newtm_note_source_read (tree expr, location_t loc)
{
  if (TREE_SIDE_EFFECTS (expr)) return NULL_TREE;
  enum tree_code code = TREE_CODE (expr);
  if (code != VAR_DECL && code != PARM_DECL && code != COMPONENT_REF
      && code != ARRAY_REF && code != INDIRECT_REF && code != MEM_REF)
    return NULL_TREE;
  ensure_sources ();
  if (newtm_sources->get (current_function_decl)) return NULL_TREE;
  tree marker = build0 (DEBUG_BEGIN_STMT, void_type_node);
  SET_EXPR_LOCATION (marker, loc);
  newtm_markers->put (marker, unshare_expr (expr));
  return marker;
}

/* The marker is a semantic read witness, not a recipe to execute again. */
tree newtm_source_read (tree marker)
{
  tree *read = newtm_markers ? newtm_markers->get (marker) : NULL;
  return read ? *read : NULL_TREE;
}

static tree source_body (tree fn)
{
  tree *saved = newtm_sources ? newtm_sources->get (fn) : NULL;
  return saved ? *saved : DECL_SAVED_TREE (fn);
}
static tree source_type (tree t)
{
  tree *saved = newtm_types && DECL_P (t) ? newtm_types->get (t) : NULL;
  return saved ? *saved : TREE_TYPE (t);
}
static tree source_initial (tree t)
{
  tree *saved = newtm_initializers ? newtm_initializers->get (t) : NULL;
  return saved ? *saved : DECL_INITIAL (t);
}

static tree collect_source (tree *tp, int *subtrees, void *argument)
{
  auto_vec<tree> *regions = (auto_vec<tree> *) argument;
  tree t = *tp;
  if (TREE_CODE (t) == OMP_TRANSACTION
      || TREE_CODE (t) == DEBUG_BEGIN_STMT) regions->safe_push (t);
  if (TREE_CODE (t) == DECL_EXPR)
    {
      tree d = DECL_EXPR_DECL (t);
      if (TREE_CODE (d) == VAR_DECL)
        {
          newtm_types->put (d, TREE_TYPE (d));
          newtm_initializers->put (d, unshare_expr (DECL_INITIAL (d)));
        }
    }
  if (DECL_P (t))
    { newtm_types->put (t, TREE_TYPE (t)); *subtrees = 0; }
  if (TYPE_P (t)) *subtrees = 0;
  return NULL_TREE;
}

void newtm_save_source_body (tree fn)
{
  if (!DECL_SAVED_TREE (fn)) return;
  ensure_sources ();
  if (newtm_sources->get (fn)) return;
  auto_vec<tree> original, copied;
  tree body = DECL_SAVED_TREE (fn);
  walk_tree_without_duplicates (&body, collect_source, &original);
  for (tree p = DECL_ARGUMENTS (fn); p; p = DECL_CHAIN (p))
    newtm_types->put (p, TREE_TYPE (p));
  tree copy = unshare_expr (body);
  walk_tree_without_duplicates (&copy, collect_source, &copied);
  gcc_assert (original.length () == copied.length ());
  for (unsigned i = 0; i < copied.length (); ++i)
    {
      if (TREE_CODE (original[i]) == OMP_TRANSACTION)
        newtm_regions->put (copied[i], original[i]);
      else if (tree *read = newtm_markers->get (original[i]))
        newtm_markers->put (copied[i], *read);
    }
  newtm_sources->put (fn, copy);
}

namespace {
struct identity
{
  tree object, offset;
  bool readonly;
  identity (tree o = NULL_TREE, tree d = NULL_TREE, bool r = false)
    : object (o), offset (d), readonly (r) {}
};
struct alias { tree decl; identity value; bool effect = false; };
struct registration
{
  tree decl, extent;
  identity base;
  bool aggregate;
};
struct state
{
  tree function, origin, region, plan, returned;
  bool active;
  auto_vec<registration> registered;
  auto_vec<tree> locals, stack, invalid_accesses;
  auto_vec<unsigned> switches;
  auto_vec<alias> pointers, references;
  state (tree fn) : function (fn), origin (fn), region (NULL_TREE), plan (NULL_TREE),
                   returned (NULL_TREE), active (false) {}
};

static bool member (const auto_vec<tree> &v, tree t)
{
  for (unsigned i = 0; i < v.length (); ++i) if (v[i] == t) return true;
  return false;
}
static location_t location (tree t)
{
  return EXPR_P (t) && EXPR_LOCATION (t) != UNKNOWN_LOCATION
    ? EXPR_LOCATION (t) : input_location;
}
static void reject (tree t, const char *message)
{
  error_at (location (t), "%s", message);
}

/* Do not infer language support from sizeof/triviality.  The C++ class
   subset is intentionally empty, including structs.  Pointer objects do not
   recursively admit their pointees; actual indirect accesses validate those. */
static bool supported_type (tree type)
{
  if (!type || type == error_mark_node) return false;
  if (TREE_CODE (type) == REFERENCE_TYPE) type = TREE_TYPE (type);
  if (TYPE_VOLATILE (type) || TYPE_ATOMIC (type) || !COMPLETE_TYPE_P (type)
      || !TYPE_SIZE_UNIT (type)
      || TREE_CODE (TYPE_SIZE_UNIT (type)) != INTEGER_CST) return false;
  switch (TREE_CODE (type))
    {
    case BOOLEAN_TYPE: case INTEGER_TYPE: case ENUMERAL_TYPE:
    case REAL_TYPE: case POINTER_TYPE: return true;
    case COMPLEX_TYPE: return supported_type (TREE_TYPE (type));
    case ARRAY_TYPE: return TREE_CODE (TREE_TYPE (type)) != POINTER_TYPE
      && supported_type (TREE_TYPE (type));
    case RECORD_TYPE:
      if (strstr (lang_hooks.name, "C++")) return false;
      for (tree field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field))
        if (TREE_CODE (field) == FIELD_DECL
            && (DECL_BIT_FIELD (field) || TREE_CODE (TREE_TYPE (field)) == POINTER_TYPE
                || !supported_type (TREE_TYPE (field))))
          return false;
      return true;
    default: return false;
    }
}

static identity object_identity (tree, state &, unsigned = 0);
static identity pointer_identity (tree, state &, unsigned = 0);
static void examine (tree, state &, bool = false);

static identity lookup (const auto_vec<alias> &v, tree decl)
{
  for (unsigned i = v.length (); i; --i)
    if (v[i-1].decl == decl) return v[i-1].value;
  return identity ();
}
static identity add_offset (identity id, tree offset)
{
  if (id.object)
    id.offset = fold_build2 (PLUS_EXPR, size_type_node, id.offset,
                            fold_convert (size_type_node, offset));
  return id;
}

static identity pointer_identity (tree t, state &s, unsigned depth)
{
  if (!t || depth > 64) return identity ();
  STRIP_NOPS (t);
  switch (TREE_CODE (t))
    {
    case ADDR_EXPR: return object_identity (TREE_OPERAND (t, 0), s, depth + 1);
    case POINTER_PLUS_EXPR:
      return add_offset (pointer_identity (TREE_OPERAND (t, 0), s, depth + 1),
                         TREE_OPERAND (t, 1));
    case VAR_DECL: case PARM_DECL: case CALL_EXPR:
      if (TREE_CODE (t) != CALL_EXPR && TREE_CODE (source_type (t)) == REFERENCE_TYPE)
        return object_identity (t, s, depth + 1);
      {
        identity id = lookup (s.pointers, t);
        if (!id.object && VAR_P (t) && TYPE_READONLY (source_type (t)))
          return pointer_identity (source_initial (t), s, depth + 1);
        return id;
      }
    case PREINCREMENT_EXPR: case POSTINCREMENT_EXPR:
    case PREDECREMENT_EXPR: case POSTDECREMENT_EXPR:
      return lookup (s.pointers, t);
    case SAVE_EXPR: return pointer_identity (TREE_OPERAND (t, 0), s, depth + 1);
    default: return identity ();
    }
}

static identity object_identity (tree t, state &s, unsigned depth)
{
  if (!t || depth > 64) return identity ();
  STRIP_NOPS (t);
  switch (TREE_CODE (t))
    {
    case VAR_DECL: case PARM_DECL:
      if (TREE_CODE (source_type (t)) == REFERENCE_TYPE)
        {
          for (unsigned i = s.references.length (); i; --i)
            if (s.references[i-1].decl == t)
              {
                identity id = s.references[i-1].value;
                id.readonly |= TYPE_READONLY (TREE_TYPE (source_type (t)));
                return id;
              }
          return identity (t, size_zero_node,
                            TYPE_READONLY (TREE_TYPE (source_type (t))));
        }
      return identity (t, size_zero_node, TYPE_READONLY (source_type (t)));
    case COMPONENT_REF:
      if (DECL_BIT_FIELD (TREE_OPERAND (t, 1))) return identity ();
      {
        identity id = add_offset (object_identity (TREE_OPERAND (t, 0), s, depth + 1),
                                  byte_position (TREE_OPERAND (t, 1)));
        id.readonly |= TYPE_READONLY (TREE_TYPE (t));
        return id;
      }
    case ARRAY_REF:
      {
        tree index = fold_build2 (MINUS_EXPR, size_type_node,
                      fold_convert (size_type_node, TREE_OPERAND (t, 1)),
                      fold_convert (size_type_node, array_ref_low_bound (t)));
        tree extent = TYPE_SIZE_UNIT (TREE_TYPE (t));
        if (!extent) return identity ();
        identity id = add_offset (object_identity (TREE_OPERAND (t, 0), s, depth + 1),
                                 fold_build2 (MULT_EXPR, size_type_node, index,
                                              fold_convert (size_type_node, extent)));
        id.readonly |= TYPE_READONLY (TREE_TYPE (t));
        return id;
      }
    case INDIRECT_REF: case MEM_REF:
      {
        identity id = pointer_identity (TREE_OPERAND (t, 0), s, depth + 1);
        return TREE_CODE (t) == MEM_REF
          ? add_offset (id, TREE_OPERAND (t, 1)) : id;
      }
    default: return identity ();
    }
}

static tree value_type (tree t)
{
  tree type = source_type (t);
  return TREE_CODE (type) == REFERENCE_TYPE ? TREE_TYPE (type) : type;
}
static bool same_registration (const registration &a, const registration &b)
{
  return a.base.object == b.base.object && a.aggregate == b.aggregate
    && operand_equal_p (a.base.offset, b.base.offset, 0)
    && operand_equal_p (a.extent, b.extent, 0);
}
static bool member_registration (const auto_vec<registration> &v,
                                 const registration &r)
{
  for (const registration &p : v) if (same_registration (p, r)) return true;
  return false;
}
static const registration *find_registration (identity id, tree size, state &s,
                                              tree *relative)
{
  for (const registration &r : s.registered)
    if (r.base.object == id.object)
      {
        tree delta = id.offset == r.base.offset
          || operand_equal_p (id.offset, r.base.offset, 0)
          ? size_zero_node : fold_build2 (MINUS_EXPR, size_type_node,
                                          id.offset, r.base.offset);
        if (!r.aggregate
            && (!integer_zerop (delta) || !operand_equal_p (size, r.extent, 0)))
          continue;
        if (TREE_CODE (delta) == INTEGER_CST)
          {
            tree extent = fold_convert (size_type_node, r.extent);
            if (tree_int_cst_lt (extent, delta)) continue;
            tree remaining = fold_build2 (MINUS_EXPR, size_type_node, extent, delta);
            if (TREE_CODE (size) == INTEGER_CST && tree_int_cst_lt (remaining, size))
              continue;
          }
        *relative = delta;
        return &r;
      }
  return NULL;
}

/* Plan tuple: physical base declaration, canonical base offset, registered
   extent, kind, access-relative offset, access size, mode, function, location.
   Reference-to-subobject registration retains its own extent, never silently
   registers the containing aggregate.  CALL entries have mode=4 and no range.
   These are unevaluated recipes, not executable duplicate index evaluations. */
static void plan_entry (state &s, const registration *reg, tree offset,
                        tree size, int mode, tree fn)
{
  /* A captured constant read and its surviving source node denote the same
     access when their identity, range and source point coincide. */
  if (mode == 1)
    for (tree p = s.plan; p; p = TREE_CHAIN (p))
      {
        tree v = TREE_VALUE (p);
        if (TREE_VEC_ELT (v, 0) == reg->base.object && TREE_VEC_ELT (v, 7) == fn
            && tree_to_uhwi (TREE_VEC_ELT (v, 6)) == 1
            && tree_to_uhwi (TREE_VEC_ELT (v, 8)) == input_location
            && operand_equal_p (TREE_VEC_ELT (v, 1), reg->base.offset, 0)
            && operand_equal_p (TREE_VEC_ELT (v, 4), offset, 0)
            && operand_equal_p (TREE_VEC_ELT (v, 5), size, 0)) return;
      }
  tree entry = make_tree_vec (9);
  TREE_VEC_ELT (entry, 0) = reg ? reg->base.object : NULL_TREE;
  TREE_VEC_ELT (entry, 1) = reg ? reg->base.offset : NULL_TREE;
  TREE_VEC_ELT (entry, 2) = reg ? reg->extent : NULL_TREE;
  TREE_VEC_ELT (entry, 3) = build_int_cst (integer_type_node, reg && reg->aggregate);
  TREE_VEC_ELT (entry, 4) = offset;
  TREE_VEC_ELT (entry, 5) = size;
  TREE_VEC_ELT (entry, 6) = build_int_cst (integer_type_node, mode);
  TREE_VEC_ELT (entry, 7) = fn;
  TREE_VEC_ELT (entry, 8) = build_int_cst (unsigned_type_node, input_location);
  s.plan = tree_cons (NULL_TREE, entry, s.plan);
}

static void access (tree t, state &s, bool write)
{
  if (member (s.invalid_accesses, t)) return;
  tree type = value_type (t);
  if (!supported_type (type))
    { s.invalid_accesses.safe_push (t); reject (t, "transaction access has an unsupported type"); return; }
  if (write && (TYPE_READONLY (type) || TREE_READONLY (t)))
    { s.invalid_accesses.safe_push (t); reject (t, "transaction cannot write a const object"); return; }
  if (!write && POINTER_TYPE_P (type) && !pointer_identity (t, s).object)
    { reject (t, "transaction pointer value has no provable canonical identity"); }
  identity id = object_identity (t, s);
  if (!id.object)
    { s.invalid_accesses.safe_push (t); reject (t, "transaction access has no provable canonical object"); return; }
  if (write && id.readonly)
    { s.invalid_accesses.safe_push (t); reject (t, "transaction cannot write a const canonical object"); return; }
  tree relative = NULL_TREE;
  const registration *reg = find_registration (id, TYPE_SIZE_UNIT (type), s, &relative);
  if (!member (s.locals, id.object) && !reg)
    { s.invalid_accesses.safe_push (t);
      error_at (location (t), "transaction access to %qD requires explicit registration",
                id.object); return; }
  if (!member (s.locals, id.object))
    plan_entry (s, reg, relative, TYPE_SIZE_UNIT (type), write ? 2 : 1, s.function);
}

static void address_dependencies (tree t, state &s)
{
  if (!t) return;
  STRIP_NOPS (t);
  if (TREE_CODE (t) == COMPONENT_REF)
    address_dependencies (TREE_OPERAND (t, 0), s);
  else if (TREE_CODE (t) == ARRAY_REF)
    {
      address_dependencies (TREE_OPERAND (t, 0), s);
      examine (TREE_OPERAND (t, 1), s);
    }
  else if (TREE_CODE (t) == INDIRECT_REF || TREE_CODE (t) == MEM_REF)
    {
      tree pointer = TREE_OPERAND (t, 0);
      STRIP_NOPS (pointer);
      if (!(DECL_P (pointer) && TREE_CODE (source_type (pointer)) == REFERENCE_TYPE))
        examine (pointer, s);
    }
}

static void preserve_changes (state &s, unsigned mark, auto_vec<tree> &changed)
{
  for (unsigned i = mark; i < s.pointers.length (); ++i)
    changed.safe_push (s.pointers[i].decl);
  s.pointers.truncate (mark);
}

struct mutations
{
  state *s;
  auto_vec<tree> keys;
  bool indirect = false;
  bool calls = false;
};
static tree pointer_mutations (tree *tp, int *subtrees, void *argument)
{
  tree t = *tp;
  mutations *m = (mutations *) argument;
  if (DECL_P (t) || TYPE_P (t)) { *subtrees = 0; return NULL_TREE; }
  enum tree_code code = TREE_CODE (t);
  if (code == CALL_EXPR && m->calls) m->indirect = true;
  if (code == MODIFY_EXPR || code == INIT_EXPR || code == PREINCREMENT_EXPR
      || code == POSTINCREMENT_EXPR || code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR)
    {
      tree lhs = TREE_OPERAND (t, 0);
      if (POINTER_TYPE_P (value_type (lhs)))
        {
          identity id = object_identity (lhs, *m->s);
          if (id.object && DECL_P (id.object)) m->keys.safe_push (id.object);
          else m->indirect = true;
        }
    }
  if (code == ADDR_EXPR && VAR_P (TREE_OPERAND (t, 0))
      && POINTER_TYPE_P (value_type (TREE_OPERAND (t, 0))))
    m->keys.safe_push (TREE_OPERAND (t, 0));
  return NULL_TREE;
}

static void call (tree t, state &s)
{
  tree fn = get_callee_fndecl (t);
  for (int i = 0; i < call_expr_nargs (t); ++i)
    examine (CALL_EXPR_ARG (t, i), s);
  if (!s.active)
    { s.pointers.truncate (0); return; }
  if (fn && (DECL_IS_OPERATOR_NEW_P (fn) || DECL_IS_OPERATOR_DELETE_P (fn)))
    { reject (t, "transaction does not support program allocation or deallocation"); return; }
  if (!fn || !source_body (fn)
      || DECL_BUILT_IN_CLASS (fn) != NOT_BUILT_IN || stdarg_p (TREE_TYPE (fn)))
    { reject (t, "transaction call requires a direct same-unit prepared definition"); return; }
  tree result = TREE_TYPE (TREE_TYPE (fn));
  if (TREE_CODE (result) == REFERENCE_TYPE)
    { reject (t, "transaction prepared function cannot return a reference"); return; }
  if (TREE_CODE (result) != VOID_TYPE && !supported_type (result))
    { reject (t, "transaction prepared function has an unsupported result type"); return; }
  newtm_occurrence_select (fn);
  plan_entry (s, NULL, NULL_TREE, NULL_TREE, 4, fn);
  /* Pointer provenance is an ABI prerequisite, also at recursive call edges. */
  for (int arg = 0; arg < call_expr_nargs (t); ++arg)
    if (POINTER_TYPE_P (TREE_TYPE (CALL_EXPR_ARG (t, arg)))
        && !pointer_identity (CALL_EXPR_ARG (t, arg), s).object)
      reject (t, "transaction pointer argument has no provable canonical identity");
  if (member (s.stack, fn))
    {
      mutations recursive_changes;
      recursive_changes.s = &s;
      for (tree activation : s.stack)
        {
          tree recursive_body = source_body (activation);
          walk_tree_without_duplicates (&recursive_body, pointer_mutations, &recursive_changes);
        }
      if (recursive_changes.indirect || !recursive_changes.keys.is_empty ())
        reject (t, "transaction recursive pointer effects require activation-specific identity");
      for (int arg = 0; arg < call_expr_nargs (t); ++arg)
        if (POINTER_TYPE_P (TREE_TYPE (CALL_EXPR_ARG (t, arg))))
          {
            identity id = pointer_identity (CALL_EXPR_ARG (t, arg), s);
            tree relative;
            tree pointee = TREE_TYPE (TREE_TYPE (CALL_EXPR_ARG (t, arg)));
            tree size = TYPE_SIZE_UNIT (pointee);
            reject (t, "transaction recursive pointer bindings require activation-specific identity");
            if (id.object && !member (s.locals, id.object)
                && !find_registration (id, size ? size : size_one_node, s, &relative))
              reject (t, "transaction recursive pointer argument requires registration");
          }
      return;
    }
  unsigned local_mark = s.locals.length (), pointer_mark = s.pointers.length ();
  unsigned reference_mark = s.references.length ();
  int i = 0;
  for (tree p = DECL_ARGUMENTS (fn); p; p = DECL_CHAIN (p), ++i)
    {
      if (!supported_type (source_type (p)))
        { reject (t, "transaction prepared function has an unsupported parameter type"); continue; }
      if (TREE_CODE (source_type (p)) == REFERENCE_TYPE)
        {
          identity id = i < call_expr_nargs (t)
            ? pointer_identity (CALL_EXPR_ARG (t, i), s) : identity ();
          if (!id.object)
            reject (t, "transaction reference argument has no provable identity");
          s.references.safe_push ({ p, id });
        }
      else
        {
          s.locals.safe_push (p);
          if (POINTER_TYPE_P (TREE_TYPE (p)))
            s.pointers.safe_push ({ p, i < call_expr_nargs (t)
              ? pointer_identity (CALL_EXPR_ARG (t, i), s) : identity () });
        }
    }
  tree saved_fn = s.function, saved_return = s.returned;
  s.function = fn; s.returned = NULL_TREE;
  s.stack.safe_push (fn);
  examine (source_body (fn), s);
  s.stack.pop ();
  identity returned = s.returned && s.returned != error_mark_node
    ? pointer_identity (s.returned, s) : identity ();
  for (unsigned local = local_mark; local < s.locals.length (); ++local)
    if (returned.object == s.locals[local]) returned = identity ();
  if (returned.object && DECL_P (returned.object)
      && DECL_CONTEXT (returned.object) == fn && !TREE_STATIC (returned.object))
    returned = identity ();
  s.returned = saved_return; s.function = saved_fn;
  s.locals.truncate (local_mark);
  auto_vec<alias> effects;
  for (unsigned changed = pointer_mark; changed < s.pointers.length (); ++changed)
    if (s.pointers[changed].effect && DECL_P (s.pointers[changed].decl)
        && DECL_CONTEXT (s.pointers[changed].decl) != fn)
      effects.safe_push (s.pointers[changed]);
  s.pointers.truncate (pointer_mark);
  s.pointers.safe_splice (effects);
  s.references.truncate (reference_mark);
  if (POINTER_TYPE_P (result))
    {
      if (!returned.object)
        reject (t, "transaction pointer result has no provable canonical identity");
      s.pointers.safe_push ({ t, returned });
    }
}

static void examine (tree t, state &s, bool write)
{
  if (!t || t == error_mark_node) return;
  if (EXPR_P (t) && EXPR_LOCATION (t) != UNKNOWN_LOCATION)
    input_location = EXPR_LOCATION (t);
  enum tree_code code = TREE_CODE (t);
  if (code == DEBUG_BEGIN_STMT)
    {
      if (s.active && newtm_markers)
        if (tree *read = newtm_markers->get (t)) access (*read, s, false);
      return;
    }
  if (code == OMP_TRANSACTION)
    {
      bool outer = !s.active;
      tree saved_region = s.region, saved_plan = s.plan;
      unsigned registered_mark = s.registered.length (), local_mark = s.locals.length ();
      if (outer) { s.active = true; s.registered.truncate (0); }
      s.region = t; s.plan = NULL_TREE;
      for (tree p = OMP_TRANSACTION_OBJECTS (t); p; p = TREE_CHAIN (p))
        {
          tree decl = TREE_VALUE (p);
          if (!supported_type (source_type (decl)))
            { error_at (location (t), "transaction object %qD has an unsupported type", decl); continue; }
          identity id = object_identity (decl, s);
          /* The tracing bridge takes these addresses.  Mark before any part
             of the function is gimplified, including uses preceding the
             directive; late addressability would leave invalid register uses
             in an enclosing OpenMP outlined body. */
          if (id.object && DECL_P (id.object)) TREE_ADDRESSABLE (id.object) = 1;
          if (TREE_CODE (source_type (decl)) != REFERENCE_TYPE)
            TREE_ADDRESSABLE (decl) = 1;
          if (member (s.locals, id.object))
            reject (t, "transaction registration cannot outlive a private local object");
          else if (id.object)
            {
              tree type = value_type (decl);
              registration reg = { decl, TYPE_SIZE_UNIT (type), id,
                TREE_CODE (type) == ARRAY_TYPE || TREE_CODE (type) == RECORD_TYPE };
              for (const registration &p : s.registered)
                if (p.base.object == id.object && !same_registration (p, reg)
                    && TREE_CODE (p.base.offset) == INTEGER_CST
                    && TREE_CODE (id.offset) == INTEGER_CST)
                  {
                    tree p_end = fold_build2 (PLUS_EXPR, size_type_node,
                                   p.base.offset, fold_convert (size_type_node, p.extent));
                    tree end = fold_build2 (PLUS_EXPR, size_type_node, id.offset,
                                            fold_convert (size_type_node, reg.extent));
                    if (tree_int_cst_lt (id.offset, p_end)
                        && tree_int_cst_lt (p.base.offset, end))
                      reject (t, "transaction distinct registered objects overlap");
                  }
              if (!member_registration (s.registered, reg)) s.registered.safe_push (reg);
            }
        }
      examine (OMP_BODY (t), s);
      if (s.function == s.origin)
        {
          tree *original = newtm_regions ? newtm_regions->get (t) : NULL;
          OMP_TRANSACTION_PLAN (original ? *original : t) = nreverse (s.plan);
        }
      s.plan = saved_plan; s.region = saved_region;
      s.locals.truncate (local_mark);
      if (outer)
        { s.active = false; s.registered.truncate (registered_mark); }
      return;
    }
  if (code == STATEMENT_LIST)
    {
      if (write && s.active)
        { reject (t, "transaction does not support a statement-expression lvalue"); return; }
      for (tree_stmt_iterator p = tsi_start (t); !tsi_end_p (p); tsi_next (&p))
        examine (tsi_stmt (p), s);
      return;
    }
  if (code == BIND_EXPR)
    {
      if (write && s.active)
        { reject (t, "transaction does not support a statement-expression lvalue"); return; }
      unsigned mark = s.locals.length ();
      for (tree d = BIND_EXPR_VARS (t); d; d = DECL_CHAIN (d))
        if (TREE_CODE (d) == VAR_DECL && s.active && !TREE_STATIC (d))
          s.locals.safe_push (d);
      examine (BIND_EXPR_BODY (t), s);
      s.locals.truncate (mark);
      return;
    }
  if (code == DECL_EXPR)
    {
      tree d = DECL_EXPR_DECL (t);
      if (TREE_CODE (d) != VAR_DECL) return;
      if (s.active && !TREE_STATIC (d) && !member (s.locals, d))
        s.locals.safe_push (d);
      if (s.active && !supported_type (source_type (d)))
        error_at (DECL_SOURCE_LOCATION (d), "transaction local %qD has an unsupported type", d);
      tree initial = source_initial (d);
      if (initial && initial != error_mark_node)
        {
          examine (initial, s);
          if (POINTER_TYPE_P (TREE_TYPE (d)) && !TREE_STATIC (d))
            {
              identity value = pointer_identity (initial, s);
              if (s.active && !value.object)
                reject (t, "transaction pointer initializer has no provable canonical identity");
              s.pointers.safe_push ({ d, value });
            }
          if (TREE_CODE (source_type (d)) == REFERENCE_TYPE)
            s.references.safe_push ({ d, pointer_identity (initial, s) });
        }
      return;
    }
  if (code == SAVE_EXPR && s.active && TREE_SIDE_EFFECTS (t))
    {
      mutations changes;
      changes.s = &s;
      tree operand = TREE_OPERAND (t, 0);
      walk_tree_without_duplicates (&operand, pointer_mutations, &changes);
      if (changes.indirect || !changes.keys.is_empty ())
        { reject (t, "transaction saved pointer mutation has no single-evaluation identity"); return; }
    }
  if (code == CALL_EXPR) { call (t, s); return; }
  if (code == CONSTRUCTOR)
    {
      unsigned HOST_WIDE_INT index;
      tree value;
      FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (t), index, value)
        examine (value, s);
      return;
    }
  if (code == NOP_EXPR || code == CONVERT_EXPR || code == NON_LVALUE_EXPR
      || (code == VIEW_CONVERT_EXPR && location_wrapper_p (t)))
    {
      tree operand = TREE_OPERAND (t, 0);
      if (s.active && TREE_TYPE (operand) && TREE_TYPE (t)
          && POINTER_TYPE_P (TREE_TYPE (operand)) != POINTER_TYPE_P (TREE_TYPE (t))
          && TREE_CODE (TREE_TYPE (t)) != VOID_TYPE)
        reject (t, "transaction does not support pointer representation conversions");
      examine (operand, s, write);
      return;
    }
  if (code == COMPOUND_EXPR)
    {
      examine (TREE_OPERAND (t, 0), s);
      examine (TREE_OPERAND (t, 1), s, write);
      return;
    }
  if (code == COND_EXPR || code == IF_STMT)
    {
      examine (TREE_OPERAND (t, 0), s);
      unsigned mark = s.registered.length (), pointers = s.pointers.length ();
      auto_vec<registration> yes;
      auto_vec<tree> changed;
      examine (TREE_OPERAND (t, 1), s, write);
      for (unsigned i = mark; i < s.registered.length (); ++i) yes.safe_push (s.registered[i]);
      s.registered.truncate (mark);
      preserve_changes (s, pointers, changed);
      examine (TREE_OPERAND (t, 2), s, write);
      auto_vec<registration> both;
      for (unsigned i = mark; i < s.registered.length (); ++i)
        if (member_registration (yes, s.registered[i])) both.safe_push (s.registered[i]);
      s.registered.truncate (mark);
      s.registered.safe_splice (both);
      preserve_changes (s, pointers, changed);
      for (tree d : changed) s.pointers.safe_push ({ d, identity () });
      return;
    }
  if (code == LOOP_EXPR || code == SWITCH_EXPR || code == SWITCH_STMT
      || code == FOR_STMT || code == WHILE_STMT || code == DO_STMT)
    {
      unsigned mark = s.registered.length (), pointers = s.pointers.length ();
      bool is_switch = code == SWITCH_EXPR || code == SWITCH_STMT;
      mutations changed_pointers;
      changed_pointers.s = &s;
      changed_pointers.calls = true;
      if (!is_switch)
        {
          tree body = t;
          walk_tree_without_duplicates (&body, pointer_mutations, &changed_pointers);
          if (changed_pointers.indirect)
            for (unsigned i = 0; i < pointers; ++i)
              changed_pointers.keys.safe_push (s.pointers[i].decl);
          for (tree key : changed_pointers.keys)
            s.pointers.safe_push ({ key, identity (), true });
        }
      if (is_switch) s.switches.safe_push (mark);
      if (code == FOR_STMT)
        {
          examine (TREE_OPERAND (t, 0), s);
          examine (TREE_OPERAND (t, 1), s);
          examine (TREE_OPERAND (t, 3), s);
          examine (TREE_OPERAND (t, 2), s);
        }
      else if (code == WHILE_STMT || code == DO_STMT || code == SWITCH_STMT)
        {
          examine (TREE_OPERAND (t, 0), s);
          examine (TREE_OPERAND (t, 1), s);
        }
      else
        for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i)
          examine (TREE_OPERAND (t, i), s);
      if (is_switch) s.switches.pop ();
      s.registered.truncate (mark);
      auto_vec<tree> changed;
      preserve_changes (s, pointers, changed);
      for (tree d : changed) s.pointers.safe_push ({ d, identity () });
      return;
    }
  if (code == CASE_LABEL_EXPR && !s.switches.is_empty ())
    { s.registered.truncate (s.switches.last ()); return; }
  if (code == SIZEOF_EXPR || code == PAREN_SIZEOF_EXPR || code == ALIGNOF_EXPR
      || code == NOEXCEPT_EXPR) return;
  if (code == TARGET_EXPR)
    {
      tree temporary = TARGET_EXPR_SLOT (t);
      if (s.active)
        {
          if (!supported_type (source_type (temporary)) || TARGET_EXPR_CLEANUP (t))
            { reject (t, "transaction temporary requires an unsupported type or cleanup"); return; }
          s.locals.safe_push (temporary);
        }
      examine (TARGET_EXPR_INITIAL (t), s);
      return;
    }
  if (code == MODIFY_EXPR || code == INIT_EXPR)
    {
      tree lhs = TREE_OPERAND (t, 0), rhs = TREE_OPERAND (t, 1);
      examine (rhs, s);
      if (!(code == INIT_EXPR && member (s.locals, object_identity (lhs, s).object)))
        examine (lhs, s, true);
      if (POINTER_TYPE_P (value_type (lhs)))
        {
          identity target = object_identity (lhs, s);
          identity value = pointer_identity (rhs, s);
          if (s.active && !value.object)
            reject (t, "transaction pointer assignment has no provable canonical identity");
          if (s.active && value.object && member (s.locals, value.object)
              && target.object && !member (s.locals, target.object))
            reject (t, "transaction pointer cannot escape a private object");
          if (target.object && DECL_P (target.object) && integer_zerop (target.offset))
            s.pointers.safe_push ({ target.object, value, true });
        }
      return;
    }
  if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR
      || code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR)
    {
      tree operand = TREE_OPERAND (t, 0);
      identity old = POINTER_TYPE_P (value_type (operand))
        ? pointer_identity (operand, s) : identity ();
      examine (operand, s);
      examine (operand, s, true);
      if (POINTER_TYPE_P (value_type (operand)))
        {
          identity target = object_identity (operand, s);
          tree amount = TYPE_SIZE_UNIT (TREE_TYPE (value_type (operand)));
          identity next = old;
          if (!amount) next = identity ();
          else
            {
              amount = fold_convert (size_type_node, amount);
              if (code == PREDECREMENT_EXPR || code == POSTDECREMENT_EXPR)
                amount = fold_build1 (NEGATE_EXPR, size_type_node, amount);
              next = add_offset (next, amount);
            }
          if (target.object && DECL_P (target.object) && integer_zerop (target.offset))
            s.pointers.safe_push ({ target.object, next, true });
          s.pointers.safe_push ({ t,
            code == POSTINCREMENT_EXPR || code == POSTDECREMENT_EXPR ? old : next });
        }
      return;
    }
  if (code == RETURN_EXPR)
    {
      tree value = TREE_OPERAND (t, 0);
      if (value && (TREE_CODE (value) == MODIFY_EXPR || TREE_CODE (value) == INIT_EXPR))
        value = TREE_OPERAND (value, 1);
      examine (value, s);
      if (s.active && value && POINTER_TYPE_P (TREE_TYPE (value)))
        {
          identity id = pointer_identity (value, s);
          identity previous = s.returned && s.returned != error_mark_node
            ? pointer_identity (s.returned, s) : identity ();
          if (!id.object || s.returned == error_mark_node
              || (s.returned && (id.object != previous.object
                  || !operand_equal_p (id.offset, previous.offset, 0))))
            {
              reject (t, "transaction pointer result has no unique canonical identity");
              s.returned = error_mark_node;
            }
          else s.returned = value;
        }
      return;
    }
  if (code == ADDR_EXPR)
    {
      tree value = TREE_OPERAND (t, 0);
      /* Address formation is not a read of the designated value.  Index and
         pointer computations still have their own access requirements. */
      address_dependencies (value, s);
      return;
    }
  if (s.active)
    {
      if (code == REALPART_EXPR || code == IMAGPART_EXPR || code == BIT_FIELD_REF)
        {
          identity id = object_identity (TREE_OPERAND (t, 0), s);
          if (!id.object || !member (s.locals, id.object))
            reject (t, "transaction does not support a partial scalar access");
          else examine (TREE_OPERAND (t, 0), s, write);
          return;
        }
      if (code == TRY_CATCH_EXPR || code == TRY_FINALLY_EXPR || code == CATCH_EXPR
          || code == EH_FILTER_EXPR || code == WITH_CLEANUP_EXPR
          || code == THROW_EXPR || code == TRY_BLOCK || code == HANDLER
          || code == CLEANUP_STMT || code == TYPEID_EXPR)
        { reject (t, "transaction does not support exceptions or user cleanups"); return; }
      if (code == ASM_EXPR)
        { reject (t, "transaction does not support external assembly effects"); return; }
      if (code == NEW_EXPR || code == VEC_NEW_EXPR || code == DELETE_EXPR
          || code == VEC_DELETE_EXPR)
        { reject (t, "transaction does not support program allocation or deallocation"); return; }
      if (code >= OMP_PARALLEL && code <= OMP_ATOMIC_CAPTURE_NEW)
        { reject (t, "transaction does not support OpenMP effects inside its body"); return; }
      if (code == VAR_DECL || code == PARM_DECL || code == COMPONENT_REF
          || code == ARRAY_REF || code == INDIRECT_REF || code == MEM_REF)
        {
          address_dependencies (t, s);
          access (t, s, write);
          return;
        }
      if ((write && TREE_CODE_CLASS (code) == tcc_reference)
          || code == VIEW_CONVERT_EXPR)
        { reject (t, "transaction lvalue or representation view is unsupported"); return; }
      if (code == GOTO_EXPR && TREE_CODE (TREE_OPERAND (t, 0)) == LABEL_DECL
          && !DECL_ARTIFICIAL (TREE_OPERAND (t, 0)))
        { reject (t, "transaction does not support unstructured jumps"); return; }
    }
  if (DECL_P (t) || TYPE_P (t) || CONSTANT_CLASS_P (t) || code == BLOCK) return;
  if (EXPR_P (t))
    for (int i = 0; i < TREE_OPERAND_LENGTH (t); ++i) examine (TREE_OPERAND (t, i), s);
}
} // namespace

void
newtm_analyze_unit (void)
{
  if (seen_error ()) return; /* Do not analyze malformed frontend trees. */
  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    if (source_body (node->decl))
      {
        state s (node->decl);
        input_location = DECL_SOURCE_LOCATION (node->decl);
        examine (source_body (node->decl), s);
      }
}

#include "gt-newtm-analysis.h"
