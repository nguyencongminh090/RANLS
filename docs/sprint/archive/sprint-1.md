# Sprint 1 (closed 2026-08-21)

**Goal:** Fix the P0 memory-safety/data-correctness cluster surfaced by the 2026-08-21 `src/`
review, in dependency order, starting with the test harness that later items need for regression
coverage.
**Dates:** 2026-08-21 to 2026-08-21 (same-day close — all items dispatched and verified in one
working session).

## Final state — all items shipped

| Order | CODE | Summary | Status |
|---|---|---|---|
| 1 | TEST-01 | Stand up test infrastructure (header-only, model/protocol only, no display server) | ✅ DONE |
| 2a | PROTO-01 | Harden Gomocup parser: OOB `currentPVs_[-1]`, unbounded `NUMPV` resize, unvalidated DB coords | ✅ DONE |
| 2b | STATE-01 | Stale PV/status/board markers survive New Game, makeMove, undo/redo | ✅ DONE |
| 3 | RT-01 | Throttle the 6 unthrottled engine→UI emit sites | ✅ DONE |

Points were never estimated this sprint (see `docs/sprint/burndown.md` — no burndown rows were
recorded; sprint closed same-day as it opened).

## What shipped

- **TEST-01:** header-only test infrastructure for model/protocol code, no display server
  dependency — unblocked regression coverage for everything after it.
- **PROTO-01:** hardened `GomocupProtocol` parsing against OOB `currentPVs_[-1]` in `onPVDone`,
  unbounded `NUMPV`-driven resize, and unvalidated database coordinates.
- **STATE-01:** fixed stale PV/engine-status/board markers surviving New Game, `makeMove`, and
  undo/redo.
- **RT-01:** coalesced the 6 unthrottled `signal_analysis` emit sites onto a single dirty-flag +
  `Glib::signal_timeout` tick (~75ms) in `MainWindow`, with an immediate `flush()` path on search
  completion/stop so the final result is never delayed. Cached `evalHistory()` (previously
  recomputed the whole variation-tree walk on every emit). Measured: multiPV=8 over a 20-depth
  replay went from 160 raw emits to 1 coalesced UI update. See
  `docs/fix-log/2026-08-21-rt-01-throttle-analysis-signal.md` for full verification detail.
  Deliberately deviated from the instruction file's suggestion to put the timer inside
  `GameState` — it lives in `MainWindow` instead, since `tests/CMakeLists.txt` keeps
  `game_state.cpp` buildable without glibmm/gtkmm on purpose, for unit testing.

## Rolled over to Backlog

Nothing rolled over — all four committed items finished within the sprint.

## Next sprint

Sprint 2 pulls the P0 backlog items (STATE-02, ENG-01, RT-02, RT-03) into Active — see
`docs/sprint/current.md`.
