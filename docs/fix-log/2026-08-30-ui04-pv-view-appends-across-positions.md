# UI-04 — PV view appends PV lines across positions instead of replacing

**Timestamp:** 2026-08-30

## Prompt

Tracked task UI-04. The Principal Variations list kept accumulating rows: with MultiPV = 1 the panel
showed several `PV #1` rows (successive depth/eval snapshots), and PV rows from earlier positions
persisted after navigating away. `PVView::update()` already diffs by row count and filters empty
PVs, so the accumulation was upstream in the `std::vector<PVLine>` feeding it.

## Action

Root cause: `GomocupProtocol::currentPVs_` (the vector emitted via `signal_analysis` and mirrored
into `GameState::pvLines_`) was only ever cleared inside `generateAnalyzeRequest()`. Nothing cleared
it on a position change, and `EngineController` forwarded every `signal_analysis` emission
unconditionally — so trailing MESSAGE/INFO lines the engine emits for a just-finished search (after
STOP or natural completion) repopulated it and, if the user had navigated to a new position
meanwhile, surfaced the previous position's PV rows on the new one.

Changes:

- `src/engine/i_engine_protocol.h` — added pure virtual `clearAnalysisState()`.
- `src/engine/gomocup_protocol.{h,cpp}` — extracted the state-clear block out of
  `generateAnalyzeRequest()` into `clearAnalysisState()` (clears `currentPVs_`, `currentStatus_`,
  `currentPVIndex_`, `currentNumPV_`, `currentBestLine_`, `resetCurrentPVState()`);
  `generateAnalyzeRequest()` now calls it.
- `src/engine/engine_controller.cpp` — `connectProtocolSignals()`:
  - the `signal_analysis` handler returns early when `!gameState_.isAnalyzing()`, so stale
    post-stop / post-completion updates are dropped;
  - new connection to `gameState_.signal_board_changed` calling `protocol_->clearAnalysisState()`,
    so move / undo / redo / New Game / load discards the protocol's held analysis for the old
    position.

Out of scope (left untouched, per the task boundaries): `PVView` rebuild/hover machinery (RT-03),
the analysis-signal throttle path (RT-01). Verified STATE-01 already clears model-side `pvLines_`
on every position change and STATE-03's `commitPV` truncation already handles same-round shrink and
per-PV-index overwrite — no changes needed there.

## Tests

New `tests/test_ui04_pv_reset.cpp` (wired into `tests/CMakeLists.txt`), 4 cases:

1. repeated `PV #1` snapshots for one position overwrite a single slot (paren-format + realtime-PV);
2. MultiPV = N keeps at most one row per PV index across successive rounds;
3. `clearAnalysisState()` drops all PV lines (simulates the board-change handler);
4. a fresh `generateAnalyzeRequest()` also clears prior-position PV lines.

`RUN_TESTS=1 ./build.sh`: build clean; ctest `rapfi-gui-tests` passes — 112 test cases /
942 assertions, 0 failed (was 108 / 927 before).
