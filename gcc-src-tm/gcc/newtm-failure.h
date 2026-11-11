/* Borrowed-frame failure propagation and relative nesting, before lowering.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_FAILURE_H
#define GCC_NEWTM_FAILURE_H
class newtm_flow_graph;
struct newtm_failure_builder;
class newtm_failure_analysis
{
public:
  /* Calls must already satisfy the independent runtime/prepared-call
     classifier. Callee success borrows and preserves the caller's depth. */
  newtm_failure_analysis (const newtm_flow_graph &, tree);
  ~newtm_failure_analysis ();
  /* Obligation, not permission: the actual false edge must propagate INVALID. */
  void invalid (gimple *, bool = false);
  bool verify (const char **, gimple ** = NULL);
private:
  newtm_failure_analysis (const newtm_failure_analysis &) = delete;
  newtm_failure_analysis &operator= (const newtm_failure_analysis &) = delete;
  newtm_failure_builder *m_builder;
};
#endif
