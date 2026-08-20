# STATE-01 — Stale PV / engine status / board markers survive a position change

**Status:** open
**Area:** GameState ↔ analysis UI lifecycle
**Priority:** P0
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

`pvLines_` and `engineStatus_` are analysis results for **one specific position**, but their
lifetime is not tied to the position's lifetime. Two distinct defects:

### (a) Cleared, but no signal — the UI never finds out

`newGame` (`src/model/game_state.cpp:9-23`) and `makeMove` (`src/model/game_state.cpp:52-76`) both
clear `pvLines_` and reset `engineStatus_`, then emit `signal_board_changed` and
`signal_tree_updated` — but **not** `signal_engine_analysis`.

`AnalysisPanel` only refreshes `PVView` and `EngineStatusView` from `signal_engine_analysis`
(`src/ui/analysis_panel.cpp:71-77`). Its `signal_board_changed` handler
(`src/ui/analysis_panel.cpp:88-95`) refreshes only the win graph and the tree views.

Result: after **New Game** or after playing a move, the PV list and the D/N/NPS/T/Eval/Best readout
still display the previous position's data.

### (b) Not cleared at all — undo/redo

`undoMove` (`src/model/game_state.cpp:78-96`) and `redoMove`
(`src/model/game_state.cpp:98-120`) never touch `pvLines_` or `engineStatus_`.

`MainWindow`'s `signal_board_changed` handler clears `boardViewModel_.candidateMoves`
(`src/main_window.cpp:280`) — but then calls `boardViewModel_.update()` on the next line, which
repopulates `candidateMoves` straight back from the stale `state_.pvLines()`
(`src/model/board_view_model.cpp:46-64`).

Result: **after undo/redo the board still paints the previous position's candidate-move markers**,
with winrate labels that belong to a position no longer on the board.

## Why it matters

This is the worst class of bug for an analysis tool: the UI confidently displays engine numbers that
do not describe the position shown. A user reading Eval/Best after an undo is reading a lie.

## Acceptance criteria

- Any operation that changes the current position (`newGame`, `loadPosition`, `makeMove`,
  `undoMove`, `redoMove`, `gotoMove`, `gotoPath`) clears `pvLines_` + `engineStatus_` and notifies
  the analysis UI, through one shared code path rather than six copies.
- After New Game: PV list empty, engine status readout back to `-`.
- After undo/redo: no candidate markers on the board, PV list empty.
- Regression test covering at least: makeMove→undo→assert `pvLines()` empty, and
  setAnalysisData→newGame→assert the analysis signal fired and data is cleared.

## Scope boundary

- Do not fold in the empty-state *message* work (UX-01) — clearing and "showing a placeholder" are
  separate items.
- Do not change `setAnalysisData`'s tree-eval writeback in this item beyond what the shared reset
  path requires; the eval-writeback condition bug is UI-03.

## Testing note

Per `/CLAUDE.md` ("Bug-fix workflow"), this needs a regression test. **The repo currently has no
test infrastructure at all** — no test target in `CMakeLists.txt`, no test directory. Standing up a
minimal test target for `src/model/` (which has no GTK dependency and is the right place to test
this) is a prerequisite and should be scoped as part of this item or split into its own.

## Related

- STATE-03 (stale/empty PVLine slots inside the protocol), UX-01 (empty states), UI-03 (eval writeback)
