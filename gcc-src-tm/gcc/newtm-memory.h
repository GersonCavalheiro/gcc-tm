/* Memory identities in current declaration CFGs.  */
#ifndef GCC_NEWTM_MEMORY_H
#define GCC_NEWTM_MEMORY_H

enum newtm_memory_role
{
  NEWTM_MEMORY_REGISTER, NEWTM_MEMORY_READ, NEWTM_MEMORY_WRITE,
  NEWTM_MEMORY_PRIVATE_READ, NEWTM_MEMORY_PRIVATE_WRITE,
  NEWTM_MEMORY_ADDRESS
};
enum newtm_memory_dependency
{
  NEWTM_MEMORY_CHANNELS = 1, NEWTM_MEMORY_STATUS = 2,
  NEWTM_MEMORY_OWNER = 4, NEWTM_MEMORY_IDENTITY = 8
};
enum newtm_memory_state
{ NEWTM_MEMORY_ABSENT, NEWTM_MEMORY_DEPENDENT, NEWTM_MEMORY_REFUSED };
struct newtm_memory_view
{
  newtm_memory_state state;
  unsigned operations, dependencies;
};
/* Only called in the live preparation window.  These operands are consumed
   by snapshot; no instruction, descriptor or access recipe survives it. */
extern void newtm_memory_note (gimple *, newtm_memory_role, tree, tree, tree,
                               tree, tree, tree, tree = NULL_TREE);
extern bool newtm_memory_snapshot (tree, gimple_seq);
extern void newtm_memory_retire ();
/* Read-only proof of a current declaration CFG, not a final acceptance fence.
   All channel/status/owner dependencies must be discharged by blocks 3--5. */
extern bool newtm_memory_verify_cfg (tree);
extern bool newtm_memory_get (tree, newtm_memory_view *);
/* Current declaration-CFG transport proof.  Conditional on call-status and owner
   proofs; never grants current_approved by itself. */
extern bool newtm_channels_verify_cfg (tree);
extern bool newtm_channels_verify_entry (tree, gimple *);
extern bool newtm_channels_snapshot_entry (tree, gimple_seq, gimple *);
extern bool newtm_channels_get (tree, newtm_memory_view *);
extern bool newtm_status_verify_cfg (tree);
extern bool newtm_status_verify_entry (tree, gimple *);
extern bool newtm_status_get (tree, newtm_memory_view *);
/* Per-controller current protocol component.  Does not certify graph coverage
   or publish current_approved; channels/status remain separate components. */
extern bool newtm_owner_snapshot_entry (tree, gimple_seq, gimple *, tree);
extern bool newtm_owner_verify_entry (tree, gimple *);
extern bool newtm_owner_get (tree, newtm_memory_view *);
/* Closed classification of current effects, conditional on the
   independent identity/channel/status/owner components.  No graph approval. */
extern bool newtm_coverage_verify_cfg (tree);
extern bool newtm_coverage_verify_entry (tree, gimple *);
extern bool newtm_coverage_get (tree, newtm_memory_view *);
extern bool newtm_coverage_entry_get (tree, newtm_memory_view *);
#endif
