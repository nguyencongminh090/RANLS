# Sprint 3 (closed 2026-08-21)

**Goal:** Clear the P1 backlog surfaced by the 2026-08-21 `src/` review — wrong results and wasted
work: hardcoded board size, unbounded PV-slot growth, tree-view rebuild cost, `undoAll`/`redoAll`
flooding, and win-rate/tree-table attribution errors.
**Dates:** 2026-08-21 to 2026-08-21 (same-day close — all six items dispatched, landed on `main`).

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| PROTO-02 | Hardcoded board size 15 in coordinate parsing, `Best:` readout, and star points — breaks every non-15×15 board | ✅ DONE |
| STATE-03 | `currentPVs_` never shrinks and materialises empty PV slots rendered as garbage rows | ✅ DONE |
| RT-04 | Both tree views fully rebuild many times per second during analysis; `layoutTree` is O(n²) | ✅ DONE |
| NAV-01 | `undoAll`/`redoAll` send one database query and rebuild the whole UI per ply | ✅ DONE |
| UI-01 | Win-rate graph attributes evals to the wrong side (off by one ply); evals can go unrecorded | ✅ DONE |
| UI-02 | Tree "Table" tab can't click-to-jump and shows no current path; the two tree views disagree | ✅ DONE |

Points were never estimated this sprint. Final burndown row (before `docs/sprint/burndown.md` was
reset for Sprint 4): `2026-08-21 | 0 / 6 items | — | All six items landed same-day`.

## What shipped

- **PROTO-02:** `parseEngineCoord` now takes the real board size instead of a hardcoded 15;
  `EngineStatusView`'s "Best:" readout and `BoardRenderer`'s star points use the real board size
  too; `sendConfig()` refreshes `GomocupProtocol::boardSize_` even while the engine is stopped. See
  `docs/fix-log/2026-08-21-proto-02-hardcoded-board-size-15.md`.
- **STATE-03:** `GomocupProtocol::commitPV` truncates `currentPVs_` to size 1 when a new round's PV
  index 0 arrives with stale higher-index entries from a larger previous round; `PVView::update`
  filters out empty-move `PVLine`s before rendering. See
  `docs/fix-log/2026-08-21-state-03-currentpvs-never-shrinks.md`.
- **RT-04:** `GameState::setAnalysisData` coalesces `signal_tree_updated` onto RT-01's tick/flush via
  a `treeDirty_` flag; `TreeExplorer::update` diffs rows with `splice()` instead of
  `remove_all()`+re-append; `TreeNodeView::layoutTree` threads the parent path/index through
  recursion instead of scanning `nodes_`, making layout O(n) instead of O(n²). See
  `docs/fix-log/2026-08-21-rt-04-tree-views-full-rebuild.md`.
- **NAV-01:** Split `undoMove()`/`redoMove()` into silent position-mutation halves so
  `undoAll`/`redoAll`/`gotoMove` do exactly one `clearDatabase()`/`signal_board_changed` for the
  whole bulk op instead of once per ply; wired up the previously-dead `signal_move_selected` in
  `gotoMove()`. See `docs/fix-log/2026-08-21-nav-01-bulk-navigation-flooded-engine-and-ui.md`.
- **UI-01:** Fixed off-by-one side-to-move attribution in the win-rate graph, fixed eval writeback
  wrongly gated on depth/nodes only (dropped same-depth score revisions), made unevaluated nodes
  distinguishable from a true 50%; confirmed mate scores already render sanely at both extremes. See
  `docs/fix-log/2026-08-21-ui-01-winrate-attribution-errors.md`.
- **UI-02:** Table tab now click-to-jumps and highlights the current row (`Gtk::NoSelection` →
  `SingleSelection`, wired `signal_node_selected` → `gotoPath`, added current-row CSS); the two tree
  tabs are labelled to show they render different datasets. See
  `docs/fix-log/2026-08-21-ui-02-tree-table-tab-click-to-jump-and-highlight.md`.

## Rolled over to Backlog

Nothing rolled over — all six committed items finished within the sprint.

## Next sprint

Sprint 4 pulls the remaining backlog from the 2026-08-21 `src/` review (UI-03, UX-01, UX-02, UX-03,
UX-04, CLEAN-01) into Active — see `docs/sprint/current.md`.
