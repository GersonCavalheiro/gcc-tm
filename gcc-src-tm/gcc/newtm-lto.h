/* Reconstruct transaction proofs across LTO. No approval is transported.
   Copyright (C) 2026 Free Software Foundation, Inc.
   Distributed under GPLv3 or later; see COPYING3. */
#ifndef GCC_NEWTM_LTO_H
#define GCC_NEWTM_LTO_H
extern void newtm_lto_prepare ();
extern void newtm_lto_rebuild_unit ();
extern bool newtm_lto_window_p ();
extern bool newtm_lto_projection_begin ();
extern void newtm_lto_projection_end ();
extern tree newtm_lto_image (tree);
extern tree newtm_lto_source_ssa (tree);
extern void newtm_lto_finish (bool = false);
extern void newtm_lto_validate_body (function *);
extern bool newtm_lto_protected_p (tree);
extern tree newtm_lto_payload (tree);
extern void newtm_lto_receive (tree, tree);
extern tree newtm_clone_export ();
extern void newtm_clone_import (tree);
extern tree newtm_memory_export ();
extern void newtm_memory_import (tree);
extern tree newtm_access_export ();
extern void newtm_access_import (tree);
/* Only declaration/scalar facts, not local proof results or instruction maps. */
inline tree newtm_lto_pack (hash_map<tree, tree> *map)
{
  tree list = NULL_TREE;
  if (map) for (auto p : *map) list = tree_cons (p.first, p.second, list);
  return list;
}
inline void newtm_lto_unpack (hash_map<tree, tree> *&map, tree list)
{
  if (!map) map = hash_map<tree, tree>::create_ggc ();
  for (tree p = list; p; p = TREE_CHAIN (p)) map->put (TREE_PURPOSE (p), TREE_VALUE (p));
}
#endif
