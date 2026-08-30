# UI-04 — PV view appends lines across positions instead of replacing

**Status:** ✅ FIXED

Fixed 2026-08-30. Two upstream changes, no PVView/throttle refactor (RT-03/RT-01 left alone):

1. `GomocupProtocol::clearAnalysisState()` extracted from `generateAnalyzeRequest()` (which still
   calls it) and promoted to the `IEngineProtocol` interface. `EngineController` now wires it to
   `GameState::signal_board_changed`, so a move / undo / redo / New Game / load discards the
   protocol's `currentPVs_` + `currentStatus_` + per-PV state machine for the old position.
2. `EngineController`'s `signal_analysis` handler now drops updates while
   `!gameState_.isAnalyzing()` — trailing MESSAGE/INFO lines the engine emits after STOP (or
   natural completion) for the just-finished position can no longer push that position's PV rows
   onto a position the user has since navigated to.

Verified STATE-01 (`resetAnalysisState`) already clears the model-side `pvLines_` on every position
change and STATE-03's `commitPV` truncation already handles same-round shrink and per-index
overwrite — no change needed there. All commit paths (`commitPV`, `onPVDone`, `parseRealtimePV`)
already write by PV index rather than appending.

**Tests:** `tests/test_ui04_pv_reset.cpp` (4 cases, wired into `tests/CMakeLists.txt`): repeated
`PV #1` snapshots for one position collapse to a single slot; MultiPV=N keeps one slot per index
across rounds; `clearAnalysisState()` drops all lines; a fresh `generateAnalyzeRequest()` also
clears. Full suite: 112 test cases / 942 assertions passing (`RUN_TESTS=1 ./build.sh`).
**Area:** analysis panel / PV display (`src/ui/pv_view.cpp`, PV source in `src/engine/` + `src/model/`)
**Priority:** P2
**Source:** filed 2026-08-30 from a UI review session (screenshot: four "PV #1" rows with differing
depth/eval stacked while MultiPV = 1)

## Problem

The Principal Variations list keeps accumulating rows. With MultiPV = 1 the panel still shows
several `PV #1` rows, each a different depth/eval snapshot — i.e. successive analysis results (and
results from earlier positions) are appended rather than replacing the previous set.

Expected: the PV list reflects **only the current position's current analysis**. It should refresh
(replace) on every new position and on every analysis restart, and show multiple rows *only* when
MultiPV > 1 (one row per PV index, `PV #1 .. PV #N`).

## Where to look

- `PVView::update()` (`src/ui/pv_view.cpp:43`) already diffs by row count and filters empty PVs —
  the accumulation is upstream, in whatever builds the `std::vector<PVLine>` passed in.
- The PV container in the model/protocol layer (`currentPVs_` — see STATE-03, PROTO-01). Check that
  it is cleared on: new move, undo/redo, New Game (STATE-01 covered markers but verify PV vector),
  and on a fresh `analyze` for the same position.
- Whether multiple `PV #1` entries with the same `pvIndex` are being pushed instead of overwriting
  the slot for that index.

## Acceptance criteria

- MultiPV = 1 → exactly one PV row at any time, updated in place as depth increases.
- MultiPV = N → at most N rows, one per PV index.
- Changing position (move / undo / redo / New Game) clears stale PV rows immediately.

## Related

- STATE-01 (stale analysis after position change), STATE-03 (`currentPVs_` never shrinks),
  PROTO-01 (parser hardening), RT-03 (PVView rebuild vs. hover).
