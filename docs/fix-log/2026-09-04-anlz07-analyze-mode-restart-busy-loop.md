# 2026-09-04 — ANLZ-07: Analyze Mode restart busy-loops once a search converges

## Summary

Regression against the just-shipped ANLZ-06: once ANLZ-06 correctly stopped an analysis-intent
search's terminal coordinate from being played, `scheduleAnalyzeModeRestart()` kept re-arming a
brand new `YXBOARD`+`YXNBEST` round-trip on **every** transition to Idle, with no check that the
position or the search's result had actually changed since the previous run. Once a search
converges quickly and stably (a forced mate found early, a small/solved position), this busy-looped
at native CPU speed forever: `STOP` → full board redump → search → discard → `STOP` → … — pegging a
CPU core and spamming the Engine Log for as long as Analyze Mode stayed on and the position didn't
change.

Fix (resolved with the user 2026-09-04, see `docs/instruction/ANLZ-07-*.md`): **skip-restart-if-
unchanged only** — no minimum-interval backstop.

## Root cause

`MainWindow::scheduleAnalyzeModeRestart()`'s deferred idle callback did `controller_.stopAnalysis();
controller_.analyze();` unconditionally whenever Analyze Mode was on and the engine was Idle,
including every time it was itself the cause of that Idle transition (`connectSignals()`'s
`signal_state_changed` → `Idle` handler calls it again after every search completes). Nothing
compared the just-finished search's result to the previous one, so a search that had already
converged (same best move / eval, same position) kept getting re-run from scratch as fast as the
engine could answer.

## Fix

- `EngineController` (`src/engine/engine_controller.{h,cpp}`): new private cache — `haveLastAnalysis_`,
  `lastAnalysisPath_` (keyed on `GameState::currentPath()`), `lastAnalysisResult_` (a small
  `AnalysisResult{Coord bestMove; std::string evalText;}` struct) — updated only when
  `protocol_->signal_move`'s handler sees `wasSearching && intent == SearchIntent::Analysis` (an
  analysis-intent search genuinely completed, not aborted, not a `requestEngineMove()` search). The
  result is captured via a new `captureAnalysisResult()` helper that reads `gameState_.pvLines()` /
  `gameState_.engineStatus()` — data already parsed by `signal_analysis`, no new engine query — using
  the same "pvLines()[0] if present, else raw EngineStatus" derivation `EngineStatusView::update()`
  already uses for display. New public accessor `analysisConverged()` reports whether the just-
  captured result matched the previous one for the exact same path.
- `MainWindow::scheduleAnalyzeModeRestart()` (`src/main_window.{h,cpp}`) gained a `force` parameter
  (default `false`). Its deferred idle callback now bails (`return;`, stays Idle, no restart) when
  `!force && controller_.analysisConverged()`. Two call sites pass `force=true` because they always
  have a genuine reason to restart regardless of any cached result: the `signal_board_changed`
  handler (a real position change — any cached "converged" result there is necessarily for a
  *different* position and must never suppress analysing this one) and `onToggleAnalyzeMode(true)`
  (the user explicitly asked to (re)analyse). A new `analyzeModeForce_` member latches `force` across
  coalesced calls so a later non-forced call arriving before the idle callback runs can't downgrade
  an earlier forced one.
- No minimum-interval/backoff timer, per the resolved design — a search whose result keeps changing
  between runs on the same position (genuine deepening) is not converged and must keep restarting;
  a timer would only mask that case.

## Scope discipline

- `EngineController`/`MainWindow` layering only, same as ANLZ-05/06. No protocol change — `YXNBEST`'s
  request shape and `GomocupProtocol` untouched.
- ANLZ-06's `SearchIntent` gate untouched — the convergence cache is populated only for
  `SearchIntent::Analysis` completions, so it plays no role in the `SearchIntent::Move` path.
- `requestEngineMove()` (ENG-02/UI-06 "Engine plays `<side>`" auto-move) is unaffected: its
  completions never populate the convergence cache (they're `SearchIntent::Move`, not `Analysis`),
  and its own Idle-transition handling (`maybeStartAutoMove()`) doesn't call
  `scheduleAnalyzeModeRestart()` with anything that would change behaviour — verified by re-running
  `test_anlz05_no_automove_action.cpp` and `test_anlz06_search_intent_gate.cpp` in isolation (green).
- ANLZ-01's guarantee ("every newly-visited position gets a WinGraph point") holds: `analysisConverged()`
  is false until at least two analysis-intent searches have completed back-to-back on the *same*
  position, so the first (and often only) analysis of a newly-visited position is never skipped.

## Tests

New `tests/test_anlz07_analyze_restart_convergence.cpp` (3 cases, `ranls-gui-ui-tests`) — real
`MainWindow` + a wire spy on `EngineProcess::signal_line_sent` (counting `YXNBEST` round-trips) +
direct inbound-line injection via `EngineProcess::signal_line_received.emit(...)` (the
`test_anlz06_search_intent_gate.cpp` technique), against a real (inert) `/bin/cat` "engine":

1. Two identical completed analysis results on the same position → only one continuation round-trip
   is ever sent (the busy-loop is gone) — pumps the loop for 200ms afterward and asserts no third
   `YXNBEST` appears.
2. A changed result on the same position (simulating deepening progress) → keeps restarting.
3. A real position change (`makeMove` → `signal_board_changed` → `force=true`) always restarts, even
   when the new position's first completed result happens to numerically match a *different*
   position's cached "converged" result — proves position-keying doesn't leak across positions.

`test_anlz05_no_automove_action.cpp`, `test_anlz06_search_intent_gate.cpp`, and the rest of both
suites re-verified green in the same run.

## Verification

- `./build.sh` clean: only the 3 pre-existing `-Wunused-function` warnings in
  `src/engine/gomocup_protocol.cpp` (known-OK per sprint history), no new warnings.
- `ctest --test-dir build_cmd --output-on-failure`: `ranls-gui-tests` (model/engine layer) 24 cases
  green; `rel02-version-single-source` green; `ranls-gui-ui-tests` 24 cases — 23 passed, 1 failed
  (`UI-12: appended moves keep the Move Log scrolled to the bottom`). Confirmed **pre-existing and
  unrelated**: rebuilt and re-ran the exact same test case at the pre-fix commit
  (`3aa72a9`, before any ANLZ-07 code change) and it fails identically there
  (`CHECK( 0 >= 6,987/6,993 )` — a GTK scroll-adjustment timing flake in this headless build
  environment, not touched by this fix). All 3 new ANLZ-07 cases pass in isolation
  (`--test-case="ANLZ-07*"` → 3/3, 23/23 assertions).
- Manual live-engine smoke NOT run (no engine binary / display server on the build host) — the
  reported busy-loop symptom (forced-mate position, native-speed `STOP`/`YXNBEST` cycling) is exactly
  what the regression tests reproduce and pin at the protocol-round-trip level.

## Deviation from the instruction file

None — implementation follows `docs/instruction/ANLZ-07-analyze-mode-restart-busy-loop.md`'s
resolved design (skip-restart-if-unchanged, no backstop timer) as written.
