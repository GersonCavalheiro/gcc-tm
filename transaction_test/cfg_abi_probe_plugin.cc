/* Corrupt an actual call after OpenMP expansion, before the production audit. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "fold-const.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "cgraph.h"
#include "diagnostic-core.h"
#include "newtm-clone.h"
#include "internal-fn.h"
int plugin_is_GPL_compatible;
static bool changed;
static const char *mode = "abi";
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_cfg_abi_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass {
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override {
    if (!strncmp (mode, "offset_", 7)) {
      tree original = newtm_clone_original (fn->decl);
      if (!original || !DECL_NAME (original) || strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), "element")) return 0;
      basic_block block;
      FOR_EACH_BB_FN (block, fn)
        for (gimple_stmt_iterator it = gsi_start_bb (block); !gsi_end_p (it); gsi_next (&it)) {
          gimple *stmt = gsi_stmt (it);
          if (changed) continue;
          if (!strcmp (mode, "offset_unchecked") && is_gimple_call (stmt) && gimple_call_internal_p (stmt)
              && gimple_call_internal_fn (stmt) == IFN_MUL_OVERFLOW) {
            tree pair = gimple_call_lhs (stmt), part = TREE_TYPE (TREE_TYPE (pair));
            tree operands[2];
            for (unsigned i = 0; i < 2; ++i) {
              operands[i] = gimple_call_arg (stmt, i);
              if (!types_compatible_p (TREE_TYPE (operands[i]), part)) {
                tree converted = create_tmp_var (part);
                gsi_insert_before (&it, gimple_build_assign (converted, NOP_EXPR, operands[i]), GSI_SAME_STMT);
                operands[i] = converted;
              }
            }
            tree product = create_tmp_var (part);
            gsi_insert_before (&it, gimple_build_assign (product, MULT_EXPR, operands[0], operands[1]), GSI_SAME_STMT);
            gsi_replace (&it, gimple_build_assign (pair, COMPLEX_EXPR, product, build_zero_cst (part)), false);
            changed = true;
          } else if (!strcmp (mode, "offset_operator") && is_gimple_call (stmt) && gimple_call_internal_p (stmt)
              && gimple_call_internal_fn (stmt) == IFN_MUL_OVERFLOW) {
            gimple_call_set_internal_fn (as_a<gcall *> (stmt), IFN_ADD_OVERFLOW); changed = true;
          } else if ((!strcmp (mode, "offset_flag") || !strcmp (mode, "offset_projection")) && is_gimple_assign (stmt)) {
            enum tree_code code = gimple_assign_rhs_code (stmt);
            if (!strcmp (mode, "offset_flag") && code == IMAGPART_EXPR) {
              gimple_assign_set_rhs_from_tree (&it, build_zero_cst (TREE_TYPE (gimple_assign_lhs (stmt)))); changed = true;
            } else if (!strcmp (mode, "offset_projection") && code == REALPART_EXPR) {
              tree pair = TREE_OPERAND (gimple_assign_rhs1 (stmt), 0);
              gimple_assign_set_rhs_from_tree (&it, build1 (IMAGPART_EXPR, TREE_TYPE (gimple_assign_lhs (stmt)), pair)); changed = true;
            }
          } else if ((!strcmp (mode, "offset_bypass") || !strcmp (mode, "offset_copy"))
                     && is_gimple_call (stmt) && gimple_call_fndecl (stmt)
                     && fndecl_built_in_p (gimple_call_fndecl (stmt), BUILT_IN_GOMP_TRANSACTION_STM_V1_READ)) {
            tree value = gimple_call_arg (stmt, 2);
            if (!strcmp (mode, "offset_copy")) {
              tree copy = create_tmp_var (TREE_TYPE (value));
              gsi_insert_before (&it, gimple_build_assign (copy, value), GSI_SAME_STMT);
              gimple_call_set_arg (stmt, 2, copy);
            } else gimple_call_set_arg (stmt, 2, build_zero_cst (TREE_TYPE (value)));
            changed = true;
          }
        }
      return 0;
    }
    if (!strncmp (mode, "memory_", 7)) {
      if (!newtm_clone_original (fn->decl)) return 0;
      if (!strcmp (mode, "memory_insert")) {
        tree original = newtm_clone_original (fn->decl);
        if (changed || !DECL_NAME (original) || strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), "pure")) return 0;
        tree object = NULL_TREE; varpool_node *variable;
        FOR_EACH_VARIABLE (variable)
          if (DECL_NAME (variable->decl) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (variable->decl)), "shared_value")) object = variable->decl;
        if (!object) return 0;
        basic_block block;
        FOR_EACH_BB_FN (block, fn) {
          gimple_stmt_iterator it = gsi_start_bb (block);
          if (gsi_end_p (it)) continue;
          tree buffer = create_tmp_var (integer_type_node), status = create_tmp_var (uint32_type_node);
          TREE_ADDRESSABLE (buffer) = 1;
          gcall *call = gimple_build_call (builtin_decl_explicit (BUILT_IN_GOMP_TRANSACTION_STM_V1_READ), 5,
            DECL_ARGUMENTS (fn->decl), build_fold_addr_expr (object), size_zero_node,
            build_fold_addr_expr (buffer), TYPE_SIZE_UNIT (integer_type_node));
          gimple_call_set_lhs (call, status); gsi_insert_before (&it, call, GSI_SAME_STMT);
          changed = true; break;
        }
        return 0;
      }
      if (!strcmp (mode, "memory_rebind")) {
        bool reads = false; basic_block candidate;
        FOR_EACH_BB_FN (candidate, fn)
          for (gimple_stmt_iterator it = gsi_start_bb (candidate); !gsi_end_p (it); gsi_next (&it)) {
            gimple *stmt = gsi_stmt (it);
            if (is_gimple_call (stmt) && gimple_call_fndecl (stmt)
                && fndecl_built_in_p (gimple_call_fndecl (stmt), BUILT_IN_GOMP_TRANSACTION_STM_V1_READ)) reads = true;
          }
        if (!reads) return 0;
        tree other = NULL_TREE; varpool_node *variable;
        FOR_EACH_VARIABLE (variable)
          if (DECL_NAME (variable->decl) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (variable->decl)), "other_value")) other = variable->decl;
        if (!other) return 0;
        basic_block block;
        FOR_EACH_BB_FN (block, fn)
          for (gimple_stmt_iterator it = gsi_start_bb (block); !gsi_end_p (it); gsi_next (&it)) {
            struct rebind {
              static tree visit (tree *value, int *descend, void *data) {
                walk_stmt_info *wi = (walk_stmt_info *) data;
                if (TREE_CODE (*value) == ADDR_EXPR) {
                  tree object = TREE_OPERAND (*value, 0);
                  if (VAR_P (object) && DECL_NAME (object) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (object)), "shared_value")) {
                    *value = fold_convert (TREE_TYPE (*value), build_fold_addr_expr ((tree) wi->info)); changed = true;
                  }
                }
                if (DECL_P (*value) || TYPE_P (*value)) *descend = 0;
                return NULL_TREE;
              }
            };
            walk_stmt_info wi = {}; wi.info = other;
            walk_gimple_op (gsi_stmt (it), rebind::visit, &wi);
          }
        return 0;
      }
      basic_block block;
      FOR_EACH_BB_FN (block, fn)
        for (gimple_stmt_iterator it = gsi_start_bb (block); !gsi_end_p (it); gsi_next (&it)) {
          gimple *stmt = gsi_stmt (it);
          if (changed || !is_gimple_call (stmt) || !gimple_call_fndecl (stmt)) continue;
          tree callee = gimple_call_fndecl (stmt);
          if (!fndecl_built_in_p (callee, BUILT_IN_GOMP_TRANSACTION_STM_V1_READ)) continue;
          if (!strcmp (mode, "memory_duplicate")) gsi_insert_before (&it, gimple_copy (stmt), GSI_SAME_STMT);
          else if (!strcmp (mode, "memory_operation")) gimple_call_set_fndecl (stmt, builtin_decl_explicit (BUILT_IN_GOMP_TRANSACTION_STM_V1_WRITE));
          else if (!strcmp (mode, "memory_copy")) {
            tree value = gimple_call_arg (stmt, 1), copy = create_tmp_var (TREE_TYPE (value));
            gsi_insert_before (&it, gimple_build_assign (copy, value), GSI_SAME_STMT);
            gimple_call_set_arg (stmt, 1, copy);
          } else {
            unsigned arg = !strcmp (mode, "memory_size") ? 4 : 2;
            tree type = TREE_TYPE (gimple_call_arg (stmt, arg));
            gimple_call_set_arg (stmt, arg, !strcmp (mode, "memory_offset_max") ? TYPE_MAX_VALUE (type) : build_int_cst (type, arg == 4 ? 3 : 1));
          }
          changed = true;
        }
      return 0;
    }
    if (!strncmp (mode, "registration_", 13)) {
      if (!newtm_clone_original (fn->decl)) return 0;
      if (!strcmp (mode, "registration_rebind")) {
        tree other = NULL_TREE; varpool_node *variable;
        FOR_EACH_VARIABLE (variable)
          if (DECL_NAME (variable->decl) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (variable->decl)), "other_value")) other = variable->decl;
        if (!other) return 0;
        basic_block block;
        FOR_EACH_BB_FN (block, fn)
          for (gimple_stmt_iterator it = gsi_start_bb (block); !gsi_end_p (it); gsi_next (&it)) {
            struct rebind {
              static tree visit (tree *value, int *descend, void *data) {
                walk_stmt_info *wi = (walk_stmt_info *) data;
                if (TREE_CODE (*value) == ADDR_EXPR) {
                  tree object = TREE_OPERAND (*value, 0);
                  if (VAR_P (object) && DECL_NAME (object) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (object)), "shared_value")) {
                    *value = fold_convert (TREE_TYPE (*value), build_fold_addr_expr ((tree) wi->info)); changed = true;
                  }
                }
                if (DECL_P (*value) || TYPE_P (*value)) *descend = 0;
                return NULL_TREE;
              }
            };
            walk_stmt_info wi = {}; wi.info = other;
            walk_gimple_op (gsi_stmt (it), rebind::visit, &wi);
          }
        return 0;
      }
      basic_block block;
      FOR_EACH_BB_FN (block, fn)
        for (gimple_stmt_iterator it = gsi_start_bb (block); !gsi_end_p (it); gsi_next (&it)) {
          gimple *stmt = gsi_stmt (it);
          if (changed || !is_gimple_call (stmt) || !gimple_call_fndecl (stmt)
              || !fndecl_built_in_p (gimple_call_fndecl (stmt), BUILT_IN_GOMP_TRANSACTION_STM_V1_REGISTER)) continue;
          if (!strcmp (mode, "registration_self_copy") || !strcmp (mode, "registration_self_offset")) {
            tree value = gimple_call_arg (stmt, 1);
            if (!VAR_P (value)) continue;
            if (!strcmp (mode, "registration_self_copy"))
              gsi_insert_before (&it, gimple_build_assign (value, value), GSI_SAME_STMT);
            else gsi_insert_before (&it, gimple_build_assign (value, POINTER_PLUS_EXPR, value, size_one_node), GSI_SAME_STMT);
            changed = true; continue;
          }
          if (!strcmp (mode, "registration_copy")) {
            tree value = gimple_call_arg (stmt, 1), copy = create_tmp_var (TREE_TYPE (value));
            gsi_insert_before (&it, gimple_build_assign (copy, value), GSI_SAME_STMT);
            gimple_call_set_arg (stmt, 1, copy); changed = true; continue;
          }
          if (!strcmp (mode, "registration_duplicate")) {
            gsi_insert_before (&it, gimple_copy (stmt), GSI_SAME_STMT); changed = true; continue;
          }
          unsigned index = !strcmp (mode, "registration_object") ? 1
            : (!strcmp (mode, "registration_kind") || !strcmp (mode, "registration_valid_kind")) ? 3 : 2;
          tree type = TREE_TYPE (gimple_call_arg (stmt, index));
          tree value = !strcmp (mode, "registration_valid_kind") ? build_int_cst (type, 1)
            : !strcmp (mode, "registration_valid_extent") ? build_int_cst (type, 3)
            : !strcmp (mode, "registration_kind") ? build_int_cst (type, 2)
            : !strcmp (mode, "registration_extent_max") ? TYPE_MAX_VALUE (type) : build_zero_cst (type);
          gimple_call_set_arg (stmt, index, value); changed = true;
        }
      return 0;
    }
    if (!strcmp (mode, "formal_swap")) {
      if (!changed && newtm_clone_original (fn->decl) && newtm_clone_region_owner (fn->decl)) {
        tree first = DECL_ARGUMENTS (fn->decl), second = first ? DECL_CHAIN (first) : NULL_TREE;
        tree left = second ? DECL_CHAIN (second) : NULL_TREE, right = left ? DECL_CHAIN (left) : NULL_TREE;
        if (left && right && types_compatible_p (TREE_TYPE (left), TREE_TYPE (right))) {
          DECL_CHAIN (second) = right; DECL_CHAIN (left) = DECL_CHAIN (right); DECL_CHAIN (right) = left;
          changed = true;
        }
      }
      return 0;
    }
    if (!strcmp (mode, "parameter_owner")) {
      if (!changed && newtm_clone_original (fn->decl) && DECL_ARGUMENTS (fn->decl)) {
        DECL_CONTEXT (DECL_ARGUMENTS (fn->decl)) = newtm_clone_original (fn->decl);
        changed = true;
      }
      return 0;
    }
    if (!strcmp (mode, "frame_field")) {
      if (!newtm_clone_original (fn->decl)) return 0;
      basic_block block;
      FOR_EACH_BB_FN (block, fn)
        for (gimple_stmt_iterator it = gsi_start_bb (block); !gsi_end_p (it); gsi_next (&it)) {
          struct field_probe {
            static tree visit (tree *node, int *descend, void *) {
              if (!changed && TREE_CODE (*node) == COMPONENT_REF) {
                TREE_OPERAND (*node, 1) = copy_node (TREE_OPERAND (*node, 1));
                changed = true;
              }
              if (DECL_P (*node) || TYPE_P (*node)) *descend = 0;
              return NULL_TREE;
            }
          };
          walk_stmt_info wi = {};
          walk_gimple_op (gsi_stmt (it), field_probe::visit, &wi);
        }
      return 0;
    }
    if (!strcmp (mode, "capture_store_swap")) {
      cgraph_node *node;
      FOR_EACH_DEFINED_FUNCTION (node) {
        function *body = DECL_STRUCT_FUNCTION (node->decl);
        if (!body || !body->cfg || changed) continue;
        gimple *left = NULL; tree right = NULL_TREE;
        basic_block block;
        FOR_EACH_BB_FN (block, body)
          for (gimple_stmt_iterator it = gsi_start_bb (block); !gsi_end_p (it); gsi_next (&it)) {
            gimple *stmt = gsi_stmt (it);
            if (!is_gimple_assign (stmt) || !gimple_assign_single_p (stmt)) continue;
            tree lhs = gimple_assign_lhs (stmt);
            if (TREE_CODE (lhs) != COMPONENT_REF) continue;
            tree field = TREE_OPERAND (lhs, 1);
            if (!DECL_NAME (field)) continue;
            if (!strcmp (IDENTIFIER_POINTER (DECL_NAME (field)), "left")) left = stmt;
            if (!strcmp (IDENTIFIER_POINTER (DECL_NAME (field)), "right")) right = gimple_assign_rhs1 (stmt);
          }
        if (left && right) { gimple_assign_set_rhs1 (left, right); changed = true; }
      }
      return 0;
    }
    if (!strcmp (mode, "capture_field_swap")) {
      cgraph_node *node;
      FOR_EACH_DEFINED_FUNCTION (node) {
        function *body = DECL_STRUCT_FUNCTION (node->decl);
        if (!body || !body->cfg || !node->parallelized_function) continue;
        basic_block block;
        FOR_EACH_BB_FN (block, body)
          for (gimple_stmt_iterator it = gsi_start_bb (block); !gsi_end_p (it); gsi_next (&it)) {
            struct swap_field {
              static tree visit (tree *value, int *descend, void *) {
                if (TREE_CODE (*value) == COMPONENT_REF) {
                  tree field = TREE_OPERAND (*value, 1);
                  if (DECL_NAME (field) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (field)), "left"))
                    for (tree other = TYPE_FIELDS (TREE_TYPE (TREE_OPERAND (*value, 0))); other; other = DECL_CHAIN (other))
                      if (other != field && TREE_CODE (other) == FIELD_DECL && types_compatible_p (TREE_TYPE (other), TREE_TYPE (field))) {
                        TREE_OPERAND (*value, 1) = other; changed = true; break;
                      }
                }
                if (DECL_P (*value) || TYPE_P (*value)) *descend = 0;
                return NULL_TREE;
              }
            };
            walk_stmt_info wi = {};
            walk_gimple_op (gsi_stmt (it), swap_field::visit, &wi);
          }
      }
      return 0;
    }
    if (!strncmp (mode, "controller_", 11)) {
      cgraph_node *node;
      FOR_EACH_DEFINED_FUNCTION (node) {
        function *body = DECL_STRUCT_FUNCTION (node->decl);
        if (!body || !body->cfg || newtm_clone_original (node->decl)) continue;
        basic_block block;
        FOR_EACH_BB_FN (block, body)
          for (gimple_stmt_iterator it = gsi_start_bb (block); !gsi_end_p (it); gsi_next (&it)) {
            gimple *stmt = gsi_stmt (it);
            if (changed || !is_gimple_call (stmt)) continue;
            tree callee = gimple_call_fndecl (stmt);
            if (!callee || !newtm_clone_original (callee) || !newtm_clone_region_owner (callee)) continue;
            if (!strcmp (mode, "controller_capture_truncate")) {
              if (node->decl != fn->decl || gimple_call_num_args (stmt) < 3) continue;
              tree value = gimple_call_arg (stmt, 2);
              tree small = create_tmp_var (short_unsigned_type_node), result = create_tmp_var (TREE_TYPE (value));
              gsi_insert_before (&it, gimple_build_assign (small, NOP_EXPR, value), GSI_SAME_STMT);
              gsi_insert_before (&it, gimple_build_assign (result, NOP_EXPR, small), GSI_SAME_STMT);
              gimple_call_set_arg (stmt, 2, result);
            } else if (!strcmp (mode, "controller_capture_swap")) {
              if (gimple_call_num_args (stmt) < 4) continue;
              tree first = gimple_call_arg (stmt, 2), second = gimple_call_arg (stmt, 3);
              if (!types_compatible_p (TREE_TYPE (first), TREE_TYPE (second))) continue;
              gimple_call_set_arg (stmt, 2, second); gimple_call_set_arg (stmt, 3, first);
            } else if (!strcmp (mode, "controller_remove")) { gsi_remove (&it, true); changed = true; return 0; }
            else if (!strcmp (mode, "controller_duplicate")) gsi_insert_before (&it, gimple_copy (stmt), GSI_SAME_STMT);
            else if (!strcmp (mode, "controller_channels")) gimple_call_set_arg (stmt, 0, build_zero_cst (integer_type_node));
            else if (!strcmp (mode, "controller_owner")) {
              if (!node->parallelized_function) continue;
              newtm_clone_record_outline (node->decl, callee);
            }
            changed = true;
          }
      }
      return 0;
    }
    basic_block bb;
    FOR_EACH_BB_FN (bb, fn)
      for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it)) {
        gimple *stmt = gsi_stmt (it);
        if (changed || !is_gimple_call (stmt)) continue;
        tree callee = gimple_call_fndecl (stmt);
        if ((!strcmp (mode, "context") || !strcmp (mode, "context_copy")
             || !strcmp (mode, "context_overwrite") || !strcmp (mode, "context_alias")) && newtm_clone_original (fn->decl)
            && callee && newtm_clone_original (callee)) {
          tree value = gimple_call_arg (stmt, 0);
          if (!strcmp (mode, "context_alias")) {
            tree copy = create_tmp_var (TREE_TYPE (value));
            tree pointer = create_tmp_var (build_pointer_type (TREE_TYPE (value)));
            TREE_ADDRESSABLE (copy) = 1;
            gsi_insert_before (&it, gimple_build_assign (copy, value), GSI_SAME_STMT);
            gsi_insert_before (&it, gimple_build_assign (pointer, build_fold_addr_expr (copy)), GSI_SAME_STMT);
            tree memory = build2 (MEM_REF, TREE_TYPE (value), pointer, build_int_cst (TREE_TYPE (pointer), 0));
            gsi_insert_before (&it, gimple_build_assign (memory, build_zero_cst (TREE_TYPE (value))), GSI_SAME_STMT);
            tree loaded = create_tmp_var (TREE_TYPE (value));
            gsi_insert_before (&it, gimple_build_assign (loaded, copy), GSI_SAME_STMT);
            gimple_call_set_arg (stmt, 0, loaded);
          } else if (!strcmp (mode, "context_copy")) {
            tree copy = create_tmp_var (TREE_TYPE (value));
            gsi_insert_before (&it, gimple_build_assign (copy, value), GSI_SAME_STMT);
            gimple_call_set_arg (stmt, 0, copy);
          } else if (!strcmp (mode, "context_overwrite")) {
            tree parameter = DECL_ARGUMENTS (fn->decl);
            gsi_insert_before (&it, gimple_build_assign (parameter, build_zero_cst (TREE_TYPE (parameter))), GSI_SAME_STMT);
          } else gimple_call_set_arg (stmt, 0, build_zero_cst (TREE_TYPE (value)));
          changed = true;
          continue;
        }
        if (!strncmp (mode, "context", 7)) continue;
        if (!strcmp (mode, "channels") && newtm_clone_original (fn->decl)
            && callee && newtm_clone_original (callee)) {
          gimple_call_set_arg (stmt, 0, build_zero_cst (integer_type_node));
          changed = true;
          continue;
        }
        if (!strcmp (mode, "channels")) continue;
        if (callee && fndecl_built_in_p (callee, BUILT_IN_NORMAL)
            && DECL_FUNCTION_CODE (callee) == BUILT_IN_GOMP_TRANSACTION_STM_V1_READ) {
          if (!strcmp (mode, "target"))
            gimple_call_set_fndecl (stmt, build_fn_decl ("injected_unprepared", TREE_TYPE (callee)));
          else if (!strcmp (mode, "indirect"))
            gimple_call_set_fn (as_a<gcall *> (stmt), build_zero_cst (build_pointer_type (TREE_TYPE (callee))));
          else gimple_call_set_arg (stmt, 2, build_zero_cst (ptr_type_node));
          changed = true;
        }
      }
    return 0;
  }
};
}
static void finish (void *, void *) { if (!changed && !seen_error ()) error ("CFG probe did not find its target call"); }
int plugin_init (plugin_name_args *a, plugin_gcc_version *v) {
  if (!plugin_default_version_check (v, &gcc_version)) return 1;
  for (int i = 0; i < a->argc; ++i)
    if (!strcmp (a->argv[i].key, "mode")) mode = a->argv[i].value;
  register_callback (a->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  register_pass_info info = { new probe (g), "ompexp", 1, PASS_POS_INSERT_AFTER };
  register_callback (a->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &info);
  return 0;
}
