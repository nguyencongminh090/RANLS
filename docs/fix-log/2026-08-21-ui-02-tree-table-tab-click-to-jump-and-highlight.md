# 2026-08-21 — UI-02: Tree "Table" tab can't click-to-jump and shows no current path

**Task:** `docs/todo/UI-02-tree-view-parity.md` (Active, P1, Sprint 3 — see
`docs/sprint/current.md`). No `docs/instruction/UI-02-*.md` entry exists — expected, per
`instruction.md`'s convention (not every task has one).

## Problem

`TreeExplorer` (the "Table" tab) and `TreeNodeView` (the "Visual" tab) both render the same
underlying game data but behaved inconsistently:

1. **Click-to-jump was a stub.** `src/ui/tree_explorer.cpp` built a `Gtk::GestureClick` whose
   `signal_released()` handler was an empty placeholder — clicking a Table-tab row did nothing. The
   Visual tab already jumped correctly via `TreeNodeView::signal_node_clicked` →
   `gameState_.gotoPath(path)` (`src/ui/analysis_panel.cpp`). `TreeExplorer::signal_node_selected`
   was declared but never emitted, and `RowData` already carried the `path` needed to make it work.
2. **No current-position highlight in the Table tab.** `TreeExplorer::update()` listed history rows
   with no styling to mark the current position; `TreeNodeView` already highlighted the current
   node/edge path.
3. **The two tabs show different datasets with no indication of that.** `TreeExplorer` is fed
   `gameState_.history()` (current line only); `TreeNodeView` is fed the whole `VariationTree`. Bare
   "Table"/"Visual" labels implied two renderings of the same thing.

## Fix

- **`src/ui/tree_explorer.h`/`.cpp`**: replaced the `Gtk::NoSelection` model (which forced the dead
  manual-gesture workaround) with `Gtk::SingleSelection`, which gives row activation (click and
  keyboard) via `signal_selection_changed()` for free. That handler emits
  `signal_node_selected(row->path)`, guarded by a new `inUpdate_` bool so `update()`'s own
  `selection_->set_selected(...)` call — made to keep the selection reflecting the current row after
  a rebuild — doesn't re-emit the signal and fight whatever triggered the rebuild.
- **`src/model/tree_row_highlight.h`** (new): a gtkmm-free `isCurrentHistoryRow(rowIndex,
  historyCount)` predicate. Because `TreeExplorer::update()` only ever lists the current line up to
  the cursor (no "future"/redo rows), the current position is always the last row — factored out so
  the logic is unit-testable without a display server.
- **`src/ui/tree_explorer.cpp`**: `RowData` gained an `isCurrent` bool computed via that predicate;
  the column bind factory toggles a `current-row` CSS class on the row's labels accordingly.
- **`src/resources/style.css`**: added `.tree-explorer .current-row` (bold, accent blue) — visual
  parity with `TreeNodeView`'s existing current-path highlight.
- **`src/ui/analysis_panel.cpp`**: wired `treeExplorer_.signal_node_selected` to
  `gameState_.gotoPath(path)` (same call the Visual tab already made). Renamed the notebook tabs from
  bare "Visual"/"Table" to "Visual (All Branches)" / "Table (Current Line)", each with a tooltip
  spelling out the dataset difference, instead of adding a separate header widget — keeps the fix
  inside the notebook's existing tab-label mechanism.
- **`tests/test_ui02_tree_row_highlight.cpp`** (new, wired into `tests/CMakeLists.txt`): pins
  `isCurrentHistoryRow`'s behavior — empty history, single-row history, only-the-last-row-is-current
  across a 5-row history, and an out-of-range index doesn't falsely match.

## Verification

`RUN_TESTS=1 bash build.sh` against a clean `build_cmd` (Ninja/Release): full app and test binary
both compile with no errors — only pre-existing, unrelated `-Wunused-function` warnings in
`src/engine/gomocup_protocol.cpp`.

```
Test project .../build_cmd
    Start 1: rapfi-gui-tests
1/1 Test #1: rapfi-gui-tests ..................   Passed    0.01 sec
100% tests passed, 0 tests failed out of 1
```

`doctest`'s test listing confirms all 4 new cases are part of that binary's 64 total cases and pass:
`isCurrentHistoryRow: empty history has no current row`, `single-row history marks row 0 current`,
`only the last row is current`, `does not match an index beyond the row count`.

**Not verified:** live interactive click/keyboard-navigation behavior in the running app — no display
server is available in this sandboxed environment. Verified by code reading instead:
`signal_selection_changed` is gtkmm's standard activation signal for `Gtk::SingleSelection`, and the
`inUpdate_` guard is scoped strictly inside `update()`, so it cannot suppress a genuine user click.

## Status

Marked ✅ in `TODO.md`'s Active line and in `docs/todo/UI-02-tree-view-parity.md`'s `Status` field —
all five acceptance criteria in the todo file are met and the build/tests genuinely pass. RT-04 (tree
rebuild cost) is being finished concurrently in a separate worktree and also touches
`src/ui/tree_explorer.cpp`; that diff will need reconciling with this one when both land on a shared
branch, per this task's own scope boundary (rebuild cost is explicitly out of scope here).
