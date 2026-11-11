/* Declare an unprepared dependency after a valid aggregate baseline. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "cgraph.h"
#include "diagnostic-core.h"
#include "newtm-access.h"
#include "newtm-clone.h"
#include "newtm-current.h"
#include "newtm-lto.h"
int plugin_is_GPL_compatible;
static bool observe, visited;
static void ipa (void *, void *)
{
  if (seen_error ()) return;
  newtm_current_view baseline;
  if (!newtm_current_count () || !newtm_current_get (1, &baseline)
      || !baseline.current_approved || baseline.proved != NEWTM_CURRENT_ALL
      || baseline.dependencies)
    { error ("member probe lacks an approved baseline"); return; }
  visited = true;
  fputs ("UNPREPARED_MEMBER baseline approved\n", stderr);
  if (observe) return;
  tree missing = NULL_TREE; cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    if (DECL_NAME (node->decl)
        && !strcmp (IDENTIFIER_POINTER (DECL_NAME (node->decl)), "unprepared_dependency"))
      missing = node->decl;
  if (!missing || newtm_access_prepared_p (missing))
    { error ("member probe target is absent or already prepared"); return; }
  tree contracts = newtm_clone_export ();
  bool changed = false;
  /* Section 6 is the existing declarative call-dependency map.  Keep real
     calls/CFG unchanged, append one required but unprepared member. */
  for (tree p = TREE_VEC_ELT (contracts, 6); p; p = TREE_CHAIN (p))
    if (TREE_PURPOSE (p) == baseline.root)
      { TREE_VALUE (p) = tree_cons (NULL_TREE, missing, TREE_VALUE (p)); changed = true; break; }
  if (!changed) { error ("member probe found no root dependency contract"); return; }
  newtm_clone_import (contracts);
  fputs ("UNPREPARED_MEMBER declared dependency appended\n", stderr);
  if (newtm_current_verify (baseline.root)) error ("unprepared dependency was approved");
  newtm_current_view result;
  if (!newtm_current_get (newtm_current_count (), &result)
      || result.state != NEWTM_CURRENT_REFUSED || result.current_approved || result.proved)
    error ("member probe exposed partial approval");
  else fputs ("UNPREPARED_MEMBER refused proved=0\n", stderr);
}
static void finish (void *, void *)
{ if (!visited) error ("member probe missed the aggregate window"); }
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i)
    if (!strcmp (args->argv[i].key, "observe")) observe = true;
  register_callback (args->base_name, PLUGIN_ALL_IPA_PASSES_START, ipa, NULL);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
