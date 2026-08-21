# STATE-01 — Stale PV / engine status / board markers survive a position change

**Status:** ✅ FIXED
**Area:** GameState ↔ analysis UI lifecycle
**Priority:** P0
**Source:** UI/UX + codebase review, 2026-08-21

## Summary

Added `GameState::resetAnalysisState()` (`src/model/game_state.cpp`), a single private helper that
clears `pvLines_`, resets `engineStatus_` to `{}`, and emits `signal_engine_analysis` — but only if
there is actually something to clear (idempotent) and only if `analyzing_` is false (defensive
re-check; every caller already guards entry on `analyzing_` before reaching it). Wired into all
seven position-changing operations: `newGame`, `loadPosition`, `makeMove`, `undoMove`, `redoMove`,
`gotoPath` call it directly; `gotoMove` gets it transitively since it loops over `undoMove`/
`redoMove`, each of which already resets.

Went with option 1 from the instruction file (emit `signal_engine_analysis` from the reset helper
itself) rather than also refreshing `PVView`/`EngineStatusView` from `AnalysisPanel`'s
`signal_board_changed` handler — no changes were needed in `src/ui/analysis_panel.cpp` at all, since
its existing `signal_engine_analysis` handler already refreshes `PVView`/`EngineStatusView`
correctly once the signal actually fires on position change.

Removed the now-redundant `boardViewModel_.candidateMoves.clear()` in `src/main_window.cpp`'s
`signal_board_changed` handler: `BoardViewModel::update()` (called on the very next line) already
repopulates `candidateMoves` from `state_.pvLines()`, which is now correctly empty after any
position change, so the explicit clear could only ever paper over a `GameState` bug rather than fix
one. Kept `boardViewModel_.pvPreview.clear()` — that field is set by UI hover interactions, not
`update()`, so it needed its own clear.

Idempotency in `resetAnalysisState()` (skip the clear+emit when already empty/default) addresses the
`undoAll`/`redoAll` flood concern flagged in the instruction file without implementing NAV-01's
batching — each no-op call in the loop after the first is now a cheap early-return with no extra
signal emission.

## Out of scope / left as-is

- **`gotoPath`'s partial-failure path** (can return `false` partway through after already mutating
  `board_`/`history_`/`currentTreeNode_`, at the original lines ~164/165/168/172): left exactly as
  flagged in the instruction file. `resetAnalysisState()` is now called once at the top of
  `gotoPath`, before the mutating loop, same as the pre-existing `pvLines_.clear()` it replaces — so
  this fix does not make that inconsistency any better or worse. Recommend filing this as its own
  tracked item (a `bool`-returning position mutator should either fully commit or fully roll back);
  not fixed here per the explicit "do not attempt a large refactor of gotoPath's control flow" scope
  boundary.
- `src/model/game_state.cpp:211` (the `setAnalysisData` tree-eval writeback condition, UI-03) was not
  touched — the shared reset path never needed to go near it.
- No empty-state placeholder UI added (UX-01) — this item only makes the underlying data empty.
- No change to update frequency/throttling (RT-01).

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

## Verification

- New test file `tests/test_game_state.cpp` (registered in `tests/CMakeLists.txt`, which now also
  links `src/model/game_state.cpp` and `src/model/variation_tree.cpp` into `rapfi-gui-tests`): 8 new
  `TEST_CASE`s covering `makeMove`→`undoMove`, `setAnalysisData`→`newGame` (asserting both the clear
  and that `signal_engine_analysis` fired), one case each for `loadPosition`, `redoMove`, `gotoMove`,
  `gotoPath`, an idempotency case (`makeMove`→`undoMove`→`undoMove` no-op asserts
  `signal_engine_analysis` fires 0 times when nothing changed), and a case confirming analysis data
  is NOT cleared while `setAnalyzing(true)` is set (all position-changing calls correctly return
  `false` and leave `pvLines_`/`engineStatus_` untouched).
- `cmake -S . -B build_test -G Ninja && cmake --build build_test --target rapfi-gui-tests` — built
  clean, no warnings/errors introduced.
- `./build_test/tests/rapfi-gui-tests` → `[doctest] test cases: 31 | 31 passed | 0 failed | 0
  skipped` / `[doctest] assertions: 120 | 120 passed | 0 failed` / `Status: SUCCESS!` (23 pre-existing
  + 8 new, all passing).
- `cmake --build build_test --target rapfi-gui` — full application built successfully (touches
  `src/main_window.cpp`'s edit; `src/ui/analysis_panel.cpp` was read but not modified). Only
  warnings present are pre-existing unused-function warnings in `gomocup_protocol.cpp`, unrelated to
  this change.
- `build_test/` was a scratch directory, deleted after verification.

## Testing note

Per `/CLAUDE.md` ("Bug-fix workflow"), this needs a regression test. **The repo currently has no
test infrastructure at all** — no test target in `CMakeLists.txt`, no test directory. Standing up a
minimal test target for `src/model/` (which has no GTK dependency and is the right place to test
this) is a prerequisite and should be scoped as part of this item or split into its own.

## Related

- STATE-03 (stale/empty PVLine slots inside the protocol), UX-01 (empty states), UI-03 (eval writeback)
