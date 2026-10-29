/* Consumidor exclusivo dos testes: observa os campos realmente calculados,
   impede executar o acesso original quando a faixa é inválida e verifica GC
   e invalidação das cópias. Não integra a ponte STM do produto. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "context.h"
#include "cgraph.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "gimplify.h"
#include "ggc.h"
#include "newtm-descriptor.h"
#include "diagnostic-core.h"
int plugin_is_GPL_compatible;
static unsigned checked;
static bool schema_only;
static unsigned roles[7], states[8];
static void survey (gimple *stmt)
{
  for (tree p = newtm_descriptor_entries (stmt); p; p = TREE_CHAIN (p))
    {
      tree d = TREE_VALUE (p);
      unsigned state = tree_to_uhwi (TREE_VEC_ELT (d, NEWTM_D_STATE));
      unsigned role = tree_to_uhwi (TREE_VEC_ELT (TREE_PURPOSE (p), 0));
      gcc_assert (state < 8 && role < 7);
      ++states[state]; ++roles[role];
      if (state <= NEWTM_DESC_UNREGISTERED)
        {
          gcc_assert (TREE_VEC_ELT (d, NEWTM_D_OBJECT)
                      && TREE_TYPE (TREE_VEC_ELT (d, NEWTM_D_OBJECT)) == ptr_type_node);
          gcc_assert (TREE_VEC_ELT (d, NEWTM_D_VALID));
          gcc_assert (TREE_TYPE (TREE_VEC_ELT (d, NEWTM_D_EXTENT)) == size_type_node);
          gcc_assert (TREE_TYPE (TREE_VEC_ELT (d, NEWTM_D_OFFSET)) == size_type_node);
          gcc_assert (TREE_TYPE (TREE_VEC_ELT (d, NEWTM_D_KIND)) == uint32_type_node);
        }
      else
        {
          gcc_assert (!TREE_VEC_ELT (d, NEWTM_D_OBJECT)
                      && !TREE_VEC_ELT (d, NEWTM_D_EXTENT)
                      && !TREE_VEC_ELT (d, NEWTM_D_KIND)
                      && !TREE_VEC_ELT (d, NEWTM_D_VALID));
          if (state == NEWTM_DESC_INCOMING || state == NEWTM_DESC_RESULT)
            gcc_assert (TREE_VEC_ELT (d, NEWTM_D_CHANNEL));
        }
    }
}
static tree observer;
static tree inspect (gimple_stmt_iterator *p, bool *, walk_stmt_info *)
{
  gimple *stmt = gsi_stmt (*p);
  survey (stmt);
  if (schema_only) return NULL_TREE;
  tree d = newtm_descriptor_find (stmt, NEWTM_DESC_WRITE);
  if (!d) return NULL_TREE;
  tree root = TREE_VEC_ELT (d, NEWTM_D_ORIGIN);
  if (!root || !DECL_P (root) || !DECL_NAME (root)
      || strcmp (IDENTIFIER_POINTER (DECL_NAME (root)), "a")) return NULL_TREE;
  gcc_assert (tree_to_uhwi (TREE_VEC_ELT (d, NEWTM_D_STATE)) == NEWTM_DESC_REGISTERED);
  /* A copy cannot silently reuse descriptors referring to old operands. */
  gimple *copy = gimple_copy (stmt);
  tree stale = newtm_descriptor_find (copy, NEWTM_DESC_WRITE);
  gcc_assert (stale && tree_to_uhwi (TREE_VEC_ELT (stale, NEWTM_D_STATE)) == NEWTM_DESC_STALE);
  gcc_assert (!TREE_VEC_ELT (stale, NEWTM_D_VALID) && !TREE_VEC_ELT (stale, NEWTM_D_OBJECT));
  tree valid = TREE_VEC_ELT (d, NEWTM_D_VALID);
  gcall *call = gimple_build_call (observer, 5,
      TREE_VEC_ELT (d, NEWTM_D_OBJECT), TREE_VEC_ELT (d, NEWTM_D_EXTENT),
      TREE_VEC_ELT (d, NEWTM_D_OFFSET), TREE_VEC_ELT (d, NEWTM_D_KIND), valid);
  gsi_insert_before (p, call, GSI_SAME_STMT);
  tree yes = create_artificial_label (UNKNOWN_LOCATION);
  tree no = create_artificial_label (UNKNOWN_LOCATION);
  gsi_insert_before (p, gimple_build_cond (NE_EXPR, valid,
                      build_zero_cst (TREE_TYPE (valid)), yes, no), GSI_SAME_STMT);
  gsi_insert_before (p, gimple_build_label (no), GSI_SAME_STMT);
  gsi_insert_before (p, gimple_build_return (build_int_cst (integer_type_node, -100)), GSI_SAME_STMT);
  gsi_insert_before (p, gimple_build_label (yes), GSI_SAME_STMT);
  ++checked;
  return NULL_TREE;
}
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_descriptor_probe", OPTGROUP_NONE,
                         TV_NONE, 0, 0, 0, 0, 0 };
class probe : public gimple_opt_pass {
public:
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *) final override {
    if (!schema_only && strcmp (IDENTIFIER_POINTER (DECL_NAME (current_function_decl)), "probe")) return 0;
    ggc_collect (GGC_COLLECT_FORCE);
    cgraph_node *node;
    FOR_EACH_DEFINED_FUNCTION (node)
      if (DECL_NAME (node->decl)
          && !strcmp (IDENTIFIER_POINTER (DECL_NAME (node->decl)), "inspect_descriptor"))
        observer = node->decl;
    gcc_assert (schema_only || observer);
    walk_stmt_info wi = {};
    walk_gimple_seq (gimple_body (current_function_decl), inspect, NULL, &wi);
    return 0;
  }
};
}
static void finish (void *, void *) {
  if (schema_only)
    {
      for (unsigned i = 0; i < 7; ++i)
        if (i != 3 && !roles[i]) error ("descriptor schema missed role %u", i);
      for (unsigned i = 0; i < 7; ++i)
        if (i != 2 && i != 5 && !states[i]) error ("descriptor schema missed state %u", i);
      return;
    }
  if (checked != 1) error ("descriptor probe expected one registered store, got %u", checked);
}
int plugin_init (plugin_name_args *info, plugin_gcc_version *version) {
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < info->argc; ++i)
    if (!strcmp (info->argv[i].key, "schema")) schema_only = true;
  register_pass_info p = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &p);
  register_callback (info->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
