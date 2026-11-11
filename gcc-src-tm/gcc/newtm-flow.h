/* Read-only control flow for live High GIMPLE and current pre-SSA CFGs.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_FLOW_H
#define GCC_NEWTM_FLOW_H

enum newtm_flow_node_kind
{
  NEWTM_FLOW_ENTRY, NEWTM_FLOW_STMT, NEWTM_FLOW_SEQUENCE_END,
  NEWTM_FLOW_TRANSFER, NEWTM_FLOW_SCOPE_EXIT, NEWTM_FLOW_CLEANUP_ENTER,
  NEWTM_FLOW_CLEANUP_CANCEL, NEWTM_FLOW_NORMAL_EXIT,
  NEWTM_FLOW_RETURN_EXIT, NEWTM_FLOW_TERMINATE_EXIT
};
enum newtm_flow_edge_kind
{
  NEWTM_FLOW_NEXT, NEWTM_FLOW_TRUE, NEWTM_FLOW_FALSE, NEWTM_FLOW_CASE,
  NEWTM_FLOW_GOTO, NEWTM_FLOW_RETURN, NEWTM_FLOW_UNWIND,
  NEWTM_FLOW_RESUME, NEWTM_FLOW_TERMINATE
};
enum newtm_flow_scope_kind
{ NEWTM_FLOW_ROOT, NEWTM_FLOW_BIND, NEWTM_FLOW_EVAL, NEWTM_FLOW_CLEANUP };

struct newtm_flow_edge
{
  unsigned target;
  newtm_flow_edge_kind kind;
  tree selector; /* CASE_LABEL_EXPR, including default; otherwise NULL. */
};
struct newtm_flow_node
{
  newtm_flow_node_kind kind;
  gimple *statement; /* Actual operation, NULL for synthetic analysis nodes. */
  gimple *origin; /* Owner of a synthetic boundary/exit; never re-executed. */
  unsigned scope, continuation;
  bool reachable;
  auto_vec<newtm_flow_edge> successors;
  auto_vec<unsigned> predecessors;
};
struct newtm_flow_scope
{
  newtm_flow_scope_kind kind;
  unsigned parent;
  gimple *owner;
};
struct newtm_flow_summary
{
  unsigned statements, reachable_statements, nodes, edges, continuations;
  unsigned cycle_edges, normal_exits, return_exits, terminate_exits;
};
struct newtm_flow_builder;

/* A graph owns no IR and never changes GIMPLE, labels, scopes, CFG or SSA.
   Destroy it before ending the live inspection window. A cleanup operation
   can have several graph nodes, one per pending continuation, without copying
   the GIMPLE operation or re-evaluating a return operand. Only reachable
   execution states are expanded; all statements are indexed/validated. */
class newtm_flow_graph
{
public:
  newtm_flow_graph ();
  ~newtm_flow_graph ();
  bool build (gimple_seq, const char **, gimple ** = NULL);
  /* Read-only adapter for the actual pre-SSA CFG; no lexical scope inference.
     The final flag includes exception transfers in surrounding ordinary
     controller code. Prepared bodies keep the strict default domain. */
  bool build_cfg (function *, const char **, gimple ** = NULL, bool = false);
  /* Current protocol subgraph, ending immediately before EXIT.  Every actual
     outgoing path is retained; entries into its interior are refused. */
  bool build_region (const newtm_flow_graph &, unsigned, unsigned,
                     const char **, gimple ** = NULL);
  unsigned node_count () const;
  const newtm_flow_node &node (unsigned) const;
  unsigned scope_count () const;
  const newtm_flow_scope &scope (unsigned) const;
  bool contains (gimple *) const;
  bool reachable (gimple *) const;
  newtm_flow_summary summary () const;
private:
  newtm_flow_graph (const newtm_flow_graph &) = delete;
  newtm_flow_graph &operator= (const newtm_flow_graph &) = delete;
  newtm_flow_builder *m_builder;
};
#endif
