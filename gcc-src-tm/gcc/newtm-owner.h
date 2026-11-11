/* Owning controller protocol in High GIMPLE or a current native CFG region.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_OWNER_H
#define GCC_NEWTM_OWNER_H
class newtm_flow_graph;
struct newtm_owner_builder;
class newtm_owner_analysis
{
public:
  /* HELPER is an obligation from the delegated source region, not a proof.
     Calls must already pass the independent structural classifier. CURRENT
     enables conservative unknown service outcomes and mandatory conflict retry
     for a native region with an independently reconstructed lifetime boundary. */
  newtm_owner_analysis (const newtm_flow_graph &, tree helper, bool current = false);
  ~newtm_owner_analysis ();
  void invalid (gimple *, bool bad_true = false);
  bool verify (const char **, gimple ** = NULL);
  /* Actual carriers that may have held owning/borrowed identity in this
     inspection, including copies introduced after remapping. */
  bool protects (tree) const;
private:
  newtm_owner_analysis (const newtm_owner_analysis &) = delete;
  newtm_owner_analysis &operator= (const newtm_owner_analysis &) = delete;
  newtm_owner_builder *m_builder;
};
#endif
