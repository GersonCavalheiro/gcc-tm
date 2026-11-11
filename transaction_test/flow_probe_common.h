/* Exercise the production flow builder with fresh synthetic IR. The real
   preparation graph is checked by its scalar history, never by late operands. */
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "gimple.h"
#include "gimple-expr.h"
#include "gimple-iterator.h"
#include "gimple-pretty-print.h"
#include "ggc.h"
#include "diagnostic-core.h"
#include "newtm-obligation.h"
#include "newtm-clone.h"
#include "newtm-flow.h"
#include <initializer_list>
int plugin_is_GPL_compatible;
static tree roots, x;
static unsigned scenarios, assertions, production;
static bool done;
static tree keep (tree t) { roots = tree_cons (NULL_TREE, t, roots); return t; }
static void mark (void *, void *) { gt_ggc_mx (roots); }
static void require_at (bool value, unsigned line)
{ ++assertions; if (!value) internal_error ("flow probe failed at line %u, scenario %u", line, scenarios); }
#define require(V) require_at ((V), __LINE__)
static gimple_seq seq (std::initializer_list<gimple *> values)
{ gimple_seq s = NULL; for (gimple *v : values) gimple_seq_add_stmt (&s, v); return s; }
static tree label () { return keep (create_artificial_label (UNKNOWN_LOCATION)); }
static gimple *lab (tree t) { return gimple_build_label (t); }
static gimple *jump (tree t) { return gimple_build_goto (t); }
static gimple *store (int n) { return gimple_build_assign (x, build_int_cst (integer_type_node, n)); }
static gimple *ret (int n) { return gimple_build_return (build_int_cst (integer_type_node, n)); }
static gimple *branch (tree yes, tree no)
{ return gimple_build_cond (NE_EXPR, x, integer_zero_node, yes, no); }
static gimple *finally (gimple_seq body, gimple_seq cleanup)
{ return gimple_build_try (body, cleanup, GIMPLE_TRY_FINALLY); }
static char *render (gimple_seq body)
{
  char *data = NULL; size_t length = 0;
  FILE *f = open_memstream (&data, &length); require (f != NULL);
  print_gimple_seq (f, body, 0, TDF_NONE); require (!fclose (f)); return data;
}
struct sample
{
  gimple_seq body;
  newtm_flow_graph *graph;
  char *before;
  unsigned properties;
  control_flow_graph *cfg;
  sample (tree fn, gimple_seq s, const char *error = NULL)
    : body (s), graph (NULL), properties (cfun->curr_properties), cfg (cfun->cfg)
  {
    require (newtm_obligation_begin (fn, body, NEWTM_OB_COPY));
    before = render (body);
    ggc_collect (GGC_COLLECT_FORCE);
    graph = new newtm_flow_graph;
    const char *reason = NULL; gimple *bad = NULL;
    bool ok = graph->build (body, &reason, &bad);
    if (error)
      require (!ok && reason && strstr (reason, error));
    else
      {
        if (!ok) internal_error ("flow probe unexpected refusal: %s", reason);
        require (!reason && graph->node_count () && graph->node (0).kind == NEWTM_FLOW_ENTRY);
        for (unsigned i = 0; i < graph->node_count (); ++i)
          for (const newtm_flow_edge &edge : graph->node (i).successors)
            {
              require (edge.target < graph->node_count ());
              bool reverse = false;
              for (unsigned parent : graph->node (edge.target).predecessors) reverse |= parent == i;
              require (reverse && graph->node (i).reachable && graph->node (edge.target).reachable);
            }
      }
    ggc_collect (GGC_COLLECT_FORCE);
  }
  ~sample ()
  {
    char *after = render (body);
    require (!strcmp (before, after) && cfun->cfg == cfg && cfun->curr_properties == properties);
    free (before); free (after);
    delete graph; /* No GIMPLE pointer retained after closing the window. */
    newtm_obligation_end (); ++scenarios;
  }
  unsigned instances (gimple *s)
  {
    unsigned count = 0;
    for (unsigned i = 0; i < graph->node_count (); ++i)
      count += graph->node (i).reachable && graph->node (i).statement == s;
    return count;
  }
  bool path (gimple *from, gimple *to, gimple *avoid = NULL, gimple *return_origin = NULL)
  {
    auto_vec<unsigned char> seen; seen.safe_grow_cleared (graph->node_count ());
    auto_vec<unsigned> work;
    for (unsigned i = 0; i < graph->node_count (); ++i)
      if (graph->node (i).reachable && graph->node (i).statement == from)
        { work.safe_push (i); seen[i] = 1; }
    for (unsigned at = 0; at < work.length (); ++at)
      {
        unsigned id = work[at]; const newtm_flow_node &n = graph->node (id);
        if (n.statement == avoid && avoid) continue;
        if (return_origin ? n.kind == NEWTM_FLOW_RETURN_EXIT && n.origin == return_origin
                          : n.statement == to) return true;
        for (const newtm_flow_edge &e : n.successors)
          if (!seen[e.target]) { seen[e.target] = 1; work.safe_push (e.target); }
      }
    return false;
  }
};
