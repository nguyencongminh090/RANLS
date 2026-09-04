# Instruction — ANLZ-01: Analyze Mode (continuous background analysis)

## Approach

This is a **feature**, not a bug fix — but do run one `systematic-debugging`
reproduction first: record exactly which plies end up NaN in the repro flow
(*play 6 moves, Analyze once at the end*) on the pre-change tree, so the "after"
smoke test has a concrete before/after.

Resolve `features/analyze-mode/planning.md` Q1–Q8 with the user before writing
code — especially Q1 (flag location/persistence), Q5 (leave UI-13 candidate A
alone), Q6/Q8 (orthogonality to "Engine plays" and ENG-02).

The whole feature is orchestration in `MainWindow`: react to
`signal_board_changed`, and when Analyze Mode is on + engine idle + not the
engine's turn, `stopAnalysis()` then `analyze()`. `EngineController::analyze()`
already builds the request for `gameState_.currentPath()` — do not reimplement it.

**Copy `MainWindow::maybeStartAutoMove()` almost verbatim** for the coalescing:
`bool analyzeModeScheduled_` + a single `Glib::signal_idle().connect_once`. The
comment there explains why (a game load replays every move synchronously and
`GameState` rejects `makeMove()` while `analyzing_` is set) — the same hazards
apply here.

## Pitfalls

- **Restart order matters.** `analyze()` early-returns unless `state_ == Idle`, so
  you must `stopAnalysis()` (which sets Idle + flushes, RT-01) *before* `analyze()`.
  Calling `analyze()` while still `Analyzing` is a silent no-op.
- **Do not fight `maybeStartAutoMove`.** On the engine's turn with "Engine plays
  <side>" set, the auto-move idle callback and the analyze-mode idle callback both
  fire. The analyze-mode one must detect `isEnginesTurn(...)` and bail, or you get
  a race between `requestEngineMove()` and `analyze()` (and possibly a `BOARD`
  command mid-search). Reuse the shared `isEnginesTurn` predicate (ENG-02).
- **`signal_board_changed` fires from the engine's own move too.** After an
  auto-move lands, board_changed fires again → analyze-mode reschedules → ponders
  the new position. That is correct, but make sure it can't recurse while the
  state transition from `signal_move` is still unwinding (the `engineState() ==
  Idle` guard in the idle callback covers it; keep it).
- **ENG-02 orthogonality.** Never call `revertEnginePlaysToOff()` from any
  analyze-mode path. Toggling Analyze Mode must not write `MatchConfig`.
- **UI-13 candidate A.** Its derived child write is guarded on
  `child->depth <= 0 && child->nodes <= 0`, so a real analyze-mode search
  overwrites it cleanly. Do not remove or "simplify" candidate A — UI-13's
  regression test (`tests/test_ui13_wingraph_eval_coverage.cpp`) pins it.
- **STATE-02 save hazard.** `SettingsStorage::save` rewrites the whole file — when
  persisting `analyzeMode`, pass `engineConfig`, `viewConfig`, `matchConfig`, and
  the `GameSetupConfig` (rule + board size), exactly like `onSetEnginePlays`.
- **Idle vs timer.** Start with pure idle-callback coalescing (no timer). Only add
  a `Glib::signal_timeout` debounce if rapid arrow-key stepping visibly thrashes
  the engine — and if so, cancel the pending timeout on each new board_changed.
- RT-01: the analysis signal still flows through `tickAnalysis()` / `flush()`.
  Don't emit `signal_engine_analysis` yourself.

## Verification before marking this task done

All tiers, run by the implementer:

1. **`./build.sh`** — clean, no new warnings (3 pre-existing `-Wunused-function` in
   `gomocup_protocol.cpp` are the known baseline).
2. **`ctest`** — all suites green: `rapfi-gui-tests` (model, gtkmm-free),
   `rapfi-gui-ui-tests` (links gtkmm), `rel02-version-single-source`.
3. **New model-layer regression test** (`tests/test_anlz01_analyze_mode_coverage.cpp`,
   wired into `rapfi-gui-tests`): drive `GameState` + a fake analysis feed the way
   a continuous re-analyse would (call `setAnalysisData` with a search root on
   *every* played position in sequence) and assert `evalHistory()` has **no NaN**
   for any visited ply. Also assert that with the feed only on plies 0 and 2 (mode
   off) the gaps remain — i.e. the test distinguishes the two modes.
4. **New UI test** (`tests/`, `rapfi-gui-ui-tests`): the Analyze Mode toggle action
   exists, is checkable, and its state round-trips through `ViewConfig` +
   `syncAnalyzeModeMenu()`.
5. **Manual smoke** (document in the fix-log/feature note): reproduce the original
   screenshot flow with Analyze Mode ON — confirm WinGraph draws a continuous line
   with a point per move; toggle OFF and confirm one-shot Analyze/Stop unchanged;
   with "Engine plays White" + Analyze Mode ON, confirm the engine moves on its
   turn and then ponders (no hang, no double-search).

Passing 1–2 alone is **not** sufficient — 3, 4 and 5 are required.

## Boundaries — do not touch

- eval→win% maths; UI-01 attribution; UI-09 SingleSide; `buildWinGraphSeries`;
  RT-01 cadence; `WinGraphView` drawing / axes / layout.
- `EngineController::analyze()` / `stopAnalysis()` internals; the protocol layer.
- UI-13 candidate A (`setAnalysisData` child-node write) and its test.
- The save-game file format (ANLZ-03).
- The one-shot Analyze/Stop buttons' existing semantics.
