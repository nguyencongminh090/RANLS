# 2026-09-04 — ANLZ-06: an Analyze-Mode search's best move was auto-played on the board

## Prompt

Implement tracked task **ANLZ-06** end-to-end on its own branch: `EngineController` relayed
**every** engine coordinate line to `signal_engine_move` unconditionally, including the best-move
line `analyze()`'s `YXNBEST` search always emits on completion. This closed a gap the just-shipped
ANLZ-05 asserted but never enforced — pressing Stop mid-search, or clicking the board mid-search,
could still drop a stone that wasn't the user's.

## Root cause (from `docs/todo/ANLZ-06-analyze-mode-search-plays-stray-move.md`, systematic-debugging
Phase 1–2 already done at filing time)

`analyze()` and `requestEngineMove()` both transition through `EngineState::Analyzing` identically
and both end with the engine emitting a bare coordinate line for its search's best move (Rapfi
protocol: `YXNBEST <n>` still plays the engine's single best move, it only adds MultiPV detail to
the message stream — it is not a non-committal "analyse only" command). The
`protocol_->signal_move` handler in `EngineController::connectProtocolSignals()`
(`engine_controller.cpp:48-71`, pre-fix) had no discriminator for *why* the search was run — it
called `signal_engine_move.emit(move)` unconditionally, outside every guard. `wasSearching` only
gated the `analyzing_`/flush/`setState(Idle)` bookkeeping, never the emission itself.

Two symptoms, one root cause:
1. **Stop still made a move** — whenever a `YXNBEST` search terminated by completion/time-limit
   rather than a clean pre-output `STOP`, its terminal coordinate line was auto-played.
2. **A board click mid-search produced a double move** — the click handler's `stopAnalysis()` set
   `state_` back to `Idle` synchronously, but the interrupted search's best-move line still arrived
   asynchronously afterward and `signal_move`'s unconditional emit played it anyway, on top of the
   user's own just-played move.

## Action

Fix — `src/engine/engine_controller.{h,cpp}` only, per
`docs/instruction/ANLZ-06-analyze-mode-search-plays-stray-move.md`:

- Added a private `enum class SearchIntent { None, Analysis, Move }` member `searchIntent_`
  (`engine_controller.h`) — discriminates why the current/most-recent search was started, since
  `state_` alone cannot (both entry points reach `Analyzing` identically).
- `analyze()` sets `searchIntent_ = SearchIntent::Analysis;` alongside its existing
  `setAnalyzing(true)` / `setState(Analyzing)`.
- `requestEngineMove()` sets `searchIntent_ = SearchIntent::Move;` alongside the same.
- Rewrote the `protocol_->signal_move` handler: capture `wasSearching` and `intent`, reset
  `searchIntent_` to `None` immediately (consumed, so a second/late coordinate line can't be
  misread). The `wasSearching` UI-13 bookkeeping (clear `analyzing_`, flush the searched position's
  analysis) runs regardless of intent — a discarded Analysis-intent coordinate still settles the
  searched position's eval/PV before the board can advance, and the `Idle` transition re-enters
  `scheduleAnalyzeModeRestart()` so Analyze Mode simply re-ponders. `signal_engine_move.emit(move)`
  only fires when `intent == SearchIntent::Move`.
- Reset `searchIntent_ = SearchIntent::None;` on every stop path so a trailing coordinate line for
  an aborted search is inert even after `state_` has already moved on: `stopAnalysis()` (next to
  `sendLine(generateStop())`, unconditional), `stopEngine()` (next to `gameState_.setAnalyzing(false)`),
  and the `signal_process_died` lambda in the constructor.
- `generateAnalyzeRequest()` / the `YXNBEST` request shape, the `MainWindow` guards ANLZ-05 added,
  `GameState::makeMove()`'s `analyzing_` guard, and `MatchConfig`/`revertEnginePlaysToOff()`/the
  one-shot Analyze-Stop path/auto-move-with-Analyze-Mode-off — all untouched, as scoped.

### Tests

`tests/test_anlz06_search_intent_gate.cpp` (new, `ranls-gui-tests`, model/engine layer, no gtkmm).
The ANLZ-05 test (`test_anlz05_stop_then_move.cpp`) only ever fed outbound protocol lines through a
`/bin/cat` stand-in engine that never echoes a coordinate-shaped line back, so it could not catch
this. This suite feeds **inbound** coordinate lines by emitting directly on
`EngineProcess::signal_line_received` (a public signal — exactly what a real stdout reader would
call), same `/bin/cat` engine for lifecycle bookkeeping. Four cases, matching the instruction file:

1. `analyze()` running → inbound `"7,7"` → `signal_engine_move` does not fire; `engineState()` →
   `Idle`; `gameState_.isAnalyzing()` cleared (the analysis was flushed).
2. `requestEngineMove()` running → inbound `"7,7"` → `signal_engine_move` fires once with `(7,7)`;
   state → `Idle`.
3. `analyze()` running → `stopAnalysis()` → late inbound `"7,7"` → `signal_engine_move` does not
   fire (intent already reset).
4. Regression for the reported double-move: position `a1 a2 a3`, `analyze()` running,
   `stopAnalysis()`, `makeMove(a4)` succeeds, then a late inbound engine coordinate for `b1` →
   `signal_engine_move` does not fire and `gameState_.history().moveCount()` stays at 4 (only the
   user's `a4`, no `b1`).

All 4 new cases pass; both existing `test_anlz05_*` cases stay green (they assert outbound
behaviour only, unaffected by this change).

## Verification

- `RUN_TESTS=1 ./build.sh` (fresh `build_cmd`): clean — same 3 pre-existing `-Wunused-function`
  warnings in `gomocup_protocol.cpp` (unrelated, present before this change), no new warnings from
  `engine_controller.{h,cpp}`.
- `ctest` — 3/3 green:
  - `ranls-gui-tests` — Passed (includes the 4 new `ANLZ-06:*` cases; 34/34 assertions green when
    run in isolation via `--test-case="ANLZ-06:*"`).
  - `rel02-version-single-source` — Passed.
  - `ranls-gui-ui-tests` — Passed.
- `test_anlz05_*` re-run in isolation (`--test-case="ANLZ-05:*"`): 2/2 cases, 15/15 assertions,
  still green — unaffected by the routing change.
- Diff read back against the instruction file's exact code shape: intent captured and reset
  *before* the emit decision; `wasSearching` bookkeeping (clear `analyzing_` + flush) happens
  regardless of intent; UI-13 ordering preserved; emit gated strictly on
  `intent == SearchIntent::Move`.

## Manual smoke — NEEDS A HUMAN PASS

No engine binary / interactive display on the build host. Per the instruction file's checklist:

1. Engine running, Analyze Mode ON, mid-search: press Stop (toolbar, hotkey, panel Stop button,
   Analyze-Mode toggle off) → search halts, no stone placed.
2. Analyze Mode ON, mid-search: click an empty point → exactly one stone (the user's) lands, then
   analysis restarts — no second (engine) stone appears afterward.
3. Analyze Mode ON, let a search run to its natural end (short turn time) → the engine's best move
   is highlighted in the PV but not placed on the board.
4. Analyze Mode OFF, "Engine plays White", White to move → engine still auto-plays its move
   (ENG-02/UI-06 path unchanged).
