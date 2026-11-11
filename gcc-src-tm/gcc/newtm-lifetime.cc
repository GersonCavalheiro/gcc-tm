/* Must-initialization ignores padding; pointer provenance survives joins.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "tree-dfa.h"
#include "bitmap.h"
#include "gimple-pretty-print.h"
#include "options.h"
#include "newtm-flow.h"
#include "newtm-lifetime.h"
#include "newtm-clone.h"
#include "newtm-abi.h"

namespace {
enum constant { UNKNOWN, ZERO, NONZERO };
struct object { tree decl; unsigned scope, first, count, root; bool watch, borrowed; tree lexical; };
struct cell { unsigned object; HOST_WIDE_INT offset, size; };
struct requirement { gimple *call; tree ref; HOST_WIDE_INT size; };
struct production { gimple *call; tree ref; bool pointer, binding; unsigned args; };
struct call_effect { gimple *call; unsigned first, end; };
struct state
{
  auto_vec<unsigned char> live, initialized, constants, active;
  auto_vec<unsigned> pointers;
  unsigned publications = 1, returned = UNKNOWN;
  state (unsigned n, unsigned m)
  { live.safe_grow_cleared (n); active.safe_grow_cleared (n); initialized.safe_grow_cleared (m);
    constants.safe_grow_cleared (m); pointers.safe_grow_cleared (m); }
  void copy (const state &s)
  {
    publications = s.publications; returned = s.returned;
    for (unsigned i = 0; i < live.length (); ++i) { live[i] = s.live[i]; active[i] = s.active[i]; }
    for (unsigned i = 0; i < pointers.length (); ++i)
      { initialized[i] = s.initialized[i]; constants[i] = s.constants[i]; pointers[i] = s.pointers[i]; }
  }
};
static tree lhs (gimple *s)
{ return s && is_gimple_assign (s) ? gimple_assign_lhs (s)
    : s && is_gimple_call (s) ? gimple_call_lhs (s) : NULL_TREE; }
}
struct newtm_lifetime_builder
{
  const newtm_flow_graph &flow;
  tree owner, value_out = NULL_TREE, binding_out = NULL_TREE;
  auto_vec<object> objects;
  auto_vec<cell> cells;
  auto_vec<requirement> inputs;
  auto_vec<production> outputs;
  auto_vec<call_effect> effects;
  auto_vec<std::pair<gimple *, unsigned> > borrows;
  auto_vec<unsigned> readonly_objects;
  hash_map<tree, unsigned> indices;
  hash_map<tree, gimple *> definitions;
  bitmap_obstack storage;
  auto_vec<bitmap> roots;
  const char *reason = NULL; gimple *bad = NULL;
  bool transports_only = false;
  explicit newtm_lifetime_builder (const newtm_flow_graph &f, tree fn, bool current) : flow (f), owner (fn)
  {
    bitmap_obstack_initialize (&storage); roots.safe_push (bitmap_alloc (&storage));
    for (unsigned s = 1; s < flow.scope_count (); ++s)
      if (flow.scope (s).kind == NEWTM_FLOW_BIND)
        for (tree d = gimple_bind_vars (as_a<gbind *> (flow.scope (s).owner)); d; d = DECL_CHAIN (d)) add (d, s);
    for (tree d = DECL_ARGUMENTS (fn); d; d = DECL_CHAIN (d))
      { unsigned i = add (d, 0); if (i != UINT_MAX) objects[i].scope = 0; }
    for (unsigned n = 0; n < flow.node_count (); ++n)
      if (gimple *g = flow.node (n).statement)
        {
          /* CFGs no longer contain BINDs.  Discover address-only buffers and
             aggregate bases too, rather than treating missing objects as
             implicitly live external memory. */
          struct scan {
            static tree visit (tree *p, int *walk, void *data)
            {
              walk_stmt_info *wi = (walk_stmt_info *) data;
              newtm_lifetime_builder *b = (newtm_lifetime_builder *) wi->info;
              if (DECL_P (*p) && DECL_CONTEXT (*p) == b->owner) b->add (*p, 0);
              if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
              return NULL_TREE;
            }
          };
          walk_stmt_info wi = {}; wi.info = this;
          walk_gimple_op (g, scan::visit, &wi);
        }
    if (current)
      {
        /* BLOCKs are current lexical metadata on actual statements.  Unlike
           source descriptors, they are remapped along with the CFG.  Do not
           infer a new lifetime merely because an initialization was seen:
           a store after an explicit end inside the same block stays invalid. */
        hash_set<tree> seen;
        for (unsigned n = 0; n < flow.node_count (); ++n)
          if (gimple *g = flow.node (n).statement)
            for (tree block = gimple_block (g); block && TREE_CODE (block) == BLOCK;
                 block = BLOCK_SUPERCONTEXT (block))
              {
                if (seen.add (block)) break;
                for (tree d = BLOCK_VARS (block); d; d = DECL_CHAIN (d))
                  if (unsigned *i = indices.get (d))
                    if (VAR_P (d) && DECL_CONTEXT (d) == owner) objects[*i].lexical = block;
              }
      }
    for (unsigned n = 0; n < flow.node_count (); ++n)
      if (tree l = lhs (flow.node (n).statement))
        if (DECL_P (l))
          {
            if (DECL_CONTEXT (l) == owner) add (l, 0);
            gimple **old = definitions.get (l), *g = flow.node (n).statement;
            if (!old) definitions.put (l, g); else if (*old != g) *old = NULL;
          }
  }
  ~newtm_lifetime_builder () { bitmap_obstack_release (&storage); }
  bool refuse (gimple *g, const char *why)
  { if (!reason) { reason = why; bad = g; } return false; }
  bool ancestor (unsigned a, unsigned b)
  { for (;; b = flow.scope (b).parent) { if (a == b) return true; if (!b) return false; } }
  unsigned unite (unsigned a, unsigned b)
  {
    if (!a || a == b) return b;
    if (!b || !bitmap_intersect_compl_p (roots[b], roots[a])) return a;
    if (!bitmap_intersect_compl_p (roots[a], roots[b])) return b;
    bitmap r = bitmap_alloc (&storage); bitmap_ior (r, roots[a], roots[b]);
    roots.safe_push (r); return roots.length () - 1;
  }
  void flatten (unsigned object, tree type, HOST_WIDE_INT offset, unsigned depth = 0)
  {
    tree size = TYPE_SIZE (type);
    if (depth > 16 || !size || !tree_fits_shwi_p (size))
      { refuse (NULL, "V04: transport has no finite field layout"); return; }
    if (TREE_CODE (type) == RECORD_TYPE)
      {
        for (tree f = TYPE_FIELDS (type); f; f = DECL_CHAIN (f))
          if (TREE_CODE (f) == FIELD_DECL)
            {
              tree p = DECL_FIELD_OFFSET (f), b = DECL_FIELD_BIT_OFFSET (f);
              if (!p || !b || !tree_fits_shwi_p (p) || !tree_fits_shwi_p (b))
                { refuse (NULL, "V04: nonconstant transport field offset"); return; }
              flatten (object, TREE_TYPE (f), offset + tree_to_shwi (p) * BITS_PER_UNIT + tree_to_shwi (b), depth + 1);
            }
        return;
      }
    if (TREE_CODE (type) == ARRAY_TYPE)
      {
        tree element = TREE_TYPE (type), bytes = TYPE_SIZE (element);
        if (!bytes || !tree_fits_shwi_p (bytes) || !tree_to_shwi (bytes)
            || tree_to_shwi (size) / tree_to_shwi (bytes) > 4096)
          { refuse (NULL, "V04: transport array layout exceeds finite inspection domain"); return; }
        for (HOST_WIDE_INT at = 0; at < tree_to_shwi (size); at += tree_to_shwi (bytes))
          flatten (object, element, offset + at, depth + 1);
        return;
      }
    cell c = { object, offset, tree_to_shwi (size) }; cells.safe_push (c);
  }
  unsigned add (tree d, unsigned scope)
  {
    if (unsigned *i = indices.get (d)) return *i;
    if (!d || !(VAR_P (d) || TREE_CODE (d) == PARM_DECL || TREE_CODE (d) == RESULT_DECL)
        || TREE_STATIC (d) || DECL_EXTERNAL (d)) return UINT_MAX;
    unsigned i = objects.length ();
    object o = { d, scope, cells.length (), 0, 0, false, false, NULL_TREE };
    indices.put (d, i); objects.safe_push (o);
    /* Ordinary aggregates need lifetime/provenance, not a byte initialization
       proof. ABI transports are flattened when explicitly watched. */
    cell c = { i, 0, DECL_SIZE (d) && tree_fits_shwi_p (DECL_SIZE (d)) ? tree_to_shwi (DECL_SIZE (d)) : 0 };
    cells.safe_push (c); objects[i].count = 1;
    bitmap r = bitmap_alloc (&storage); bitmap_set_bit (r, i); roots.safe_push (r);
    objects[i].root = roots.length () - 1; return i;
  }
  void watch (tree d)
  {
    unsigned i = add (d, 0); if (i == UINT_MAX || objects[i].watch) return;
    objects[i].watch = true;
    /* The old scalar cell remains inert; existing indices are never shifted. */
    objects[i].first = cells.length (); flatten (i, TREE_TYPE (d), 0);
    objects[i].count = cells.length () - objects[i].first;
  }
  bool region (tree ref, tree &base, HOST_WIDE_INT &offset, HOST_WIDE_INT &size, unsigned depth = 0)
  {
    if (!ref || depth > 32) return false;
    bool reverse;
    base = get_ref_base_and_extent_hwi (ref, &offset, &size, &reverse);
    if (!base || offset < 0 || size < 0 || reverse) return false;
    if (TREE_CODE (base) == MEM_REF || TREE_CODE (base) == INDIRECT_REF)
      {
        tree p = TREE_OPERAND (base, 0); tree addr = address (p, depth + 1);
        if (!addr) return false;
        tree b; HOST_WIDE_INT at, extent;
        if (!region (addr, b, at, extent, depth + 1)) return false;
        if (TREE_CODE (base) == MEM_REF)
          {
            tree displacement = TREE_OPERAND (base, 1);
            if (!tree_fits_shwi_p (displacement)) return false;
            at += tree_to_shwi (displacement) * BITS_PER_UNIT;
          }
        base = b; offset += at;
      }
    return offset >= 0;
  }
  tree address (tree t, unsigned depth = 0)
  {
    if (!t || depth > 32) return NULL_TREE;
    if (TREE_CODE (t) == ADDR_EXPR) return TREE_OPERAND (t, 0);
    if (CONVERT_EXPR_CODE_P (TREE_CODE (t))) return address (TREE_OPERAND (t, 0), depth + 1);
    if (DECL_P (t) && !TREE_ADDRESSABLE (t))
      if (gimple **d = definitions.get (t))
        if (*d && is_gimple_assign (*d) && (gimple_assign_single_p (*d) || gimple_assign_cast_p (*d)))
          return address (gimple_assign_rhs1 (*d), depth + 1);
    return NULL_TREE;
  }
  unsigned contents (tree ref, const state &s)
  {
    tree b = NULL_TREE; HOST_WIDE_INT at, size; unsigned result = 0;
    if (region (ref, b, at, size))
      if (unsigned *i = indices.get (b))
        for (unsigned n = objects[*i].first; n < objects[*i].first + objects[*i].count; ++n)
          if (cells[n].offset < at + size && at < cells[n].offset + cells[n].size) result = unite (result, s.pointers[n]);
    tree raw = ref ? get_base_address (ref) : NULL_TREE;
    if (raw && (TREE_CODE (raw) == MEM_REF || TREE_CODE (raw) == INDIRECT_REF) && !indices.get (b))
      {
        unsigned p = pointer (TREE_OPERAND (raw, 0), s), i; bitmap_iterator it;
        EXECUTE_IF_SET_IN_BITMAP (roots[p], 0, i, it)
          if (!objects[i].borrowed)
            for (unsigned n = objects[i].first; n < objects[i].first + objects[i].count; ++n)
              result = unite (result, s.pointers[n]);
      }
    return result;
  }
  unsigned pointer (tree t, const state &s, unsigned depth = 0)
  {
    if (!t) return 0;
    if (depth > 64) { unsigned all = 0; for (const object &o : objects) all = unite (all, o.root); return all; }
    if (TREE_CODE (t) == ADDR_EXPR)
      {
        tree b = get_base_address (TREE_OPERAND (t, 0));
        if (unsigned *i = indices.get (b)) return objects[*i].root;
        if (b && (TREE_CODE (b) == MEM_REF || TREE_CODE (b) == INDIRECT_REF)) return pointer (TREE_OPERAND (b, 0), s, depth + 1);
        return 0;
      }
    if (REFERENCE_CLASS_P (t) && !contains_pointer (TREE_TYPE (t))) return 0;
    if (DECL_P (t) || REFERENCE_CLASS_P (t)) return contents (t, s);
    unsigned result = 0;
    if (TREE_CODE (t) == CONSTRUCTOR)
      {
        unsigned HOST_WIDE_INT index; tree value;
        FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (t), index, value) result = unite (result, pointer (value, s, depth + 1));
      }
    else if (EXPR_P (t))
      for (int n = 0; n < TREE_OPERAND_LENGTH (t); ++n) result = unite (result, pointer (TREE_OPERAND (t, n), s, depth + 1));
    return result;
  }
  bool alive (gimple *g, unsigned p, const state &s)
  {
    bool protocol = !transports_only;
    for (const auto &borrow : borrows) if (borrow.first == g) protocol = true;
    if (g && is_gimple_assign (g))
      if (unsigned *i = indices.get (get_base_address (gimple_assign_lhs (g))))
        protocol |= objects[*i].watch;
    bitmap_iterator it; unsigned i;
    EXECUTE_IF_SET_IN_BITMAP (roots[p], 0, i, it)
      if (!s.live[i] && (protocol || objects[i].watch || objects[i].borrowed))
        return refuse (g, "V04: transport refers to an object outside its lifetime");
    return true;
  }
  bool read (gimple *g, tree ref, const state &s, HOST_WIDE_INT extent = -1)
  {
    if (!ref || TREE_CODE (ref) == ADDR_EXPR) return true;
    tree b; HOST_WIDE_INT at, size;
    if (region (ref, b, at, size))
      if (unsigned *i = indices.get (b))
        {
          if (!s.live[*i]) return refuse (g, "V04: access to a retired transport object");
          if (extent >= 0) size = extent;
          const object &o = objects[*i];
          if (o.watch)
            {
              tree total = DECL_SIZE (o.decl);
              if (!total || !tree_fits_shwi_p (total) || at > tree_to_shwi (total) || size > tree_to_shwi (total) - at)
                return refuse (g, "V04: transport use exceeds its initialized object extent");
              for (unsigned n = o.first; n < o.first + o.count; ++n)
                if (cells[n].offset < at + size && at < cells[n].offset + cells[n].size && !s.initialized[n])
                  return refuse (g, "V04: transport field is not initialized on every reaching path");
            }
          unsigned p = 0;
          for (unsigned n = o.first; n < o.first + o.count; ++n)
            if (cells[n].offset < at + size && at < cells[n].offset + cells[n].size) p = unite (p, s.pointers[n]);
          return alive (g, p, s);
        }
    if (REFERENCE_CLASS_P (ref))
      {
        tree base = get_base_address (ref);
        if (base && (TREE_CODE (base) == MEM_REF || TREE_CODE (base) == INDIRECT_REF))
          {
            unsigned p = pointer (TREE_OPERAND (base, 0), s);
            if (!alive (g, p, s)) return false;
            bitmap_iterator it; unsigned i;
            EXECUTE_IF_SET_IN_BITMAP (roots[p], 0, i, it)
              if (objects[i].watch)
                for (unsigned n = objects[i].first; n < objects[i].first + objects[i].count; ++n)
                  if (!s.initialized[n]) return refuse (g, "V04: alias can consume an uninitialized transport field");
          }
      }
    return alive (g, contents (ref, s), s);
  }
  bool reads (gimple *g, tree t, const state &s)
  {
    if (!t) return true;
    if (TREE_CODE (t) == ADDR_EXPR) return alive (g, pointer (t, s), s);
    if (DECL_P (t) || REFERENCE_CLASS_P (t)) return read (g, t, s);
    if (TREE_CODE (t) == CONSTRUCTOR)
      {
        unsigned HOST_WIDE_INT index; tree value;
        FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (t), index, value) if (!reads (g, value, s)) return false;
      }
    if (EXPR_P (t)) for (int n = 0; n < TREE_OPERAND_LENGTH (t); ++n)
      if (!reads (g, TREE_OPERAND (t, n), s)) return false;
    return true;
  }
  unsigned number (tree t, const state &s)
  {
    if (!t) return UNKNOWN;
    if (TREE_CODE (t) == INTEGER_CST) return integer_zerop (t) ? ZERO : NONZERO;
    tree b; HOST_WIDE_INT at, size;
    if (region (t, b, at, size)) if (unsigned *i = indices.get (b))
      if (objects[*i].count == 1) return s.constants[objects[*i].first];
    return UNKNOWN;
  }
  bool available (tree t, const state &s)
  {
    if (!t) return true;
    if (TREE_CODE (t) == ADDR_EXPR)
      {
        tree ref = TREE_OPERAND (t, 0);
        if (TREE_CODE (ref) == ARRAY_REF) return available (TREE_OPERAND (ref, 1), s);
        tree b = get_base_address (ref);
        return !b || !(TREE_CODE (b) == MEM_REF || TREE_CODE (b) == INDIRECT_REF)
          || available (TREE_OPERAND (b, 0), s);
      }
    tree b; HOST_WIDE_INT at, size;
    if (region (t, b, at, size)) if (unsigned *i = indices.get (b))
      {
        const object &o = objects[*i]; tree type = TREE_TYPE (o.decl);
        /* Ordinary program memory follows the source language's definedness
           contract. Compiler buffers and scalar transport dependencies are
           proved here; this is not a general C uninitialized-memory checker. */
        bool scalar = DECL_P (t) && !TREE_ADDRESSABLE (o.decl)
          && (INTEGRAL_TYPE_P (type) || POINTER_TYPE_P (type) || SCALAR_FLOAT_TYPE_P (type));
        if (o.watch || scalar)
          for (unsigned n = o.first; n < o.first + o.count; ++n)
            if (cells[n].offset < at + size && at < cells[n].offset + cells[n].size && !s.initialized[n]) return false;
        return true;
      }
    if (TREE_CODE (t) == CONSTRUCTOR)
      {
        unsigned HOST_WIDE_INT n; tree v;
        FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (t), n, v) if (!available (v, s)) return false;
      }
    else if (EXPR_P (t))
      for (int n = 0; n < TREE_OPERAND_LENGTH (t); ++n) if (!available (TREE_OPERAND (t, n), s)) return false;
    return true;
  }
  bool constructed (tree ctor, HOST_WIDE_INT offset, HOST_WIDE_INT size, unsigned depth = 0)
  {
    if (depth > 16 || TREE_CODE (ctor) != CONSTRUCTOR) return false;
    unsigned HOST_WIDE_INT n; tree index, value; HOST_WIDE_INT next = 0;
    FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (ctor), n, index, value)
      {
        HOST_WIDE_INT at = 0, count = 0;
        if (index && TREE_CODE (index) == FIELD_DECL)
          {
            tree p = DECL_FIELD_OFFSET (index), b = DECL_FIELD_BIT_OFFSET (index), total = DECL_SIZE (index);
            if (!p || !b || !total || !tree_fits_shwi_p (p) || !tree_fits_shwi_p (b) || !tree_fits_shwi_p (total)) return false;
            at = tree_to_shwi (p) * BITS_PER_UNIT + tree_to_shwi (b); count = tree_to_shwi (total);
          }
        else if (TREE_CODE (TREE_TYPE (ctor)) == ARRAY_TYPE)
          {
            tree total = TYPE_SIZE (TREE_TYPE (TREE_TYPE (ctor)));
            if (!total || !tree_fits_shwi_p (total) || (index && !tree_fits_shwi_p (index))) return false;
            count = tree_to_shwi (total); at = (index ? tree_to_shwi (index) : next) * count;
            next = (index ? tree_to_shwi (index) : next) + 1;
          }
        else return false;
        if (at <= offset && size <= count - (offset - at))
          return TREE_CODE (value) != CONSTRUCTOR || constructed (value, offset - at, size, depth + 1);
      }
    return !CONSTRUCTOR_NO_CLEARING (ctor);
  }
  void write (tree ref, state &s, unsigned p, unsigned c = UNKNOWN, bool initialized = true)
  {
    tree b; HOST_WIDE_INT at, size;
    if (!region (ref, b, at, size))
      {
        tree raw = ref ? get_base_address (ref) : NULL_TREE;
        if (raw && (TREE_CODE (raw) == MEM_REF || TREE_CODE (raw) == INDIRECT_REF))
          {
            unsigned points = pointer (TREE_OPERAND (raw, 0), s);
            bitmap_iterator it; unsigned i;
            EXECUTE_IF_SET_IN_BITMAP (roots[points], 0, i, it)
              for (unsigned n = objects[i].first; n < objects[i].first + objects[i].count; ++n)
                { s.initialized[n] = 0; s.pointers[n] = unite (s.pointers[n], p); s.constants[n] = UNKNOWN; }
          }
        return;
      }
    if (unsigned *i = indices.get (b))
      for (unsigned n = objects[*i].first; n < objects[*i].first + objects[*i].count; ++n)
        if (cells[n].offset < at + size && at < cells[n].offset + cells[n].size)
          {
            bool full = at <= cells[n].offset && cells[n].size <= size - (cells[n].offset - at);
            s.initialized[n] = initialized && full; s.pointers[n] = full ? p : unite (s.pointers[n], p); s.constants[n] = c;
          }
  }
  unsigned published (tree l)
  {
    if (!l) return 0;
    tree b = get_base_address (l);
    if (!b || TREE_CODE (b) != MEM_REF) return 0;
    tree p = TREE_OPERAND (b, 0);
    return p == value_out ? 1 : p == binding_out ? 2 : 0;
  }
  bool inspect (unsigned id, const state &s)
  {
    const newtm_flow_node &node = flow.node (id); gimple *g = node.statement;
    unsigned expected = value_out ? (binding_out ? 3 : 1) : 0;
    if (node.kind == NEWTM_FLOW_RETURN_EXIT && expected)
      {
        if (s.returned == ZERO && s.publications != (1u << expected))
          return refuse (node.origin, "V05: successful return lacks a complete value/binding publication");
        if (s.returned != ZERO && s.publications != 1)
          return refuse (node.origin, "V05: failure or unproved return after publishing an output");
        if (s.returned == UNKNOWN) return refuse (node.origin, "V05: nonvoid frame has an unproved publication outcome");
      }
    if (node.kind == NEWTM_FLOW_NORMAL_EXIT && (expected || s.publications != 1))
      return refuse (node.origin, "V05: transport frame falls through without a proved publication outcome");
    if (!g) return true;
    if (is_gimple_assign (g))
      {
        tree l = lhs (g);
        if (gimple_clobber_p (g)) return true;
        for (unsigned n = 1; n < gimple_num_ops (g); ++n) if (!reads (g, gimple_op (g, n), s)) return false;
        tree b; HOST_WIDE_INT at, size;
        if (region (l, b, at, size)) if (unsigned *i = indices.get (b))
          if (!s.live[*i]) return refuse (g, "V04: write to a retired transport object");
        tree raw = get_base_address (l);
        if (raw && (TREE_CODE (raw) == MEM_REF || TREE_CODE (raw) == INDIRECT_REF))
          {
            unsigned points = pointer (TREE_OPERAND (raw, 0), s);
            if (!alive (g, points, s)) return false;
            for (unsigned object : readonly_objects)
              if (bitmap_bit_p (roots[points], object))
                return refuse (g, "V04: store through a read-only incoming channel");
          }
        unsigned p = 0;
        for (unsigned n = 1; n < gimple_num_ops (g); ++n) p = unite (p, pointer (gimple_op (g, n), s));
        unsigned bit = published (l);
        if ((bit || (memory_lvalue (l) && (!region (l, b, at, size) || !indices.get (b)))) && p)
          {
            unsigned i; bitmap_iterator it;
            EXECUTE_IF_SET_IN_BITMAP (roots[p], 0, i, it)
              if (!transports_only || objects[i].watch || objects[i].borrowed)
                return refuse (g, "V04: address of frame-private storage escapes through publication or a store");
          }
        if (bit)
          {
            if (!available (gimple_assign_rhs1 (g), s)) return refuse (g, "V05: publication consumes an undefined value");
            tree parameter = bit == 1 ? value_out : binding_out;
            if (TREE_CODE (l) != MEM_REF || TREE_OPERAND (l, 0) != parameter
                || !integer_zerop (TREE_OPERAND (l, 1))
                || !types_compatible_p (TREE_TYPE (l), TREE_TYPE (TREE_TYPE (parameter))))
              return refuse (g, "V05: partial or mistyped output publication");
            for (unsigned mask = 0; mask < 4; ++mask)
              if ((s.publications & (1u << mask)) && (mask & bit)) return refuse (g, "V05: output is published more than once");
          }
      }
    else if (is_gimple_call (g))
      {
        if (s.publications != 1) return refuse (g, "V05: a call can fail or alter context after output publication");
        for (unsigned n = 0; n < gimple_call_num_args (g); ++n)
          {
            tree arg = gimple_call_arg (g, n);
            if (!reads (g, arg, s)) return false;
            bool allowed = false;
            for (const auto &r : borrows) if (r.first == g && r.second == n) allowed = true;
            if (!allowed)
              {
                unsigned p = pointer (arg, s), i; bitmap_iterator it;
                EXECUTE_IF_SET_IN_BITMAP (roots[p], 0, i, it)
                  if (objects[i].watch || objects[i].borrowed)
                    return refuse (g, "V04: compiler transport escapes through an unproved call argument");
              }
          }
        for (const requirement &r : inputs)
          if (r.call == g)
            {
              if (!read (g, r.ref, s, r.size)) return false;
              for (const production &o : outputs)
                if (o.call == g)
                  {
                    tree a, b; HOST_WIDE_INT start, size, at, count;
                    if (region (r.ref, a, start, size) && region (o.ref, b, at, count) && a == b)
                      {
                        if (r.size >= 0) size = r.size;
                        if (start < at + count && at < start + size)
                          return refuse (g, "V04: incoming transport overlaps a result buffer");
                      }
                  }
              tree fn = gimple_call_fndecl (g);
              if (fn && fndecl_built_in_p (fn, BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE) && contents (r.ref, s))
                return refuse (g, "V04: frame-private address escapes through a transactional write");
            }
      }
    else if (gimple_code (g) == GIMPLE_RETURN)
      { if (!reads (g, gimple_return_retval (as_a<greturn *> (g)), s)) return false; }
    else if (gimple_code (g) == GIMPLE_COND || gimple_code (g) == GIMPLE_SWITCH)
      for (unsigned n = 0; n < gimple_num_ops (g); ++n) if (!reads (g, gimple_op (g, n), s)) return false;
    return true;
  }
  static bool memory_lvalue (tree l)
  { return REFERENCE_CLASS_P (l) || (VAR_P (l) && (TREE_STATIC (l) || DECL_EXTERNAL (l))); }
  bool contains_pointer (tree type, unsigned depth = 0)
  {
    if (depth > 16) return true;
    if (POINTER_TYPE_P (type)) return true;
    if (TREE_CODE (type) == ARRAY_TYPE) return contains_pointer (TREE_TYPE (type), depth + 1);
    if (TREE_CODE (type) == RECORD_TYPE || TREE_CODE (type) == UNION_TYPE)
      for (tree f = TYPE_FIELDS (type); f; f = DECL_CHAIN (f))
        if (TREE_CODE (f) == FIELD_DECL && contains_pointer (TREE_TYPE (f), depth + 1)) return true;
    return false;
  }
  void transfer (unsigned id, state &s)
  {
    gimple *g = flow.node (id).statement; if (!g) return;
    if (is_gimple_assign (g))
      {
        if (gimple_clobber_p (g))
          {
            write (lhs (g), s, 0, UNKNOWN, false);
            if (unsigned *i = indices.get (get_base_address (lhs (g))))
              {
                if (gimple_clobber_p (g, CLOBBER_STORAGE_END) || gimple_clobber_p (g, CLOBBER_OBJECT_END)) s.live[*i] = 0;
                if (gimple_clobber_p (g, CLOBBER_STORAGE_BEGIN) || gimple_clobber_p (g, CLOBBER_OBJECT_BEGIN)) s.live[*i] = 1;
              }
            return;
          }
        unsigned p = 0, c = UNKNOWN; bool defined = true;
        for (unsigned n = 1; n < gimple_num_ops (g); ++n)
          { p = unite (p, pointer (gimple_op (g, n), s)); defined &= available (gimple_op (g, n), s); }
        if (TREE_CODE (TREE_TYPE (lhs (g))) == BOOLEAN_TYPE
            || TREE_CODE_CLASS (gimple_assign_rhs_code (g)) == tcc_comparison) p = 0;
        if (gimple_assign_single_p (g)) c = number (gimple_assign_rhs1 (g), s);
        tree rhs = gimple_assign_rhs1 (g);
        if (gimple_assign_single_p (g) && TREE_CODE (rhs) == CONSTRUCTOR)
          {
            tree b; HOST_WIDE_INT at, size;
            if (region (lhs (g), b, at, size))
              if (unsigned *i = indices.get (b))
                for (unsigned n = objects[*i].first; n < objects[*i].first + objects[*i].count; ++n)
                  if (at <= cells[n].offset && cells[n].size <= size - (cells[n].offset - at))
                    {
                      bool full = defined && constructed (rhs, cells[n].offset - at, cells[n].size);
                      s.initialized[n] = full; s.pointers[n] = full ? p : unite (s.pointers[n], p); s.constants[n] = UNKNOWN;
                    }
          }
        else write (lhs (g), s, p, c, defined);
        unsigned bit = published (lhs (g));
        if (bit)
          {
            unsigned next = 0;
            for (unsigned mask = 0; mask < 4; ++mask)
              if (s.publications & (1u << mask)) next |= 1u << (mask | bit);
            s.publications = next;
          }
      }
    else if (is_gimple_call (g))
      {
        tree fn = gimple_call_fndecl (g);
        for (const call_effect &effect : effects)
          if (effect.call == g)
            {
              unsigned p = 0;
              for (unsigned n = effect.first; n < effect.end; ++n) p = unite (p, pointer (gimple_call_arg (g, n), s));
              bool changed;
              do
                {
                  unsigned next = p, i; bitmap_iterator it;
                  EXECUTE_IF_SET_IN_BITMAP (roots[p], 0, i, it)
                    for (unsigned n = objects[i].first; n < objects[i].first + objects[i].count; ++n)
                      next = unite (next, s.pointers[n]);
                  changed = next != p; p = next;
                }
              while (changed);
              unsigned i; bitmap_iterator it;
              EXECUTE_IF_SET_IN_BITMAP (roots[p], 0, i, it)
                if (contains_pointer (TREE_TYPE (objects[i].decl)))
                  for (unsigned n = objects[i].first; n < objects[i].first + objects[i].count; ++n)
                    { s.pointers[n] = unite (s.pointers[n], p); s.constants[n] = UNKNOWN; }
            }
        write (lhs (g), s, 0, fn && fndecl_built_in_p (fn, BUILT_IN_GOMP_TRANSACTION_STM_V1_FAIL) ? NONZERO : UNKNOWN);
        for (const production &o : outputs) if (o.call == g)
          {
            unsigned p = 0;
            if (o.pointer || o.binding)
              for (unsigned n = 2; n < o.args; ++n) p = unite (p, pointer (gimple_call_arg (g, n), s));
            write (o.ref, s, p);
          }
      }
    else if (gimple_code (g) == GIMPLE_RETURN) s.returned = number (gimple_return_retval (as_a<greturn *> (g)), s);
  }
  void edge (unsigned from, unsigned to, state &s)
  {
    unsigned a = flow.node (from).scope, b = flow.node (to).scope;
    for (unsigned i = 0; i < objects.length (); ++i)
      if (objects[i].scope && ancestor (objects[i].scope, a) != ancestor (objects[i].scope, b))
        {
          s.live[i] = ancestor (objects[i].scope, b);
          for (unsigned n = objects[i].first; n < objects[i].first + objects[i].count; ++n)
            { s.initialized[n] = 0; s.pointers[n] = 0; s.constants[n] = UNKNOWN; }
        }
    /* Empty CFG transfer nodes carry the previous lexical state.  Compare
       scopes at the next actual statement, separately on each incoming path,
       before merging. This also handles edges through empty loop latches. */
    gimple *next = flow.node (to).statement;
    tree block = next ? gimple_block (next) : NULL_TREE;
    if (block)
      for (unsigned i = 0; i < objects.length (); ++i)
        if (objects[i].lexical)
          {
            bool inside = false;
            for (tree p = block; p && TREE_CODE (p) == BLOCK; p = BLOCK_SUPERCONTEXT (p))
              if (p == objects[i].lexical) { inside = true; break; }
            if (inside != bool (s.active[i]))
              {
                s.active[i] = s.live[i] = inside;
                for (unsigned n = objects[i].first; n < objects[i].first + objects[i].count; ++n)
                  { s.initialized[n] = 0; s.pointers[n] = 0; s.constants[n] = UNKNOWN; }
              }
          }
  }
  bool branch (gimple *g, const newtm_flow_edge &edge, state &s)
  {
    if (!g || gimple_code (g) != GIMPLE_COND
        || (edge.kind != NEWTM_FLOW_TRUE && edge.kind != NEWTM_FLOW_FALSE)) return true;
    gcond *c = as_a<gcond *> (g);
    tree a = gimple_cond_lhs (c), b = gimple_cond_rhs (c);
    tree_code code = gimple_cond_code (c);
    if (code != EQ_EXPR && code != NE_EXPR) return true;
    unsigned av = number (a, s), bv = number (b, s);
    bool eq = edge.kind == NEWTM_FLOW_TRUE ? code == EQ_EXPR : code == NE_EXPR;
    if (av && bv && (av == ZERO || bv == ZERO))
      if ((av == bv) != eq) return false;
    if (bv == ZERO && DECL_P (a))
      if (unsigned *i = indices.get (a)) if (objects[*i].count == 1)
        s.constants[objects[*i].first] = eq ? ZERO : NONZERO;
    return true;
  }
  bool merge (state &a, const state &b)
  {
    bool changed = false;
#define UPDATE(A, B) do { auto next = (B); if ((A) != next) { (A) = next; changed = true; } } while (0)
    UPDATE (a.publications, a.publications | b.publications);
    UPDATE (a.returned, a.returned == b.returned ? a.returned : unsigned (UNKNOWN));
    for (unsigned i = 0; i < a.live.length (); ++i)
      { UPDATE (a.live[i], a.live[i] & b.live[i]); UPDATE (a.active[i], a.active[i] & b.active[i]); }
    for (unsigned i = 0; i < a.pointers.length (); ++i)
      {
        UPDATE (a.initialized[i], a.initialized[i] & b.initialized[i]);
        UPDATE (a.constants[i], a.constants[i] == b.constants[i] ? a.constants[i] : unsigned (UNKNOWN));
        UPDATE (a.pointers[i], unite (a.pointers[i], b.pointers[i]));
      }
#undef UPDATE
    return changed;
  }
  bool verify ()
  {
    if (reason) return false;
    /* Publication masks are distinct continuations through native cleanup
       routers. Joining a failed unpublished path with a successful published
       one before its selector would lose exactly the fact being proved. */
    auto_vec<state *> in; in.safe_grow_cleared (flow.node_count () * 4);
    auto_vec<unsigned> work; work.safe_push (0);
    auto_vec<unsigned char> queued; queued.safe_grow_cleared (in.length ()); queued[0] = 1;
    in[0] = new state (objects.length (), cells.length ());
    for (unsigned i = 0; i < objects.length (); ++i)
      {
        in[0]->live[i] = !objects[i].scope;
        if (TREE_CODE (objects[i].decl) == PARM_DECL)
          for (unsigned n = objects[i].first; n < objects[i].first + objects[i].count; ++n)
            { in[0]->initialized[n] = 1; if (objects[i].borrowed) in[0]->pointers[n] = objects[i].root; }
      }
    for (unsigned at = 0; at < work.length (); ++at)
      {
        unsigned row = work[at], id = row / 4; queued[row] = 0;
        state out (objects.length (), cells.length ()); out.copy (*in[row]); transfer (id, out);
        for (const newtm_flow_edge &e : flow.node (id).successors)
          {
            state next (objects.length (), cells.length ()); next.copy (out); edge (id, e.target, next);
            if (!branch (flow.node (id).statement, e, next)) continue;
            unsigned mask = next.publications == 1 ? 0 : next.publications == 2 ? 1 : next.publications == 4 ? 2 : 3;
            unsigned target = e.target * 4 + mask;
            bool changed;
            if (!in[target]) { in[target] = new state (objects.length (), cells.length ()); in[target]->copy (next); changed = true; }
            else changed = merge (*in[target], next);
            if (changed && !queued[target]) { work.safe_push (target); queued[target] = 1; }
          }
      }
    bool ok = true;
    for (unsigned n = 0; n < in.length (); ++n) if (in[n] && ok) ok = inspect (n / 4, *in[n]);
    if (!ok && flag_transaction_debug)
      { fputs ("NEWTM lifetime: ", stderr); if (bad) print_gimple_stmt (stderr, bad, 0, TDF_NONE); }
    for (state *s : in) delete s;
    return ok;
  }
};
newtm_lifetime_analysis::newtm_lifetime_analysis (const newtm_flow_graph &f, tree fn, bool current) : m_builder (new newtm_lifetime_builder (f, fn, current)) {}
newtm_lifetime_analysis::~newtm_lifetime_analysis () { delete m_builder; }
void newtm_lifetime_analysis::watch (tree t) { m_builder->watch (t); }
void newtm_lifetime_analysis::borrowed (tree t)
{ unsigned i = m_builder->add (t, 0); if (i != UINT_MAX) m_builder->objects[i].borrowed = true; }
void newtm_lifetime_analysis::readonly (tree t)
{
  borrowed (t);
  unsigned *i = m_builder->indices.get (t);
  if (i) m_builder->readonly_objects.safe_push (*i);
}
void newtm_lifetime_analysis::transport_only () { m_builder->transports_only = true; }
void newtm_lifetime_analysis::borrow (gimple *g, unsigned n)
{ m_builder->borrows.safe_push (std::make_pair (g, n)); }
void newtm_lifetime_analysis::effects (gimple *g, unsigned first, unsigned end)
{ call_effect e = { g, first, end }; m_builder->effects.safe_push (e); }
void newtm_lifetime_analysis::input (gimple *g, tree ref, HOST_WIDE_INT size)
{ requirement r = { g, ref, size < 0 ? size : size * BITS_PER_UNIT }; m_builder->inputs.safe_push (r); }
void newtm_lifetime_analysis::output (gimple *g, tree ref, bool pointer, bool binding, unsigned args)
{ m_builder->watch (ref); production p = { g, ref, pointer, binding, args }; m_builder->outputs.safe_push (p); }
void newtm_lifetime_analysis::publication (tree v, tree b) { m_builder->value_out = v; m_builder->binding_out = b; }
bool newtm_lifetime_analysis::verify (const char **why, gimple **bad)
{
  bool ok = m_builder->verify ();
  if (why) *why = m_builder->reason;
  if (bad) *bad = m_builder->bad;
  return ok;
}
