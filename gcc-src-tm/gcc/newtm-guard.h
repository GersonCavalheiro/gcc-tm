/* Local must-proofs on the temporary High GIMPLE flow graph.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_GUARD_H
#define GCC_NEWTM_GUARD_H
class newtm_flow_graph;
struct newtm_guard_builder;
class newtm_guard_analysis
{
public:
  explicit newtm_guard_analysis (const newtm_flow_graph &);
  ~newtm_guard_analysis ();
  /* Requirements are mathematical predicates, not emitter approvals. */
  bool require (gimple *, tree, const char **, gimple ** = NULL);
  /* Current reaching definition, dominant and not overwritten before use. */
  gimple *definition_at (gimple *, tree);
  /* Includes addresses present in the actual IR, even before flags are fixed. */
  bool address_exposed (tree) const;
  bool arithmetic (gimple *, const char **, gimple ** = NULL);
  bool overflow_dependencies (gimple *, tree, const char **, gimple ** = NULL);
private:
  newtm_guard_analysis (const newtm_guard_analysis &) = delete;
  newtm_guard_analysis &operator= (const newtm_guard_analysis &) = delete;
  newtm_guard_builder *m_builder;
};
#endif
