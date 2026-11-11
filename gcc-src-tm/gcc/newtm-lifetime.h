/* Definite transport initialization, lexical lifetime and publication.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_LIFETIME_H
#define GCC_NEWTM_LIFETIME_H
class newtm_flow_graph;
struct newtm_lifetime_builder;
class newtm_lifetime_analysis
{
public:
  explicit newtm_lifetime_analysis (const newtm_flow_graph &, tree, bool = false);
  ~newtm_lifetime_analysis ();
  void watch (tree);
  void borrowed (tree);
  /* A transport borrowed for reads must remain read-only even through aliases. */
  void readonly (tree);
  /* A controller's surrounding source code follows ordinary C/C++ escape
     rules. Only compiler transports are exclusive there. */
  void transport_only ();
  void borrow (gimple *, unsigned);
  void effects (gimple *, unsigned, unsigned);
  void input (gimple *, tree, HOST_WIDE_INT = -1);
  /* Output definitions compose with the operation-specific success proof;
     this API alone never authorizes consuming a failed call's buffer. */
  void output (gimple *, tree, bool = false, bool = false, unsigned = 0);
  void publication (tree, tree = NULL_TREE);
  bool verify (const char **, gimple ** = NULL);
private:
  newtm_lifetime_analysis (const newtm_lifetime_analysis &) = delete;
  newtm_lifetime_analysis &operator= (const newtm_lifetime_analysis &) = delete;
  newtm_lifetime_builder *m_builder;
};
#endif
