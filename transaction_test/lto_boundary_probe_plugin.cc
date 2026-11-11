/* Inspect the real frontend and LTO processes; never query descriptors. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "options.h"
#include "newtm-obligation.h"
#include <stdio.h>
int plugin_is_GPL_compatible;
static void report (const char *stage)
{
  fprintf (stderr, "NEWTM_LTO_%s %d %llu %llu\n", stage, int (in_lto_p),
           (unsigned long long) newtm_obligation_graph_count (),
           (unsigned long long) newtm_obligation_local_count ());
}
static void start (void *, void *) { report ("START"); }
static void ipa (void *, void *) { report ("IPA"); }
static void finish (void *, void *) { report ("FINISH"); }
int plugin_init (plugin_name_args *a, plugin_gcc_version *v)
{
  if (!plugin_default_version_check (v, &gcc_version)) return 1;
  register_callback (a->base_name, PLUGIN_START_UNIT, start, NULL);
  register_callback (a->base_name, PLUGIN_ALL_IPA_PASSES_START, ipa, NULL);
  register_callback (a->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
