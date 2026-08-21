# UI-02 — Tree "Table" tab can't jump and shows no current path; the two tree views disagree

**Status:** ✅ DONE
**Area:** tree explorer vs tree node view
**Priority:** P1
**Source:** UI/UX + codebase review, 2026-08-21

## Resolution (2026-08-21)

Implemented in `src/ui/tree_explorer.{h,cpp}`, `src/ui/analysis_panel.cpp`, `src/resources/style.css`,
plus a new gtkmm-free predicate header `src/model/tree_row_highlight.h` and its test
`tests/test_ui02_tree_row_highlight.cpp` (wired into `tests/CMakeLists.txt`). See
`docs/fix-log/2026-08-21-ui-02-tree-table-tab-click-to-jump-and-highlight.md` for the full writeup.

Acceptance criteria, checked:
- **Click-to-jump**: switched the selection model from `Gtk::NoSelection` to `Gtk::SingleSelection`
  (last criterion below), and connected `selection_->signal_selection_changed()` to emit
  `signal_node_selected(row->path)`; `AnalysisPanel::connectSignals()` wires that to
  `gameState_.gotoPath(path)` — identical target semantics to the Visual tab's
  `treeNodeView_.signal_node_clicked`.
- **Current-position highlight in both views**: `RowData` gained an `isCurrent` bool, computed by
  `isCurrentHistoryRow(rowIndex, historyCount)` (the new pure-logic header — the last row in
  `TreeExplorer::update()`'s history listing is always the current position, since that listing has
  no "future" rows). The column bind factory adds/removes a `current-row` CSS class per label;
  `.tree-explorer .current-row` in `style.css` bolds it and colors it accent blue. `TreeNodeView`'s
  existing node/edge path highlight (`src/ui/tree_node_view.cpp`) was left as-is per the scope
  boundary below.
- **`signal_node_selected` wired up**: emitted from the real selection-changed handler now, not
  declared-and-dead. Guarded with an `inUpdate_` flag so `update()`'s own
  `selection_->set_selected(...)` call (to keep the selection model in sync with the new current row
  after a rebuild) does not re-emit the signal and fight whatever change triggered the rebuild.
- **Tab-labelling decision**: renamed the notebook tabs from bare "Visual"/"Table" to
  "Visual (All Branches)" / "Table (Current Line)", each with a tooltip spelling out the dataset
  difference (`AnalysisPanel::AnalysisPanel`) — `TreeNodeView` renders the whole `VariationTree`,
  `TreeExplorer` renders only `gameState_.history()` (the current line). Chose label text + tooltip
  over a separate header widget: keeps the fix scoped to the notebook's existing tab-label mechanism
  rather than adding new layout.
- **Selection model reviewed**: `Gtk::NoSelection` was the source of the manual-gesture workaround
  (`GtkGestureClick` with no reliable way to map a click to a row/path); switched to
  `Gtk::SingleSelection`, which gives row activation (click and keyboard) via
  `signal_selection_changed()` for free and removed the placeholder gesture handler entirely.

## Verification

`RUN_TESTS=1 bash build.sh` (clean `build_cmd`, Ninja/Release): full app + test binary both compile
with no errors (only pre-existing, unrelated `-Wunused-function` warnings in
`src/engine/gomocup_protocol.cpp`). `ctest --output-on-failure`:
```
Test project .../build_cmd
    Start 1: rapfi-gui-tests
1/1 Test #1: rapfi-gui-tests ..................   Passed    0.01 sec
100% tests passed, 0 tests failed out of 1
```
`doctest`'s own listing confirms all 4 new cases ran as part of that binary's 64 total cases:
`isCurrentHistoryRow: empty history has no current row`, `single-row history marks row 0 current`,
`only the last row is current`, `does not match an index beyond the row count` — all passing.

Not verified: live interactive click behavior (no display server in this sandboxed environment to
run the app and click a Table-tab row). Verified by code reading instead: `signal_selection_changed`
is gtkmm's standard row-activation signal for `Gtk::SingleSelection`, and the `inUpdate_` guard is
the only place that could suppress a genuine user click (it's set/cleared strictly inside `update()`,
never held across a user interaction).

## Problem

`TreeExplorer` (Table tab) and `TreeNodeView` (Visual tab) render the same `VariationTree` but
behave differently. The `ui-ux-review` checklist (item 5) requires them to agree on current-path
highlighting and click-to-jump.

### 1. Click-to-jump is a stub in the Table tab

`src/ui/tree_explorer.cpp:57-65`:

```cpp
click->signal_released().connect(
    [](int, double, double) {
        // In GTK4 ColumnView with NoSelection, we use position-based lookup.
        // For now, this is a placeholder for a more sophisticated selection model.
    });
```

The Visual tab jumps correctly via `signal_node_clicked` → `gotoPath`
(`src/ui/analysis_panel.cpp:109-111`). The Table tab does nothing on click.

`TreeExplorer::signal_node_selected` is declared (`src/ui/tree_explorer.h:17`) and never emitted;
`RowData` already carries the `path` needed to make it work (`src/ui/tree_explorer.cpp:109-115`).

### 2. No current-path highlight in the Table tab

`TreeExplorer::update` lists history rows with no styling to indicate which row is the current
position (`src/ui/tree_explorer.cpp:70-119`). `TreeNodeView` highlights both nodes and connecting
lines on the path (`src/ui/tree_node_view.cpp:173-178`, `:216-222`).

### 3. The two views show different things

`TreeExplorer` is fed `gameState_.history()` — the current line only. `TreeNodeView` is fed
`gameState_.tree().root()` — the entire variation tree (`src/ui/analysis_panel.cpp:82-84`). Labelling
them "Table" and "Visual" implies two renderings of one thing; they are actually two different
datasets. Either is defensible, but the tabs should say which is which.

## Acceptance criteria

- Clicking a row in the Table tab jumps to that position, with the same target semantics as the
  Visual tab (`gotoPath` with the row's `path`).
- The current position is visually marked in both views.
- `signal_node_selected` is wired up or removed.
- The relationship between the two tabs is clear from their labels or a short header — a user should
  not have to guess why the Visual tab shows branches the Table tab doesn't.
- Selection model reviewed: `Gtk::NoSelection` (`src/ui/tree_explorer.cpp:29`) is what forces the
  manual gesture workaround; `SingleSelection` is likely the right choice.

## Scope boundary

- Rebuild cost / scroll-position loss is RT-04.
- Node layout quality in the Visual tab is out of scope.

## Related

- RT-04 (tree rebuild), UI-01 (eval values shown in the table's Eval column)
