# 2026-08-21 — Stale analysis data survives a position change (STATE-01)

## Summary

`GameState::pvLines_`/`engineStatus_` are analysis results for one specific board position, but
nothing tied their lifetime to the position's lifetime: `newGame`/`makeMove` cleared them without
emitting `signal_engine_analysis` (so `AnalysisPanel`'s PV list / engine status readout kept
showing the previous position's data), and `undoMove`/`redoMove` didn't clear them at all (so
`MainWindow`'s board kept painting the previous position's candidate-move markers after undo/redo,
because its defensive `candidateMoves.clear()` was immediately overwritten by
`BoardViewModel::update()` repopulating from the still-stale `pvLines()`).

## Fix

Added one shared private helper, `GameState::resetAnalysisState()` (`src/model/game_state.cpp`),
that clears `pvLines_`, resets `engineStatus_`, and emits `signal_engine_analysis` — idempotently
(no-op, no signal, if already empty/default) and only when not mid-search (`analyzing_ == false`).
Wired into `newGame`, `loadPosition`, `makeMove`, `undoMove`, `redoMove`, and `gotoPath`; `gotoMove`
gets it transitively via its `undoMove`/`redoMove` loop.

Removed the now-redundant `boardViewModel_.candidateMoves.clear()` in `src/main_window.cpp`'s
`signal_board_changed` handler, since `BoardViewModel::update()` (called right after) now correctly
repopulates `candidateMoves` from the already-cleared `pvLines()`. Kept `pvPreview.clear()`, which is
unrelated (set by UI hover, not by `update()`).

## Files changed

- `src/model/game_state.h` — declared `resetAnalysisState()`.
- `src/model/game_state.cpp` — implemented `resetAnalysisState()`; replaced six inline
  clear-then-forget-to-signal copies with calls to it.
- `src/main_window.cpp` — removed redundant `candidateMoves.clear()`.
- `tests/CMakeLists.txt` — added `src/model/game_state.cpp` + `src/model/variation_tree.cpp` to the
  test target's sources, and registered the new test file.
- `tests/test_game_state.cpp` (new) — 8 regression tests, one per position-changing operation plus
  an idempotency check and a mid-search-not-cleared check.

## Verification

- `cmake -S . -B build_test -G Ninja && cmake --build build_test --target rapfi-gui-tests` — clean
  build.
- `./build_test/tests/rapfi-gui-tests` → 31/31 test cases passed, 120/120 assertions passed.
- `cmake --build build_test --target rapfi-gui` — full GUI application built successfully.
- `build_test/` deleted after verification.

## Left out of scope (see detail file for why)

- `gotoPath`'s pre-existing partial-failure path (can return `false` after already mutating
  board/history) — not fixed, flagged for its own tracked item; not part of this fix's reported bug.
- `setAnalysisData`'s tree-eval writeback condition (`game_state.cpp:211`, UI-03) — untouched.
- Empty-state placeholder UI (UX-01) and update-frequency/throttling (RT-01) — untouched.

## Detail

Full task record: `docs/todo/STATE-01-stale-analysis-after-position-change.md`.
