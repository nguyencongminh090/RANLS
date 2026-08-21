# 2026-08-21 — RT-04: both tree views fully rebuild many times per second during analysis

**Task:** `docs/todo/RT-04-tree-views-full-rebuild.md` (Active, Sprint 3, P1). Session note: the
code fix was implemented by a prior agent session in an isolated worktree
(`agent-ae2989c7fa02b5a22`, uncommitted) that was interrupted before verification and tracking-file
bookkeeping. This entry covers a resumed session that recovered that diff into a fresh worktree,
verified it, fixed a bug found in the recovered test file, and closed out the tracking files. No
`docs/instruction/RT-04-*.md` entry exists — expected, per `instruction.md`'s convention (not every
task has one).

## Problem

`GameState::setAnalysisData` emitted `signal_tree_updated` synchronously whenever the current tree
node's `depth` or `nodes` changed — essentially once per parsed engine line during a search (up to
8x per depth iteration with multiPV=8). That signal drove a full rebuild of both tree views:

- `TreeExplorer::update` called `store_->remove_all()` then re-appended every row, resetting the
  `Gtk::ColumnView`'s scroll position on every tick.
- `TreeNodeView::update` re-ran the whole layout; `layoutTree` built each node's path by scanning
  `nodes_` backwards to find the parent, making layout O(n²) in tree size.

## Fix

`src/model/game_state.h`/`.cpp`: added a `treeDirty_` flag mirroring RT-01's `analysisDirty_`.
`setAnalysisData` now sets `treeDirty_ = true` (instead of emitting `signal_tree_updated`) only when
the current node's `depth`/`nodes` actually changed. `tickAnalysis()` and `flush()` emit
`signal_tree_updated` right after `signal_engine_analysis` whenever `treeDirty_` is set, reusing
RT-01's existing ~75ms `Glib::signal_timeout` tick (`src/main_window.cpp:111`) instead of adding a
second throttle mechanism. Structural tree changes (`makeMove`, `gotoPath`, `undoMove`/`redoMove`,
`loadPosition`) are untouched and still emit `signal_tree_updated` synchronously — correct, since
those really do change tree structure and aren't part of the per-tick eval-refresh flood.

`src/ui/tree_explorer.cpp`: `TreeExplorer::update` no longer calls `store_->remove_all()`. It
builds the new row set off to the side, then diffs it into the existing `Gio::ListStore` with
`splice()`: only rows whose displayed fields actually changed are replaced in place; a size delta
is spliced at the tail (grow) or truncated (shrink). An update where nothing changed touches the
store not at all, preserving scroll position.

`src/ui/tree_node_view.h`/`.cpp`: `layoutTree` now threads the parent's already-built `path` and
its own index into `nodes_` (new `NodeLayout::parentIndex` field) down through the recursion,
instead of scanning `nodes_` backwards for a `TreeNode*` match to rebuild each child's path — O(1)
per node instead of O(n), so O(n) total instead of O(n²). `onDraw`'s elbow-line-drawing pass used
the same backward scan to find each node's parent layout; it now also uses `parentIndex`, so that
pass drops from O(n²) to O(n) as well (a small bonus beyond the todo file's stated scope, but the
same underlying pattern the todo file called out). Column/row assignment (branch/depth) is
untouched, so node positions remain stable across non-structural updates, same as before.

## Verification

- `bash build.sh` (Ninja/Release): clean build, both `rapfi-gui` and `tests/rapfi-gui-tests` link
  with no new warnings (pre-existing unused-function warnings in `gomocup_protocol.cpp` are
  unrelated and unchanged).
- `ctest --test-dir build_cmd --output-on-failure`: 65/65 test cases, 260/260 assertions pass,
  including 5 new cases added in `tests/test_rt04_tree_signal.cpp` (registered in
  `tests/CMakeLists.txt`): a depth-iteration burst leaves `signal_tree_updated` unemitted until a
  tick; a multiPV=8 storm (80 PV commits) collapses to exactly one emission per tick; `flush()`
  delivers a pending tree update immediately without waiting for the timer; repeating the same
  depth/nodes leaves `treeDirty_` clear (no spurious emission) even though `analysisDirty_` is set
  again; and a structural change (`makeMove`) still emits `signal_tree_updated` synchronously,
  unaffected by the throttle.
- **Bug found and fixed in the recovered test file during this session:** three of the five test
  cases used a `Harness` struct that connects `signal_tree_updated` in its constructor, then called
  `makeMove()` — itself a structural change that emits `signal_tree_updated` synchronously — before
  asserting `treeUpdatedCount == 0`. That assertion didn't account for `makeMove`'s own baseline
  emission and failed (`1 == 0` etc.) on the first build/test run. Fixed by capturing
  `const int baseline = h.treeUpdatedCount;` immediately after `makeMove()` and asserting relative
  to `baseline` in the three affected cases, rather than loosening or discarding the assertions.
- `tree_explorer.cpp`/`tree_node_view.cpp` are GTK4 widget code, outside the headless doctest
  harness by design (`tests/CMakeLists.txt` keeps the model layer buildable/testable without
  glibmm/gtkmm or a GTK main loop). The `Gio::ListStore::splice()` in-place update and the O(n)
  `layoutTree` parent-path threading were verified by reading the code path (traced `splice()`'s
  three call sites against old/new row-count deltas; traced `parentPath`/`parentIndex` being passed
  one level down at each recursive call and never rescanning `nodes_`), not by an automated or
  interactive GUI test — no display server is available in this environment to drive a live
  `Gtk::ColumnView`/`DrawingArea` and confirm scroll-position/redraw behavior visually. Stated
  explicitly per `CLAUDE.md`'s bug-fix workflow rule rather than skipped silently.

## Status

Marked ✅ in `TODO.md`'s Active line and in `docs/todo/RT-04-tree-views-full-rebuild.md`'s `Status`
field — all four acceptance criteria are met by code reading and the passing test suite. The one
open gap is live/interactive GUI verification (scroll-position preservation, visual node-position
stability), which this headless environment cannot perform; noted above rather than glossed over.
