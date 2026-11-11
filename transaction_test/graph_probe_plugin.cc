/* Invalidate one locally approved recursive member before whole-unit checks. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "diagnostic-core.h"
#include "newtm-clone.h"
#include "newtm-obligation.h"
int plugin_is_GPL_compatible;
static bool changed;
static tree invalid_owner;
static bool post;
static bool mutate;
static bool signature;
static bool signature_args;
static bool mutual;
static bool real_nested;
namespace {
const pass_data data = { GIMPLE_PASS, "newtm_graph_probe", OPTGROUP_NONE, TV_NONE, 0, 0, 0, 0, 0 };
struct probe : gimple_opt_pass
{
  probe (gcc::context *c) : gimple_opt_pass (data, c) {}
  unsigned execute (function *fn) override
  {
    if (post || mutate || signature || signature_args || real_nested) return 0;
    tree original = newtm_clone_original (fn->decl);
    if (!changed && original && DECL_NAME (original)
        && !strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), "recurse"))
      for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_count (); ++i)
        {
          newtm_obligation_view v;
          if (newtm_obligation_get (i, &v) && v.owner == fn->decl
              && v.local_state == NEWTM_LOCAL_APPROVED)
            { newtm_obligation_invalidate (i); changed = true; invalid_owner = fn->decl; break; }
        }
    return 0;
  }
};
}
static void finish (void *, void *)
{
  if (signature || signature_args)
    {
      for (unsigned HOST_WIDE_INT g = 1; g <= newtm_obligation_graph_count (); ++g)
        {
          newtm_graph_view graph;
          if (!newtm_obligation_graph_get (g, &graph) || !graph.valid) continue;
          if (graph.current_approved || !(graph.remap_boundaries & NEWTM_REMAP_OMP))
            { error ("historical graph incorrectly approved remapped IR"); return; }
          tree revisions = NULL_TREE;
          for (unsigned n = 0; n < graph.members; ++n)
            { unsigned HOST_WIDE_INT id;
              if (newtm_obligation_graph_member (g, n, &id) && id != graph.controller)
                revisions = tree_cons (NULL_TREE, build_int_cst (long_long_unsigned_type_node, id), revisions); }
          for (unsigned n = 0; n < graph.members; ++n)
            {
              unsigned HOST_WIDE_INT id; newtm_local_view local;
              if (!newtm_obligation_graph_member (g, n, &id) || id == graph.controller
                  || !newtm_obligation_local_get (id, &local)) continue;
              tree old = TREE_TYPE (local.owner);
              tree arguments = TYPE_ARG_TYPES (old);
              if (signature_args && !arguments) continue;
              tree old_argument = signature_args ? TREE_VALUE (arguments) : NULL_TREE;
              if (signature_args) TREE_VALUE (arguments) = integer_type_node;
              else TREE_TYPE (local.owner) = build_function_type_list (integer_type_node, integer_type_node, NULL_TREE);
              bool detected = newtm_obligation_graph_get (g, &graph) && !graph.valid;
              if (signature_args) TREE_VALUE (arguments) = old_argument;
              else TREE_TYPE (local.owner) = old;
              detected &= newtm_obligation_graph_get (g, &graph) && !graph.valid;
              detected &= !newtm_obligation_graph_revalidate (g, graph.controller, revisions);
              if (!detected) error ("changed signature retained a historical graph approval");
              else fprintf (stderr, "GRAPH_SIGNATURE invalidated\n");
              return;
            }
        }
      if (!seen_error ()) error ("no graph member available for signature probe");
      return;
    }
  if (mutate)
    {
      for (unsigned HOST_WIDE_INT g = 1; g <= newtm_obligation_graph_count (); ++g)
        {
          newtm_graph_view graph;
          if (!newtm_obligation_graph_get (g, &graph) || !graph.valid) continue;
          if (graph.current_approved || !(graph.remap_boundaries & NEWTM_REMAP_OMP))
            { error ("historical graph incorrectly approved remapped IR"); return; }
          tree revisions = NULL_TREE;
          for (unsigned n = 0; n < graph.members; ++n)
            { unsigned HOST_WIDE_INT id;
              if (newtm_obligation_graph_member (g, n, &id) && id != graph.controller)
                revisions = tree_cons (NULL_TREE, build_int_cst (long_long_unsigned_type_node, id), revisions); }
          for (unsigned n = 0; n < graph.members; ++n)
            {
              unsigned HOST_WIDE_INT id; newtm_local_view local;
              if (!newtm_obligation_graph_member (g, n, &id) || !newtm_obligation_local_get (id, &local)
                  || !local.dependencies) continue;
              tree old = TREE_VALUE (local.dependencies);
              TREE_VALUE (local.dependencies) = build_fn_decl ("modified_edge", TREE_TYPE (local.owner));
              bool detected = newtm_obligation_graph_get (g, &graph) && !graph.valid;
              TREE_VALUE (local.dependencies) = old;
              detected &= newtm_obligation_graph_get (g, &graph) && !graph.valid;
              detected &= !newtm_obligation_graph_commit (graph.root, graph.controller, revisions);
              detected &= !newtm_obligation_graph_revalidate (g, graph.controller, revisions);
              if (!detected) error ("changed edge retained a historical graph approval");
              else fprintf (stderr, "GRAPH_MUTATION invalidated\n");
              return;
            }
        }
      if (!seen_error ()) error ("no graph edge available for mutation probe");
      return;
    }
  if (real_nested)
    {
      bool found = false;
      for (unsigned HOST_WIDE_INT id = 1; id <= newtm_obligation_local_count (); ++id)
        { newtm_local_view v;
          if (newtm_obligation_local_get (id, &v) && (v.remap_boundaries & NEWTM_REMAP_NESTED))
            found = true; }
      if (!found) error ("tree-nested boundary did not mark any revision");
      else fprintf (stderr, "GRAPH_NESTED observed\n");
      return;
    }
  if (post)
    {
      for (unsigned HOST_WIDE_INT g = 1; g <= newtm_obligation_graph_count (); ++g)
        {
          newtm_graph_view graph;
          if (!newtm_obligation_graph_get (g, &graph) || !graph.valid) continue;
          if (graph.current_approved || !(graph.remap_boundaries & NEWTM_REMAP_OMP))
            { error ("historical graph incorrectly approved remapped IR"); return; }
          tree revisions = NULL_TREE;
          unsigned HOST_WIDE_INT first = 0;
          unsigned HOST_WIDE_INT recursive_revision = 0;
          for (unsigned n = 0; n < graph.members; ++n)
            {
              unsigned HOST_WIDE_INT revision;
              if (!newtm_obligation_graph_member (g, n, &revision))
                { error ("historical graph lost a revision"); return; }
              if (revision == graph.controller) continue;
              if (!first) first = revision;
              newtm_local_view member;
              if (newtm_obligation_local_get (revision, &member))
                if (tree original = newtm_clone_original (member.owner))
                  if (DECL_NAME (original) && !strcmp (IDENTIFIER_POINTER (DECL_NAME (original)), "recurse"))
                    recursive_revision = revision;
              revisions = tree_cons (NULL_TREE, build_int_cst (long_long_unsigned_type_node, revision), revisions);
            }
          if (!recursive_revision) continue;
          /* omplower already ran while these functions were analyzed.  The
             graph remains historical; none of its revisions silently loses
             the boundary marker, even through recursive dependencies. */
          newtm_local_view root_revision;
          if (!newtm_obligation_local_get (recursive_revision, &root_revision)
              || !(root_revision.remap_boundaries & NEWTM_REMAP_OMP))
            { error ("omplower boundary did not invalidate the current revision"); return; }
          newtm_obligation_remap_boundary (root_revision.owner, NEWTM_REMAP_NESTED);
          if (!newtm_obligation_local_get (graph.controller, &root_revision)
              || !(root_revision.remap_boundaries & NEWTM_REMAP_NESTED))
            { error ("remap boundary did not propagate to controller"); return; }
          unsigned recursive_component, component_size = 0;
          if (!newtm_obligation_graph_component (g, recursive_revision, &recursive_component))
            { error ("recursive component is missing"); return; }
          for (tree p = revisions; p; p = TREE_CHAIN (p))
            { unsigned component;
              if (!newtm_obligation_graph_component (g, tree_to_uhwi (TREE_VALUE (p)), &component))
                { error ("graph component is missing"); return; }
              component_size += component == recursive_component; }
          if (component_size != (mutual ? 2u : 1u))
            { error ("recursive component has incorrect membership"); return; }
          unsigned HOST_WIDE_INT before = newtm_obligation_graph_count ();
          for (tree p = revisions; p; p = TREE_CHAIN (p))
            {
              newtm_local_view v;
              if (!newtm_obligation_local_get (tree_to_uhwi (TREE_VALUE (p)), &v))
                { error ("historical graph revision cannot be read"); return; }
              if (!newtm_clone_matches_preflight (v.owner, v.dependencies)
                  || (v.dependencies && newtm_clone_matches_preflight (v.owner, NULL_TREE)))
                { error ("preflight accepted a missing dependency"); return; }
              tree alien = build_fn_decl ("not_a_graph_member", TREE_TYPE (v.owner));
              if (newtm_clone_matches_preflight (v.owner, tree_cons (NULL_TREE, alien, v.dependencies)))
                { error ("preflight accepted an extra dependency"); return; }
            }
          tree incomplete = NULL_TREE, extra = NULL_TREE;
          bool omitted = false;
          for (tree p = revisions; p; p = TREE_CHAIN (p))
            {
              newtm_local_view v;
              if (!newtm_obligation_local_get (tree_to_uhwi (TREE_VALUE (p)), &v))
                { error ("historical graph revision is missing"); return; }
              if (!omitted && v.owner != graph.root) { omitted = true; continue; }
              incomplete = tree_cons (NULL_TREE, TREE_VALUE (p), incomplete);
            }
          for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_local_count (); ++i)
            {
              newtm_local_view v;
              if (!newtm_obligation_local_get (i, &v) || v.state != NEWTM_LOCAL_APPROVED
                  || v.graph_invalid || !v.closed
                  || (v.scope_kind != NEWTM_OB_CLONE && v.scope_kind != NEWTM_OB_REGION)) continue;
              bool present = false;
              for (tree p = revisions; p; p = TREE_CHAIN (p))
                { newtm_local_view old; newtm_obligation_local_get (tree_to_uhwi (TREE_VALUE (p)), &old);
                  present |= old.owner == v.owner; }
              if (!present) { extra = tree_cons (NULL_TREE, build_int_cst (long_long_unsigned_type_node, i), revisions); break; }
            }
          if (!first || newtm_obligation_graph_commit (graph.root, graph.controller, NULL_TREE)
              || newtm_obligation_graph_commit (graph.root, graph.controller,
                   tree_cons (NULL_TREE, build_int_cst (long_long_unsigned_type_node, first), revisions))
              || (omitted && newtm_obligation_graph_commit (graph.root, graph.controller, incomplete))
              || (extra && newtm_obligation_graph_commit (graph.root, graph.controller, extra))
              || newtm_obligation_graph_count () != before
              || !newtm_obligation_graph_commit (graph.root, graph.controller, revisions))
            { error ("historical graph closure or atomic publication failed"); return; }
          unsigned HOST_WIDE_INT duplicate = newtm_obligation_graph_count ();
          newtm_local_view changed_member;
          if (!newtm_obligation_local_get (recursive_revision, &changed_member))
            { error ("recursive member is missing"); return; }
          newtm_obligation_graph_invalidate_callers (changed_member.owner);
          unsigned HOST_WIDE_INT invalid_count = newtm_obligation_graph_count ();
          if (newtm_obligation_graph_revalidate (g, graph.controller, NULL_TREE)
              || (omitted && newtm_obligation_graph_revalidate (g, graph.controller, incomplete))
              || newtm_obligation_graph_revalidate (g, graph.controller + 1, revisions)
              || newtm_obligation_graph_count () != invalid_count)
            { error ("failed revalidation published a graph"); return; }
          if (!newtm_obligation_graph_get (g, &graph) || graph.valid
              || newtm_obligation_graph_commit (graph.root, graph.controller, revisions)
              || !newtm_obligation_graph_revalidate (g, graph.controller, revisions))
            { error ("historical graph could not be revalidated"); return; }
          unsigned HOST_WIDE_INT rebuilt = newtm_obligation_graph_count ();
          if (!newtm_obligation_graph_get (rebuilt, &graph) || !graph.valid
              || !newtm_obligation_graph_get (g, &graph) || graph.valid
              || !newtm_obligation_graph_get (duplicate, &graph) || graph.valid)
            { error ("revalidation resurrected an old graph"); return; }
          for (unsigned n = 0; n < graph.members; ++n)
            {
              unsigned HOST_WIDE_INT revision;
              if (!newtm_obligation_graph_member (g, n, &revision) || revision == graph.controller) continue;
              for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_count (); ++i)
                {
                  newtm_obligation_view v;
                  if (newtm_obligation_get (i, &v) && v.local_result == revision
                      && v.local_state == NEWTM_LOCAL_APPROVED)
                    {
                      newtm_obligation_invalidate (i);
                      if (!newtm_obligation_graph_get (g, &graph) || graph.valid)
                        error ("historical graph was not invalidated");
                      else if (!newtm_obligation_graph_get (duplicate, &graph) || graph.valid
                               || !newtm_obligation_graph_get (rebuilt, &graph) || graph.valid
                               || newtm_obligation_graph_commit (graph.root, graph.controller, revisions)
                               || newtm_obligation_graph_revalidate (g, graph.controller, revisions))
                        error ("dependent historical revision survived invalidation");
                      else fprintf (stderr, "GRAPH_POST invalidated\n");
                      return;
                    }
                }
            }
        }
      if (!seen_error ()) error ("no historical graph available for invalidation probe");
      return;
    }
  if (!changed && !seen_error ()) error ("recursive graph probe did not invalidate a member");
  if (changed)
    {
      bool propagated = false, local_preserved = false;
      for (unsigned HOST_WIDE_INT i = 1; i <= newtm_obligation_local_count (); ++i)
        {
          newtm_local_view v;
          if (newtm_obligation_local_get (i, &v) && v.owner != invalid_owner && v.graph_invalid)
            { propagated = true;
              if (v.state == NEWTM_LOCAL_APPROVED && v.required == v.proved) local_preserved = true; }
        }
      if (!propagated) error ("recursive graph invalidation did not reach a caller");
      if (!local_preserved) error ("graph invalidation did not preserve a caller local proof");
    }
}
int plugin_init (plugin_name_args *args, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version)) return 1;
  for (int i = 0; i < args->argc; ++i)
    { if (!strcmp (args->argv[i].key, "post")) post = true;
      if (!strcmp (args->argv[i].key, "mutual")) mutual = true;
      if (!strcmp (args->argv[i].key, "real_nested")) real_nested = true;
      if (!strcmp (args->argv[i].key, "mutate")) mutate = true; }
  for (int i = 0; i < args->argc; ++i) if (!strcmp (args->argv[i].key, "signature")) signature = true;
  for (int i = 0; i < args->argc; ++i) if (!strcmp (args->argv[i].key, "signature_args")) signature_args = true;
  register_pass_info info = { new probe (g), "omplower", 1, PASS_POS_INSERT_BEFORE };
  register_callback (args->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &info);
  register_callback (args->base_name, PLUGIN_FINISH_UNIT, finish, NULL);
  return 0;
}
