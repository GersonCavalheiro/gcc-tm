/* Finite continuation-sensitive flow without lowering the inspected GIMPLE.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of GCC, distributed under GPLv3 or later; see COPYING3. */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "tree-cfg.h"
#include "newtm-flow.h"
#include "internal-fn.h"

namespace {
const unsigned absent = UINT_MAX;
typedef int_hash<unsigned, UINT_MAX, UINT_MAX - 1> index_hash;
typedef pair_hash<index_hash, index_hash> pair_index_hash;
enum position_kind { STATEMENT, SEQUENCE_END, TRANSFER, ENTRY,
                     NORMAL_EXIT, RETURN_EXIT, TERMINATE_EXIT };
struct position
{
  position_kind kind;
  unsigned scope, next, child, cleanup, target;
  gimple *stmt;
};
struct lexical_scope
{
  newtm_flow_scope view;
  unsigned owner_position, first, cleanup;
};
struct continuation
{
  unsigned cleanup_scope, resume, parent;
};
struct dfs_frame { unsigned node, edge; };
}

struct newtm_flow_builder
{
  auto_vec<position> positions;
  auto_vec<lexical_scope> scopes;
  auto_vec<continuation> continuations;
  auto_vec<newtm_flow_node *> nodes;
  hash_map<gimple *, unsigned> statements;
  hash_map<tree, unsigned> labels;
  hash_map<pair_index_hash, unsigned> states, transfers, pending;
  hash_set<gimple *> reached;
  auto_vec<unsigned> node_positions;
  auto_vec<unsigned> worklist;
  const char *reason = NULL;
  gimple *bad = NULL;
  bool attempted = false;
  unsigned normal_exit = absent;
  newtm_flow_summary totals = {};

  ~newtm_flow_builder ()
  { for (newtm_flow_node *n : nodes) delete n; }
  bool refuse (gimple *s, const char *why)
  { if (!reason) { reason = why; bad = s; } return false; }
  unsigned add_position (position_kind kind, unsigned scope, gimple *s = NULL)
  {
    position p = { kind, scope, absent, absent, absent, absent, s };
    unsigned id = positions.length ();
    positions.safe_push (p); return id;
  }
  bool ancestor (unsigned a, unsigned b) const
  {
    for (;; b = scopes[b].view.parent)
      { if (a == b) return true; if (!b) return false; }
  }
  unsigned after_scope (unsigned s) const
  { return positions[scopes[s].owner_position].next; }

  unsigned index_sequence (gimple_seq seq, unsigned parent,
                           newtm_flow_scope_kind kind, unsigned owner)
  {
    unsigned id = scopes.length ();
    lexical_scope scope = { { kind, parent, owner == absent ? NULL : positions[owner].stmt },
                            owner, absent, absent };
    scopes.safe_push (scope);
    unsigned previous = absent;
    for (gimple_stmt_iterator it = gsi_start (seq); !gsi_end_p (it) && !reason; gsi_next (&it))
      {
        gimple *s = gsi_stmt (it);
        if (statements.get (s))
          { refuse (s, "V08: repeated instruction instance in flow input"); break; }
        unsigned pc = add_position (STATEMENT, id, s);
        statements.put (s, pc);
        if (previous != absent) positions[previous].next = pc;
        else scopes[id].first = pc;
        previous = pc;
        switch (gimple_code (s))
          {
          case GIMPLE_BIND:
            {
              unsigned child = index_sequence (gimple_bind_body (as_a<gbind *> (s)), id,
                                                NEWTM_FLOW_BIND, pc);
              positions[pc].child = child; break;
            }
          case GIMPLE_TRY:
            {
              if (gimple_try_kind (as_a<gtry *> (s)) != GIMPLE_TRY_FINALLY)
                { refuse (s, "V02: unsupported exception handler in flow input"); break; }
              unsigned eval = index_sequence (gimple_try_eval (s), id, NEWTM_FLOW_EVAL, pc);
              unsigned cleanup = index_sequence (gimple_try_cleanup (s), id, NEWTM_FLOW_CLEANUP, pc);
              positions[pc].child = eval; positions[pc].cleanup = cleanup;
              scopes[eval].cleanup = cleanup; break;
            }
          case GIMPLE_LABEL:
            {
              tree label = gimple_label_label (as_a<glabel *> (s));
              if (!label || TREE_CODE (label) != LABEL_DECL || labels.get (label))
                refuse (s, "V08: missing or duplicate flow label");
              else labels.put (label, pc);
              break;
            }
          case GIMPLE_RETURN:
            { unsigned target = add_position (RETURN_EXIT, 0, s); positions[pc].target = target; break; }
          case GIMPLE_CALL:
            if ((gimple_call_flags (s) & ECF_RETURNS_TWICE) || gimple_call_ctrl_altering_p (s))
              refuse (s, "V02: nonlocal or multiple-return control in flow call");
            else if (gimple_call_noreturn_p (s))
              { unsigned target = add_position (TERMINATE_EXIT, 0, s); positions[pc].target = target; }
            break;
          case GIMPLE_ASSIGN: case GIMPLE_COND: case GIMPLE_GOTO: case GIMPLE_SWITCH:
          case GIMPLE_NOP: case GIMPLE_DEBUG: case GIMPLE_PREDICT: break;
          default: refuse (s, "V02: unsupported High GIMPLE operation in flow input"); break;
          }
      }
    unsigned end = add_position (SEQUENCE_END, id);
    if (previous != absent) positions[previous].next = end;
    else scopes[id].first = end;
    return id;
  }

  unsigned label_target (gimple *s, unsigned source_scope, tree label)
  {
    unsigned *target = label && TREE_CODE (label) == LABEL_DECL ? labels.get (label) : NULL;
    if (!target)
      { refuse (s, "V08: missing, external or computed flow target"); return absent; }
    /* A branch may enter an ordinary BIND (skipping its initializers), but
       must not bypass structured entry to an eval or cleanup region. */
    unsigned target_scope = positions[*target].scope;
    for (unsigned t = target_scope; !ancestor (t, source_scope); t = scopes[t].view.parent)
      if (scopes[t].view.kind == NEWTM_FLOW_EVAL || scopes[t].view.kind == NEWTM_FLOW_CLEANUP)
        { refuse (s, "V07: branch bypasses structured try/finally entry"); return absent; }
    return *target;
  }
  bool validate_targets ()
  {
    for (const position &p : positions)
      {
        if (p.kind != STATEMENT) continue;
        gimple *s = p.stmt;
        switch (gimple_code (s))
          {
          case GIMPLE_GOTO: label_target (s, p.scope, gimple_goto_dest (s)); break;
          case GIMPLE_COND:
            label_target (s, p.scope, gimple_cond_true_label (as_a<gcond *> (s)));
            label_target (s, p.scope, gimple_cond_false_label (as_a<gcond *> (s))); break;
          case GIMPLE_SWITCH:
            {
              gswitch *sw = as_a<gswitch *> (s);
              if (!gimple_switch_num_labels (sw))
                { refuse (s, "V08: switch lacks a default flow target"); break; }
              for (unsigned i = 0; i < gimple_switch_num_labels (sw); ++i)
                {
                  tree c = gimple_switch_label (sw, i);
                  if (!c || TREE_CODE (c) != CASE_LABEL_EXPR
                      || (i == 0 ? CASE_LOW (c) != NULL_TREE : CASE_LOW (c) == NULL_TREE))
                    { refuse (s, "V08: malformed flow switch labels"); break; }
                  label_target (s, p.scope, CASE_LABEL (c));
                }
              break;
            }
          default: break;
          }
        if (reason) return false;
      }
    return true;
  }

  newtm_flow_node_kind node_kind (const position &p) const
  {
    switch (p.kind)
      {
      case STATEMENT: return NEWTM_FLOW_STMT;
      case ENTRY: return NEWTM_FLOW_ENTRY;
      case SEQUENCE_END: return NEWTM_FLOW_SEQUENCE_END;
      case NORMAL_EXIT: return NEWTM_FLOW_NORMAL_EXIT;
      case RETURN_EXIT: return NEWTM_FLOW_RETURN_EXIT;
      case TERMINATE_EXIT: return NEWTM_FLOW_TERMINATE_EXIT;
      case TRANSFER:
        if (ancestor (p.scope, positions[p.target].scope)) return NEWTM_FLOW_TRANSFER;
        switch (scopes[p.scope].view.kind)
          {
          case NEWTM_FLOW_BIND: return NEWTM_FLOW_SCOPE_EXIT;
          case NEWTM_FLOW_EVAL: return NEWTM_FLOW_CLEANUP_ENTER;
          case NEWTM_FLOW_CLEANUP: return NEWTM_FLOW_CLEANUP_CANCEL;
          default: return NEWTM_FLOW_TRANSFER;
          }
      }
    gcc_unreachable ();
  }
  unsigned state (unsigned pc, unsigned context)
  {
    std::pair<unsigned, unsigned> key (pc, context);
    if (unsigned *old = states.get (key)) return *old;
    unsigned id = nodes.length ();
    const position &p = positions[pc];
    newtm_flow_node *n = new newtm_flow_node;
    n->kind = node_kind (p);
    n->statement = p.kind == STATEMENT ? p.stmt : NULL;
    n->origin = p.stmt ? p.stmt : scopes[p.scope].view.owner;
    n->scope = p.scope; n->continuation = context;
    n->reachable = false;
    nodes.safe_push (n); node_positions.safe_push (pc); states.put (key, id);
    return id;
  }
  unsigned route (unsigned scope, unsigned target, unsigned context)
  {
    std::pair<unsigned, unsigned> key (scope, target);
    unsigned *old = transfers.get (key), pc;
    if (old) pc = *old;
    else
      { pc = add_position (TRANSFER, scope); positions[pc].target = target; transfers.put (key, pc); }
    return state (pc, context);
  }
  unsigned continuation_for (unsigned cleanup, unsigned resume, unsigned parent)
  {
    std::pair<unsigned, unsigned> key (cleanup, resume);
    if (unsigned *old = pending.get (key)) return *old;
    unsigned id = continuations.length ();
    continuation c = { cleanup, resume, parent };
    continuations.safe_push (c); pending.put (key, id); return id;
  }
  void edge (unsigned from, unsigned to, newtm_flow_edge_kind kind, tree selector = NULL_TREE)
  {
    newtm_flow_edge e = { to, kind, selector };
    nodes[from]->successors.safe_push (e); nodes[to]->predecessors.safe_push (from);
    if (nodes[from]->reachable && !nodes[to]->reachable)
      { nodes[to]->reachable = true; worklist.safe_push (to); }
    ++totals.edges;
  }
  void expand (unsigned id)
  {
    /* Creating states/routes can reallocate positions. Never retain a
       reference into that vector across an insertion. */
    position p = positions[node_positions[id]];
    unsigned context = nodes[id]->continuation;
    if (p.kind == ENTRY)
      { edge (id, state (scopes[0].first, 0), NEWTM_FLOW_NEXT); return; }
    if (p.kind == NORMAL_EXIT) { ++totals.normal_exits; return; }
    if (p.kind == RETURN_EXIT) { ++totals.return_exits; return; }
    if (p.kind == TERMINATE_EXIT) { ++totals.terminate_exits; return; }
    if (p.kind == TRANSFER)
      {
        if (ancestor (p.scope, positions[p.target].scope))
          { edge (id, state (p.target, context), NEWTM_FLOW_NEXT); return; }
        lexical_scope s = scopes[p.scope];
        if (s.view.kind == NEWTM_FLOW_EVAL)
          {
            unsigned resume = route (s.view.parent, p.target, context);
            unsigned c = continuation_for (s.cleanup, resume, context);
            edge (id, state (scopes[s.cleanup].first, c), NEWTM_FLOW_UNWIND);
          }
        else if (s.view.kind == NEWTM_FLOW_CLEANUP)
          {
            if (!context || continuations[context].cleanup_scope != p.scope)
              { refuse (s.view.owner, "V08: cleanup exit has no matching continuation"); return; }
            edge (id, route (s.view.parent, p.target, continuations[context].parent), NEWTM_FLOW_UNWIND);
          }
        else edge (id, route (s.view.parent, p.target, context), NEWTM_FLOW_UNWIND);
        return;
      }
    if (p.kind == SEQUENCE_END)
      {
        lexical_scope s = scopes[p.scope];
        if (!p.scope) edge (id, state (normal_exit, 0), NEWTM_FLOW_NEXT);
        else if (s.view.kind == NEWTM_FLOW_CLEANUP)
          {
            if (!context || continuations[context].cleanup_scope != p.scope)
              { refuse (s.view.owner, "V08: cleanup completion has no pending destination"); return; }
            edge (id, continuations[context].resume, NEWTM_FLOW_RESUME);
          }
        else edge (id, route (p.scope, after_scope (p.scope), context), NEWTM_FLOW_NEXT);
        return;
      }
    gimple *s = p.stmt;
    reached.add (s);
    switch (gimple_code (s))
      {
      case GIMPLE_BIND: case GIMPLE_TRY:
        edge (id, state (scopes[p.child].first, context), NEWTM_FLOW_NEXT); break;
      case GIMPLE_RETURN:
        edge (id, route (p.scope, p.target, context), NEWTM_FLOW_RETURN); break;
      case GIMPLE_GOTO:
        edge (id, route (p.scope, *labels.get (gimple_goto_dest (s)), context), NEWTM_FLOW_GOTO); break;
      case GIMPLE_COND:
        edge (id, route (p.scope, *labels.get (gimple_cond_true_label (as_a<gcond *> (s))), context), NEWTM_FLOW_TRUE);
        edge (id, route (p.scope, *labels.get (gimple_cond_false_label (as_a<gcond *> (s))), context), NEWTM_FLOW_FALSE); break;
      case GIMPLE_SWITCH:
        for (unsigned i = 0; i < gimple_switch_num_labels (as_a<gswitch *> (s)); ++i)
          {
            tree c = gimple_switch_label (as_a<gswitch *> (s), i);
            edge (id, route (p.scope, *labels.get (CASE_LABEL (c)), context), NEWTM_FLOW_CASE, c);
          }
        break;
      case GIMPLE_CALL:
        if (p.target != absent)
          { edge (id, state (p.target, 0), NEWTM_FLOW_TERMINATE); break; }
        /* Fall through. */
      default: edge (id, state (p.next, context), NEWTM_FLOW_NEXT); break;
      }
  }
  void count_cycles ()
  {
    auto_vec<unsigned char> colors;
    colors.safe_grow_cleared (nodes.length ());
    auto_vec<dfs_frame> stack;
    dfs_frame first = { 0, 0 }; stack.safe_push (first); colors[0] = 1;
    while (!stack.is_empty ())
      {
        dfs_frame &f = stack.last ();
        if (f.edge == nodes[f.node]->successors.length ())
          { colors[f.node] = 2; stack.pop (); continue; }
        unsigned to = nodes[f.node]->successors[f.edge++].target;
        if (colors[to] == 1) ++totals.cycle_edges;
        else if (!colors[to])
          { colors[to] = 1; dfs_frame next = { to, 0 }; stack.safe_push (next); }
      }
  }
  bool build_cfg (function *fn, bool surrounding)
  {
    if (attempted) return refuse (NULL, "V08: flow graph must be rebuilt for each inspection");
    attempted = true;
    if (!fn || !fn->cfg) return refuse (NULL, "V08: missing current CFG");
    lexical_scope scope = { { NEWTM_FLOW_ROOT, 0, NULL }, absent, absent, absent };
    scopes.safe_push (scope);
    auto node = [&] (gimple *stmt, newtm_flow_node_kind kind) {
      newtm_flow_node *n = new newtm_flow_node;
      n->kind = kind; n->statement = stmt; n->origin = NULL;
      n->scope = n->continuation = 0; n->reachable = false;
      unsigned id = nodes.length (); nodes.safe_push (n);
      if (stmt) statements.put (stmt, id);
      return id;
    };
    node (NULL, NEWTM_FLOW_ENTRY);
    auto_vec<unsigned> first, last;
    first.safe_grow_cleared (last_basic_block_for_fn (fn));
    last.safe_grow_cleared (last_basic_block_for_fn (fn));
    basic_block bb;
    FOR_EACH_BB_FN (bb, fn)
      {
        if (!gsi_end_p (gsi_start_phis (bb))) return refuse (NULL, "V08: SSA phi requires a separate flow rule");
        unsigned previous = absent;
        for (gimple_stmt_iterator it = gsi_start_bb (bb); !gsi_end_p (it); gsi_next (&it))
          {
            gimple *stmt = gsi_stmt (it);
            if (gimple_code (stmt) == GIMPLE_BIND || gimple_code (stmt) == GIMPLE_TRY
                || is_gimple_omp (stmt)) return refuse (stmt, "V08: structured operation in lowered CFG");
            switch (gimple_code (stmt))
              {
              case GIMPLE_ASSIGN: case GIMPLE_COND: case GIMPLE_SWITCH:
              case GIMPLE_LABEL: case GIMPLE_RETURN: case GIMPLE_NOP:
              case GIMPLE_DEBUG: case GIMPLE_PREDICT: break;
              case GIMPLE_RESX: case GIMPLE_EH_DISPATCH:
                if (!surrounding) return refuse (stmt, "V08: exception transfer in prepared memory CFG");
                break;
              case GIMPLE_GOTO:
                if (TREE_CODE (gimple_goto_dest (stmt)) != LABEL_DECL)
                  return refuse (stmt, "V08: computed goto in memory CFG");
                break;
              case GIMPLE_CALL:
                if ((gimple_call_flags (stmt) & ECF_RETURNS_TWICE)
                    || (gimple_call_ctrl_altering_p (stmt)
                        && !(gimple_call_flags (stmt) & ECF_NORETURN)))
                  return refuse (stmt, "V08: nonlocal control in memory CFG");
                if (gimple_call_internal_p (stmt))
                  {
                    internal_fn code = gimple_call_internal_fn (stmt);
                    if (code != IFN_ADD_OVERFLOW && code != IFN_SUB_OVERFLOW && code != IFN_MUL_OVERFLOW)
                      return refuse (stmt, "V08: unclassified internal effect in memory CFG");
                  }
                break;
              default: return refuse (stmt, "V08: unclassified effect in memory CFG");
              }
            if (statements.get (stmt)) return refuse (stmt, "V08: repeated instruction in memory CFG");
            unsigned id = node (stmt, NEWTM_FLOW_STMT);
            if (previous != absent) edge (previous, id, NEWTM_FLOW_NEXT);
            else first[bb->index] = id;
            previous = id;
          }
        if (previous == absent) first[bb->index] = previous = node (NULL, NEWTM_FLOW_TRANSFER);
        last[bb->index] = previous;
        gimple *tail = nodes[previous]->statement;
        if (tail && is_gimple_call (tail) && gimple_call_noreturn_p (tail)
            && EDGE_COUNT (bb->succs) == 0)
          {
            unsigned terminal = node (NULL, NEWTM_FLOW_TERMINATE_EXIT);
            nodes[terminal]->origin = tail;
            edge (previous, terminal, NEWTM_FLOW_TERMINATE);
          }
      }
    unsigned exit = node (NULL, NEWTM_FLOW_NORMAL_EXIT);
    struct edge_def *e; edge_iterator ei;
    FOR_EACH_EDGE (e, ei, ENTRY_BLOCK_PTR_FOR_FN (fn)->succs)
      edge (0, e->dest == EXIT_BLOCK_PTR_FOR_FN (fn) ? exit : first[e->dest->index], NEWTM_FLOW_NEXT);
    FOR_EACH_BB_FN (bb, fn)
      FOR_EACH_EDGE (e, ei, bb->succs)
        {
          if ((e->flags & EDGE_ABNORMAL) || (!surrounding && (e->flags & EDGE_EH)))
            return refuse (nodes[last[bb->index]]->statement, "V08: unsupported exceptional CFG edge");
          gimple *tail = nodes[last[bb->index]]->statement;
          if (tail && gimple_code (tail) == GIMPLE_RETURN)
            {
              if (e->dest != EXIT_BLOCK_PTR_FOR_FN (fn))
                return refuse (tail, "V08: return has a non-exit CFG successor");
              unsigned returned = node (NULL, NEWTM_FLOW_RETURN_EXIT);
              nodes[returned]->origin = tail;
              edge (last[bb->index], returned, NEWTM_FLOW_RETURN);
              continue;
            }
          if (tail && gimple_code (tail) == GIMPLE_SWITCH)
            {
              bool found = false;
              for (unsigned i = 0; i < gimple_switch_num_labels (as_a<gswitch *> (tail)); ++i)
                {
                  tree c = gimple_switch_label (as_a<gswitch *> (tail), i);
                  if (label_to_block (fn, CASE_LABEL (c)) == e->dest)
                    { edge (last[bb->index], first[e->dest->index], NEWTM_FLOW_CASE, c); found = true; }
                }
              if (!found) return refuse (tail, "V08: switch CFG edge lacks a case selector");
              continue;
            }
          newtm_flow_edge_kind kind = e->flags & EDGE_EH ? NEWTM_FLOW_UNWIND
            : e->flags & EDGE_TRUE_VALUE ? NEWTM_FLOW_TRUE
            : e->flags & EDGE_FALSE_VALUE ? NEWTM_FLOW_FALSE : NEWTM_FLOW_NEXT;
          edge (last[bb->index], e->dest == EXIT_BLOCK_PTR_FOR_FN (fn) ? exit : first[e->dest->index], kind);
        }
    /* Build reachability after all edges, including back edges, exist. */
    auto_vec<unsigned> pending; pending.safe_push (0); nodes[0]->reachable = true;
    for (unsigned i = 0; i < pending.length (); ++i)
      {
        unsigned at = pending[i];
        if (nodes[at]->statement) reached.add (nodes[at]->statement);
        for (const newtm_flow_edge &e : nodes[at]->successors)
          if (!nodes[e.target]->reachable) { nodes[e.target]->reachable = true; pending.safe_push (e.target); }
      }
    totals.nodes = nodes.length (); totals.statements = statements.elements ();
    totals.reachable_statements = reached.elements ();
    count_cycles ();
    return true;
  }
  bool build (gimple_seq seq)
  {
    if (attempted) return refuse (NULL, "V08: flow graph must be rebuilt for each inspection");
    attempted = true;
    index_sequence (seq, 0, NEWTM_FLOW_ROOT, absent);
    if (reason || !validate_targets ()) return false;
    totals.statements = statements.elements ();
    normal_exit = add_position (NORMAL_EXIT, 0);
    unsigned entry = add_position (ENTRY, 0);
    continuation none = { absent, absent, absent }; continuations.safe_push (none);
    state (entry, 0);
    nodes[0]->reachable = true; worklist.safe_push (0);
    /* Canonical states are expanded once. Back edges reuse states; cleanup
       continuations depend on lexical exits, never on loop iteration count. */
    for (unsigned i = 0; i < worklist.length () && !reason; ++i) expand (worklist[i]);
    if (reason) return false;
    totals.nodes = nodes.length (); totals.reachable_statements = reached.elements ();
    totals.continuations = continuations.length () - 1;
    count_cycles ();
    return true;
  }
};

newtm_flow_graph::newtm_flow_graph () : m_builder (new newtm_flow_builder) {}
newtm_flow_graph::~newtm_flow_graph () { delete m_builder; }
bool newtm_flow_graph::build_region (const newtm_flow_graph &source,
                                    unsigned entry, unsigned exit,
                                    const char **why, gimple **bad)
{
  newtm_flow_builder &b = *m_builder;
  bool ok = !b.attempted && entry < source.node_count ()
    && exit < source.node_count () && entry != exit;
  b.attempted = true;
  if (!ok) b.refuse (NULL, "V08: invalid current controller boundary");
  if (ok)
    {
      lexical_scope scope = { { NEWTM_FLOW_ROOT, 0, NULL }, absent, absent, absent };
      b.scopes.safe_push (scope);
      auto_vec<unsigned> originals, mapped;
      mapped.safe_grow (source.node_count ());
      for (unsigned &i : mapped) i = absent;
      originals.safe_push (entry); mapped[entry] = 0;
      for (unsigned at = 0; at < originals.length (); ++at)
        {
          unsigned old = originals[at]; const newtm_flow_node &n = source.node (old);
          newtm_flow_node *copy = new newtm_flow_node;
          copy->kind = old == exit ? NEWTM_FLOW_NORMAL_EXIT : n.kind;
          copy->statement = old == exit ? NULL : n.statement;
          copy->origin = n.origin; copy->scope = copy->continuation = 0;
          copy->reachable = true; b.nodes.safe_push (copy);
          if (old != exit && n.kind == NEWTM_FLOW_NORMAL_EXIT)
            b.refuse (n.origin, "V08: controller leaves the function before its lifetime boundary");
          if (copy->statement)
            { b.statements.put (copy->statement, at); b.reached.add (copy->statement); }
          if (old == exit) continue;
          for (const newtm_flow_edge &e : n.successors)
            {
              if (mapped[e.target] == absent)
                { mapped[e.target] = originals.length (); originals.safe_push (e.target); }
              newtm_flow_edge edge = { mapped[e.target], e.kind, e.selector };
              copy->successors.safe_push (edge);
            }
          if (copy->successors.is_empty ()
              && n.kind != NEWTM_FLOW_RETURN_EXIT && n.kind != NEWTM_FLOW_TERMINATE_EXIT)
            b.refuse (n.statement, "V08: controller path ends outside its current boundary");
        }
      for (unsigned at = 0; at < originals.length (); ++at)
        {
          unsigned old = originals[at];
          if (old != entry && old != exit)
            for (unsigned pred : source.node (old).predecessors)
              if (source.node (pred).reachable && (mapped[pred] == absent || pred == exit))
                b.refuse (source.node (old).statement, "V08: external entry bypasses controller initialization");
          for (const newtm_flow_edge &e : b.nodes[at]->successors)
            b.nodes[e.target]->predecessors.safe_push (at);
        }
      if (mapped[exit] == absent)
        b.refuse (NULL, "V08: controller lost its normal continuation");
      b.totals.nodes = b.nodes.length (); b.totals.statements = b.statements.elements ();
      b.totals.reachable_statements = b.reached.elements (); b.count_cycles ();
    }
  if (why) *why = b.reason;
  if (bad) *bad = b.bad;
  return !b.reason;
}
bool newtm_flow_graph::build (gimple_seq seq, const char **why, gimple **bad)
{
  bool ok = m_builder->build (seq);
  if (why) *why = m_builder->reason;
  if (bad) *bad = m_builder->bad;
  return ok;
}
bool newtm_flow_graph::build_cfg (function *fn, const char **why, gimple **bad, bool surrounding)
{
  bool ok = m_builder->build_cfg (fn, surrounding);
  if (why) *why = m_builder->reason;
  if (bad) *bad = m_builder->bad;
  return ok;
}
unsigned newtm_flow_graph::node_count () const { return m_builder->nodes.length (); }
const newtm_flow_node &newtm_flow_graph::node (unsigned n) const { return *m_builder->nodes[n]; }
unsigned newtm_flow_graph::scope_count () const { return m_builder->scopes.length (); }
const newtm_flow_scope &newtm_flow_graph::scope (unsigned n) const { return m_builder->scopes[n].view; }
bool newtm_flow_graph::contains (gimple *s) const { return m_builder->statements.get (s) != NULL; }
bool newtm_flow_graph::reachable (gimple *s) const { return m_builder->reached.contains (s); }
newtm_flow_summary newtm_flow_graph::summary () const { return m_builder->totals; }
