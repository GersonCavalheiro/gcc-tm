/* Mutate actual memory identities after outlining.  */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimplify.h"
#include "gimple-iterator.h"
#include "gimplify-me.h"
#include "gimple-walk.h"
#include "fold-const.h"
#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "cgraph.h"
#include "diagnostic-core.h"
#include "internal-fn.h"
#include "newtm-clone.h"
#include "newtm-memory.h"
int plugin_is_GPL_compatible;
static const char *mode = "observe";
static bool changed;
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_memory_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
static bool named (tree t, const char *name)
{ return t && DECL_P (t) && DECL_NAME (t) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (t)), name); }
struct probe : gimple_opt_pass {
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (changed || !strcmp (mode, "observe")) return 0;
    tree original = newtm_clone_original (fn->decl);
    if (!original) return 0;
    if (!strncmp (mode, "registration_", 13))
      { if (!DECL_NAME (original) || !strstr (IDENTIFIER_POINTER (DECL_NAME (original)), "dynamic_registration_root")) return 0; }
    else if (!strncmp (mode, "frame_", 6)) {}
    else if (!strcmp (mode, "private_instance")) { if (!named (original, "private_pair")) return 0; }
    else if (!strcmp (mode, "private_clobber")) { if (!named (original, "private_zero")) return 0; }
    else if (!strcmp (mode, "real_constant")) { if (!named (original, "real_value")) return 0; }
    else if (!strcmp (mode, "choice")) { if (!named (original, "conditional_index")) return 0; }
    else if (!strcmp (mode, "switch_choice")) { if (!named (original, "switch_index")) return 0; }
    else if (strcmp (mode, "source_layout") && strcmp (mode, "forged_global") && !named (original, "transfer")) return 0;
    basic_block bb;
    if (!strcmp (mode, "forged_global"))
      {
        tree source = NULL_TREE, replacement = NULL_TREE; varpool_node *node;
        FOR_EACH_VARIABLE (node)
          { if (named (node->decl, "left")) source = node->decl;
            if (named (node->decl, "unrelated")) replacement = node->decl; }
        if (!source || !replacement) return 0;
        struct rebind_global {
          tree source, replacement;
          static tree visit (tree *p, int *walk, void *data)
          {
            walk_stmt_info *wi = (walk_stmt_info *) data;
            rebind_global *r = (rebind_global *) wi->info;
            if (*p == r->source) { *p = r->replacement; changed = true; }
            if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
            return NULL_TREE;
          }
        } state = { source, replacement };
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
            { walk_stmt_info wi = {}; wi.info = &state; walk_gimple_op (gsi_stmt (at), rebind_global::visit, &wi); }
        if (changed) { TREE_ADDRESSABLE (replacement) = 1; newtm_clone_record_decl (replacement, source); }
        return 0;
      }
    if (!strcmp (mode, "foreign_binding"))
      {
        tree input = DECL_CHAIN (DECL_ARGUMENTS (fn->decl));
        tree foreign = build_decl (UNKNOWN_LOCATION, VAR_DECL, NULL_TREE, TREE_TYPE (input));
        DECL_CONTEXT (foreign) = original;
        /* Forge only declaration correspondence, never a valid activation. */
        newtm_clone_record_decl (foreign, input);
        struct rewrite {
          tree from, to;
          static tree visit (tree *p, int *walk, void *data)
          {
            walk_stmt_info *wi = (walk_stmt_info *) data;
            rewrite *r = (rewrite *) wi->info;
            if (*p == r->from) *p = r->to;
            if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
            return NULL_TREE;
          }
        };
        rewrite state = { input, foreign };
        for (unsigned pass = 0; pass < 2; ++pass)
          {
            FOR_EACH_BB_FN (bb, fn)
              for (gimple_stmt_iterator at = gsi_start_bb (bb); !gsi_end_p (at); gsi_next (&at))
                { walk_stmt_info wi = {}; wi.info = &state; walk_gimple_op (gsi_stmt (at), rewrite::visit, &wi); }
            if (!pass)
              {
                if (newtm_memory_verify_cfg (fn->decl)) error ("foreign activation was accepted");
                fputs ("MEMORY_FOREIGN_ACTIVATION refused\n", stderr);
                state.from = foreign; state.to = input;
              }
          }
        changed = true; return 0;
      }
    if (!strcmp (mode, "private_instance"))
      {
        tree other = NULL_TREE, local; unsigned i;
        FOR_EACH_LOCAL_DECL (fn, i, local) if (named (local, "b")) other = local;
        if (!other) return 0;
        struct rebind {
          static tree visit (tree *p, int *walk, void *data)
          {
            walk_stmt_info *wi = (walk_stmt_info *) data;
            if (TREE_CODE (*p) == ADDR_EXPR && named (get_base_address (TREE_OPERAND (*p, 0)), "a"))
              {
                tree other = (tree) wi->info, ref = other;
                if (!types_compatible_p (TREE_TYPE (TREE_TYPE (*p)), TREE_TYPE (other)))
                  ref = build4 (ARRAY_REF, TREE_TYPE (TREE_TYPE (other)), other, size_zero_node, NULL_TREE, NULL_TREE);
                *p = build1 (ADDR_EXPR, TREE_TYPE (*p), ref); changed = true;
              }
            if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
            return NULL_TREE;
          }
        };
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
            { walk_stmt_info wi = {}; wi.info = other; walk_gimple_op (gsi_stmt (it), rebind::visit, &wi); }
        return 0;
      }
    if (!strncmp (mode, "frame_", 6))
      {
        struct frame_mutation {
          gimple_stmt_iterator *where;
          static tree visit (tree *p, int *walk, void *data)
          {
            walk_stmt_info *wi = (walk_stmt_info *) data;
            frame_mutation *self = (frame_mutation *) wi->info;
            if (!changed && TREE_CODE (*p) == COMPONENT_REF
                && newtm_clone_memory_field (TREE_OPERAND (*p, 1)))
              {
                tree field = TREE_OPERAND (*p, 1), base = TREE_OPERAND (*p, 0);
                if (!strcmp (mode, "frame_root") && TREE_CODE (base) == MEM_REF)
                  {
                    tree type = TREE_TYPE (TREE_OPERAND (base, 0)), replacement = create_tmp_var (type);
                    gsi_insert_before (self->where, gimple_build_assign (replacement, build_zero_cst (type)), GSI_SAME_STMT);
                    TREE_OPERAND (base, 0) = replacement; changed = true;
                  }
                if (!strcmp (mode, "frame_overlap") && DECL_FIELD_OFFSET (field) && DECL_FIELD_BIT_OFFSET (field)
                    && (!integer_zerop (DECL_FIELD_OFFSET (field)) || !integer_zerop (DECL_FIELD_BIT_OFFSET (field))))
                  { DECL_FIELD_OFFSET (field) = size_zero_node; DECL_FIELD_BIT_OFFSET (field) = bitsize_zero_node; changed = true; }
                if (!strcmp (mode, "frame_alignment"))
                  { SET_DECL_ALIGN (field, TYPE_ALIGN (TREE_TYPE (base)) * 2); changed = true; }
              }
            if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
            return NULL_TREE;
          }
        };
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
            { frame_mutation state = { &it }; walk_stmt_info wi = {}; wi.info = &state; walk_gimple_op (gsi_stmt (it), frame_mutation::visit, &wi); }
        return 0;
      }
    if (!strcmp (mode, "source_layout"))
      {
        struct mutate_layout {
          static tree visit (tree *p, int *walk, void *)
          {
            if (TREE_CODE (*p) == COMPONENT_REF)
              {
                tree field = TREE_OPERAND (*p, 1);
                if (named (field, "second"))
                  { DECL_FIELD_OFFSET (field) = size_zero_node; DECL_FIELD_BIT_OFFSET (field) = bitsize_zero_node; changed = true; }
              }
            if (DECL_P (*p) || TYPE_P (*p)) *walk = 0;
            return NULL_TREE;
          }
        };
        FOR_EACH_BB_FN (bb, fn)
          for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
            { walk_stmt_info wi = {}; walk_gimple_op (gsi_stmt (it), mutate_layout::visit, &wi); }
        return 0;
      }
    FOR_EACH_BB_FN (bb, fn)
      for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
        {
          gimple *s = gsi_stmt (it);
          if (changed && strncmp (mode, "field_", 6)) continue;
          if (!strcmp (mode, "switch_choice") && gimple_code (s) == GIMPLE_SWITCH)
            {
              gswitch *selection = as_a<gswitch *> (s);
              if (gimple_switch_num_labels (selection) < 3) continue;
              tree a = gimple_switch_label (selection, 1), b = gimple_switch_label (selection, 2);
              tree label = CASE_LABEL (a); CASE_LABEL (a) = CASE_LABEL (b); CASE_LABEL (b) = label;
              changed = true;
            }
          else if (!strcmp (mode, "choice") && gimple_code (s) == GIMPLE_COND)
            {
              gcond *cond = as_a<gcond *> (s);
              if (named (gimple_cond_lhs (cond), "choose") || named (gimple_cond_rhs (cond), "choose"))
                { gimple_cond_set_code (cond, invert_tree_comparison (gimple_cond_code (cond), false)); changed = true; }
            }
          else if (!strcmp (mode, "real_constant") && is_gimple_assign (s))
            {
              tree rhs = gimple_assign_rhs1 (s);
              if (TREE_CODE (rhs) == REAL_CST)
                { gimple_assign_set_rhs_from_tree (&it, build_zero_cst (TREE_TYPE (rhs))); changed = true; }
              else if (TREE_CODE (rhs) == CONSTRUCTOR && CONSTRUCTOR_NELTS (rhs))
                {
                  constructor_elt *elt = CONSTRUCTOR_ELT (rhs, 0);
                  if (TREE_CODE (elt->value) == REAL_CST)
                    { elt->value = build_zero_cst (TREE_TYPE (elt->value)); changed = true; }
                }
            }
          else if (!strcmp (mode, "private_clobber") && is_gimple_assign (s)
              && gimple_assign_rhs_code (s) == CONSTRUCTOR && !gimple_clobber_p (s))
            {
              gimple_assign_set_rhs_from_tree (&it, build_clobber (TREE_TYPE (gimple_assign_lhs (s))));
              changed = true;
            }
          else if (!strncmp (mode, "field_", 6) && is_gimple_assign (s)
              && gimple_assign_single_p (s) && TREE_CODE (gimple_assign_rhs1 (s)) == COMPONENT_REF)
            {
              tree ref = gimple_assign_rhs1 (s), field = TREE_OPERAND (ref, 1);
              const char *name = mode + 6;
              if (!named (field, name)) continue;
              if (!strcmp (name, "storage") || !strcmp (name, "kind") || !strcmp (name, "object_size"))
                {
                  unsigned value = !strcmp (name, "storage") ? 1 : !strcmp (name, "kind") ? 0 : 16;
                  gimple_assign_set_rhs_from_tree (&it, build_int_cst (TREE_TYPE (ref), value));
                }
              else if (!strcmp (name, "object"))
                {
                  /* Keep the field and its type, select the other incoming
                     binding at the same byte offset in the next slot. */
                  tree copy = unshare_expr (ref), base = TREE_OPERAND (copy, 0);
                  while (TREE_CODE (base) == COMPONENT_REF) base = TREE_OPERAND (base, 0);
                  if (TREE_CODE (base) != MEM_REF) continue;
                  tree pointer = TREE_OPERAND (base, 0), next = create_tmp_var (TREE_TYPE (pointer));
                  tree element = TREE_TYPE (TREE_TYPE (pointer));
                  gsi_insert_before (&it, gimple_build_assign (next, POINTER_PLUS_EXPR,
                    pointer, fold_convert (size_type_node, TYPE_SIZE_UNIT (element))), GSI_SAME_STMT);
                  TREE_OPERAND (base, 0) = next;
                  gimple_assign_set_rhs_from_tree (&it, copy);
                }
              changed = true;
            }
          else if (!strcmp (mode, "index") && is_gimple_call (s) && gimple_call_internal_p (s)
                   && gimple_call_internal_fn (s) == IFN_MUL_OVERFLOW)
            { gimple_call_set_arg (s, 0, build_zero_cst (TREE_TYPE (gimple_call_arg (s, 0)))); changed = true; }
          else if (!strcmp (mode, "cast") && is_gimple_call (s) && gimple_call_internal_p (s)
                   && gimple_call_internal_fn (s) == IFN_MUL_OVERFLOW)
            {
              for (unsigned i = 0; i < gimple_call_num_args (s); ++i)
                {
                  tree v = gimple_call_arg (s, i);
                  if (TREE_CODE (v) == INTEGER_CST || !INTEGRAL_TYPE_P (TREE_TYPE (v))) continue;
                  tree narrow = create_tmp_var (unsigned_char_type_node), wide = create_tmp_var (TREE_TYPE (v));
                  gsi_insert_before (&it, gimple_build_assign (narrow, NOP_EXPR, v), GSI_SAME_STMT);
                  gsi_insert_before (&it, gimple_build_assign (wide, NOP_EXPR, narrow), GSI_SAME_STMT);
                  gimple_call_set_arg (s, i, wide); changed = true; break;
                }
            }
          else if (!strcmp (mode, "early_address") && is_gimple_assign (s)
                   && gimple_assign_rhs_code (s) == POINTER_PLUS_EXPR)
            {
              basic_block entry = single_succ (ENTRY_BLOCK_PTR_FOR_FN (fn));
              gimple_stmt_iterator start = gsi_after_labels (entry);
              gsi_remove (&it, false); gsi_insert_before (&start, s, GSI_SAME_STMT);
              changed = true; return 0;
            }
          else if (!strncmp (mode, "registration_", 13) && is_gimple_call (s) && gimple_call_fndecl (s)
                   && fndecl_built_in_p (gimple_call_fndecl (s), BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER))
            {
              unsigned arg = !strcmp (mode, "registration_extent") ? 2 : 3;
              gimple_call_set_arg (s, arg, build_int_cst (TREE_TYPE (gimple_call_arg (s, arg)), arg == 2 ? 4 : 0));
              changed = true;
            }
          else if (!strcmp (mode, "source_layout") && is_gimple_assign (s)
                   && TREE_CODE (gimple_assign_lhs (s)) == COMPONENT_REF)
            {
              tree field = TREE_OPERAND (gimple_assign_lhs (s), 1);
              if (!named (field, "second")) continue;
              DECL_FIELD_OFFSET (field) = size_zero_node;
              DECL_FIELD_BIT_OFFSET (field) = bitsize_zero_node; changed = true;
            }
          else if (!strcmp (mode, "layout") && is_gimple_assign (s)
                   && gimple_assign_single_p (s) && TREE_CODE (gimple_assign_rhs1 (s)) == COMPONENT_REF)
            {
              tree field = TREE_OPERAND (gimple_assign_rhs1 (s), 1);
              if (!named (field, "object_size")) continue;
              DECL_FIELD_OFFSET (field) = size_zero_node;
              DECL_FIELD_BIT_OFFSET (field) = bitsize_zero_node; changed = true;
              /* The ABI record is shared with controllers. Audit this body
                 directly so graph traversal order cannot select a channel
                 diagnostic from another root before this memory diagnostic. */
              if (newtm_memory_verify_cfg (fn->decl))
                error ("altered binding layout was accepted");
              return 0;
            }
          else if (!strcmp (mode, "alias") && is_gimple_assign (s)
                   && gimple_assign_single_p (s) && TREE_CODE (gimple_assign_rhs1 (s)) == COMPONENT_REF)
            {
              tree ref = gimple_assign_rhs1 (s);
              if (!named (TREE_OPERAND (ref, 1), "object")) continue;
              /* A write to the borrowed channel creates a new memory epoch,
                 even though all loads retain their declaration/type. */
              gsi_insert_after (&it, gimple_build_assign (unshare_expr (ref), gimple_assign_lhs (s)), GSI_NEW_STMT);
              changed = true;
            }
          else if (!strcmp (mode, "private_read") && is_gimple_assign (s)
                   && gimple_assign_single_p (s) && TREE_CODE (gimple_assign_rhs1 (s)) == MEM_REF
                   && INTEGRAL_TYPE_P (TREE_TYPE (gimple_assign_rhs1 (s))))
            {
              tree q = DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (DECL_ARGUMENTS (fn->decl))));
              tree ref = unshare_expr (gimple_assign_rhs1 (s));
              TREE_OPERAND (ref, 0) = fold_convert (TREE_TYPE (TREE_OPERAND (ref, 0)), q);
              gimple_assign_set_rhs_from_tree (&it, ref); changed = true;
            }
          else if (!strcmp (mode, "private_write") && is_gimple_assign (s)
                   && TREE_CODE (gimple_assign_lhs (s)) == MEM_REF
                   && INTEGRAL_TYPE_P (TREE_TYPE (gimple_assign_lhs (s))))
            {
              tree q = DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (DECL_ARGUMENTS (fn->decl))));
              tree ref = unshare_expr (gimple_assign_lhs (s));
              TREE_OPERAND (ref, 0) = fold_convert (TREE_TYPE (TREE_OPERAND (ref, 0)), q);
              gimple_assign_set_lhs (s, ref); changed = true;
            }
          else if (!strcmp (mode, "offset") && is_gimple_call (s) && gimple_call_fndecl (s)
                   && fndecl_built_in_p (gimple_call_fndecl (s), BUILT_IN_GOMP_TRANSACTION_STM_V1_READ))
            { gimple_call_set_arg (s, 2, size_zero_node); changed = true; }
          else if (!strncmp (mode, "scalar_alias", 12) && is_gimple_call (s) && gimple_call_fndecl (s)
                   && fndecl_built_in_p (gimple_call_fndecl (s), BUILT_IN_GOMP_TRANSACTION_STM_V1_READ))
            {
              tree object = gimple_call_arg (s, 1);
              if (!VAR_P (object)) continue;
              tree alias = create_tmp_var (build_pointer_type (TREE_TYPE (object)));
              gassign *address = gimple_build_assign (alias, build_fold_addr_expr (object));
              gassign *overwrite = gimple_build_assign (build_simple_mem_ref (alias), build_zero_cst (TREE_TYPE (object)));
              gsi_insert_before (&it, address, GSI_SAME_STMT);
              gsi_insert_before (&it, overwrite, GSI_SAME_STMT);
              changed = true;
              if (!strcmp (mode, "scalar_alias_raw"))
                {
                  /* Invoke the production auditor before the pass manager's
                     GIMPLE consistency check, then remove the probe. This
                     isolates discovery from TREE_ADDRESSABLE in checking 2. */
                  if (TREE_ADDRESSABLE (object)) error ("raw alias target was already addressable");
                  if (newtm_memory_verify_cfg (fn->decl)) error ("unflagged scalar alias was accepted");
                  fputs ("MEMORY_RAW_ALIAS observed_unflagged\n", stderr);
                  gimple_stmt_iterator remove = gsi_for_stmt (overwrite); gsi_remove (&remove, false);
                  remove = gsi_for_stmt (address); gsi_remove (&remove, false);
                  return 0;
                }
              TREE_ADDRESSABLE (object) = 1;
              /* Addressable scalars require loads in call/unary operands.
                 Use the compiler's own regimplifier to preserve valid IR. */
              basic_block block;
              FOR_EACH_BB_FN (block, fn)
                for (gimple_stmt_iterator at = gsi_start_bb (block); !gsi_end_p (at); gsi_next (&at))
                  if (is_gimple_assign (gsi_stmt (at)) || is_gimple_call (gsi_stmt (at))
                      || gimple_code (gsi_stmt (at)) == GIMPLE_COND)
                    gimple_regimplify_operands (gsi_stmt (at), &at);
              return 0;
            }
          else if (!strcmp (mode, "copy") && is_gimple_call (s) && gimple_call_fndecl (s)
                   && fndecl_built_in_p (gimple_call_fndecl (s), BUILT_IN_GOMP_TRANSACTION_STM_V1_READ))
            {
              tree value = gimple_call_arg (s, 1), copy = create_tmp_var (TREE_TYPE (value));
              gsi_insert_before (&it, gimple_build_assign (copy, value), GSI_SAME_STMT);
              gimple_call_set_arg (s, 1, copy); changed = true;
            }
        }
    return 0;
  }
};
static void finish (void *, void *)
{
  if (strcmp (mode, "observe"))
    { if (!changed) error ("memory probe did not mutate its target"); return; }
  unsigned operations = 0, dependencies = 0, functions = 0;
  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    {
      newtm_memory_view view;
      if (!newtm_memory_get (node->decl, &view)) continue;
      if (view.state != NEWTM_MEMORY_DEPENDENT) error ("unexpected memory certificate state");
      operations += view.operations; dependencies |= view.dependencies; ++functions;
    }
  if (!functions || !operations || dependencies != 7) error ("missing explicit memory dependencies");
  fprintf (stderr, "MEMORY_MODEL dependent functions=%u operations=%u dependencies=%u\n", functions, operations, dependencies);
}
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i) if (!strcmp (args->argv[i].key, "mode")) mode = args->argv[i].value;
  register_pass_info info = { new probe (g), "ompexp", 1, PASS_POS_INSERT_AFTER };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &info);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
