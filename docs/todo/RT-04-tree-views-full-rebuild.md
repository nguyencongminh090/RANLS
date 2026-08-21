# RT-04 — Both tree views fully rebuild many times per second during analysis

**Status:** ✅ DONE
**Area:** tree explorer / tree node view
**Priority:** P1
**Source:** UI/UX + codebase review, 2026-08-21

## Resolution (2026-08-21)

- `GameState::setAnalysisData` no longer emits `signal_tree_updated` synchronously. It sets a new
  `treeDirty_` flag (mirroring RT-01's `analysisDirty_`) when the current node's `depth`/`nodes`
  actually change; `tickAnalysis()`/`flush()` emit `signal_tree_updated` right after
  `signal_engine_analysis` when `treeDirty_` is set, consuming both on the same ~75ms
  `Glib::signal_timeout` tick RT-01 already installed (`src/main_window.cpp:111`). Structural tree
  changes (`makeMove`, `gotoPath`, `undoMove`/`redoMove`, `loadPosition`) still emit
  `signal_tree_updated` synchronously — unaffected, and correct: those are real structure changes,
  not per-tick eval refreshes.
- `TreeExplorer::update` no longer calls `store_->remove_all()`. It builds the new row set, then
  diffs it against the existing `Gio::ListStore` with `splice()`: unchanged rows are left alone
  (only rows whose displayed data changed are replaced), and any size delta is spliced at the tail.
  Preserves the ColumnView's scroll position across an update that doesn't touch most rows.
- `TreeNodeView::layoutTree` threads the parent's already-built `path` (and its `NodeLayout` index
  in `nodes_`, via new `NodeLayout::parentIndex`) down through the recursion instead of scanning
  `nodes_` backwards for the parent — O(1) per node, O(n) total. `onDraw`'s elbow-line pass was
  using the same backward scan to find each node's parent layout for drawing; it now uses
  `parentIndex` too, so that pass is also O(n) instead of O(n²).
- Column/row assignment (`nextCol`, `depth`) is untouched, so node positions are unaffected by this
  change beyond being stable, as before, across updates that don't change tree structure.

## Verification

- `bash build.sh` (Release, Ninja) — clean build, `rapfi-gui` and `tests/rapfi-gui-tests` both link.
- `ctest --test-dir build_cmd --output-on-failure` — 65/65 test cases, 261/261 assertions pass,
  including the 5 new cases in `tests/test_rt04_tree_signal.cpp` covering: a depth-burst not
  emitting `signal_tree_updated` until a tick; a multiPV=8 storm collapsing to exactly one emission
  per tick; `flush()` delivering a pending tree update immediately; an unchanged depth/nodes leaving
  `treeDirty_` clear (no spurious emission); and a structural change (`makeMove`) still emitting
  synchronously, unaffected by the throttle.
- `tree_explorer.cpp`/`tree_node_view.cpp` are GTK widget code, outside the headless doctest
  harness by design (see `tests/CMakeLists.txt`'s standalone-model-layer note) — the in-place
  `Gio::ListStore::splice()` update and the O(n) `layoutTree` parent-path threading were verified
  by reading the code path, not by an automated test; no GUI-level regression test exists for
  either (would require driving a real `Gtk::ColumnView`/`DrawingArea`, out of scope for this
  fix's harness).
- Session note: this fix was implemented by a prior agent session in worktree
  `agent-ae2989c7fa02b5a22` and recovered/finished by a resumed session — the resumed session found
  and fixed a pre-existing bug in the recovered `tests/test_rt04_tree_signal.cpp`: three test cases
  asserted `treeUpdatedCount == 0` immediately after `makeMove()`, not accounting for `makeMove`'s
  own synchronous `signal_tree_updated` emission (a structural change, correctly unthrottled) as a
  baseline. Fixed by capturing `baseline = h.treeUpdatedCount` right after `makeMove()` and
  asserting relative to it.

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
