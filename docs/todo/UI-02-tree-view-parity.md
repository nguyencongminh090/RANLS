# UI-02 — Tree "Table" tab can't jump and shows no current path; the two tree views disagree

**Status:** open
**Area:** tree explorer vs tree node view
**Priority:** P1
**Source:** UI/UX + codebase review, 2026-08-21

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
