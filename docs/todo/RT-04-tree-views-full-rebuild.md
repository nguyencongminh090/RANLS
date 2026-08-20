# RT-04 — Both tree views fully rebuild many times per second during analysis

**Status:** open
**Area:** tree explorer / tree node view
**Priority:** P1
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

`GameState::setAnalysisData` emits `signal_tree_updated` whenever the current node's `depth` **or**
`nodes` changed (`src/model/game_state.cpp:211-221`) — i.e. on essentially every analysis update
during a search.

That signal drives a full rebuild of both tree views (`src/ui/analysis_panel.cpp:80-85`):

- **`TreeExplorer::update`** (`src/ui/tree_explorer.cpp:70-119`) calls `store_->remove_all()` and
  re-appends every row. This discards the user's scroll position on every tick.
- **`TreeNodeView::update`** (`src/ui/tree_node_view.cpp:83-100`) re-runs the whole layout and calls
  `set_size_request` each time. Worse, `layoutTree` builds each node's path by scanning `nodes_`
  backwards to find the parent (`src/ui/tree_node_view.cpp:130-137`), making layout **O(n²)** in
  tree size.

## Why it matters

The tree views are meant to be browsable *while* the engine analyzes. Currently the table scrolls
itself back to the top continuously and the graph view re-lays-out constantly, which also risks
nodes appearing to jump (the `ui-ux-review` checklist calls out layout stability explicitly).

## Acceptance criteria

- `signal_tree_updated` is not emitted at analysis-update frequency for what is only an eval/depth
  refresh of a single node — either a narrower signal carrying the changed node, or coalesced with
  RT-01's tick.
- `TreeExplorer` updates rows in place (its `Gio::ListStore` already supports per-item mutation) and
  preserves scroll position across updates.
- `TreeNodeView::layoutTree` builds paths in O(n) — pass the parent's path down through the
  recursion instead of searching `nodes_` for it.
- Node positions stay stable across updates that do not change tree structure.

## Scope boundary

- Missing click-to-jump and current-path highlight in `TreeExplorer` are UI-02, not this item.
- The tree layout *algorithm* (column assignment / elbow routing) is not in scope beyond the O(n²)
  path construction; a nicer layout is a separate design question.

## Related

- RT-01 (shared update-rate root cause), UI-02 (tree view feature parity)
