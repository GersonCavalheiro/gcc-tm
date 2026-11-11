/* Operation-specific status and output availability on live High GIMPLE.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_STATUS_H
#define GCC_NEWTM_STATUS_H
class newtm_flow_graph;
struct newtm_status_builder;
class newtm_status_analysis
{
public:
  explicit newtm_status_analysis (const newtm_flow_graph &);
  ~newtm_status_analysis ();
  /* Actual calls and resolved output objects, never approval from the emitter.
     The two independent outputs are the value and its optional binding. */
  bool operation (gimple *, tree = NULL_TREE, tree = NULL_TREE);
  /* A structurally proven PRIVATE arm producing the same complete buffer. */
  void alternative (gimple *, gimple *);
  void continuation (gimple *);
  void reset (gimple *);
  /* Additional obligation: this exact call succeeded on every path to use. */
  void require_success (gimple *, gimple *);
  /* Native compiler buffers have no ordinary pre-call DIRECT producer. */
  void strict_outputs ();
  bool verify (const char **, gimple ** = NULL);
private:
  newtm_status_analysis (const newtm_status_analysis &) = delete;
  newtm_status_analysis &operator= (const newtm_status_analysis &) = delete;
  newtm_status_builder *m_builder;
};
#endif
