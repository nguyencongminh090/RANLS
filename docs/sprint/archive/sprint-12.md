# Sprint 12 (closed 2026-09-05)

**Goal:** Post-ANLZ-01 Analyze Mode fixes plus engine-subprocess cleanup on exit
**Dates:** 2026-09-04 to 2026-09-05.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| ANLZ-05 | Analyze Mode never auto-moves; a board click mid-search stops the search, places the stone, restarts analysis | ✅ DONE |
| ANLZ-06 | Analyze-Mode search's best move must not be auto-played (Stop drops a stone; mid-search click double-moves) — `EngineController` search-intent gate | ✅ DONE |
| ANLZ-07 | Analyze-Mode restart busy-loops (STOP/redump/search/discard forever) once a search converges quickly | ✅ DONE |
| ENG-03 | Engine subprocess no longer orphaned on WM-close ("X") or GUI crash: `signal_close_request` → graceful stop + `PR_SET_PDEATHSIG` | ✅ FIXED |

Sprint opened with ANLZ-05 and ENG-03 pulled from Backlog. ANLZ-06 and ANLZ-07 were filed and
pulled in mid-sprint as regressions surfaced against ANLZ-05/ANLZ-06 respectively (same user
transcript chain), each with its own `/systematic-debugging` root-cause trace before the fix.
Points were not estimated this sprint, consistent with Sprints 3–11.

## What shipped

- **ANLZ-05** (PR #15 squash `f3bad66`): reverses `features/analyze-mode/planning.md` Q6 — while
  Analyze Mode is on, the engine never auto-moves (not even on its own turn under "Engine plays
  &lt;side&gt;"); it only analyses. A board click mid-search now stops the search, places the
  stone, and restarts analysis instead of being silently swallowed by `makeMove()`'s `analyzing_`
  guard. `MainWindow`-orchestration only; `GameState::makeMove()`'s guard and `MatchConfig`
  untouched. +`tests/test_anlz05_stop_then_move.cpp` +`tests/test_anlz05_no_automove_action.cpp`.

- **ANLZ-06** (PR #16 squash `576b25a`): root cause of the resulting "Stop drops a stone" / "mid-
  search click double-moves" regression — `EngineController` relayed **every** inbound engine
  coordinate line to `signal_engine_move` unconditionally, and `analyze()`'s `YXNBEST` search
  always ends by emitting its best move as a bare coordinate. Fix scoped to
  `src/engine/engine_controller.{h,cpp}`: a `SearchIntent` flag (`None`/`Analysis`/`Move`) set by
  `analyze()`/`requestEngineMove()`, gating `signal_engine_move` to intent `Move` only, reset on
  every stop path so a trailing coordinate from an aborted search is inert. `YXNBEST` request shape
  and the ANLZ-05 `MainWindow` guards untouched. +`tests/test_anlz06_search_intent_gate.cpp` (4
  cases) feeding **inbound** coordinate lines — closes the gap ANLZ-05's outbound-only test left.

- **ANLZ-07** (PR #17 squash `82be450`): once ANLZ-06 correctly discarded the terminal coordinate,
  `scheduleAnalyzeModeRestart()` re-armed unconditionally on every Idle transition with no check
  that the position/result had changed, busy-looping `STOP`→redump→search→discard forever once a
  search converged quickly (e.g. a forced mate). Design resolved with the user: skip-restart-if-
  unchanged only, no backstop timer. New `EngineController::analysisConverged()` compares the
  just-completed analysis-intent result to the previous completed result for the same
  `currentPath()`; `scheduleAnalyzeModeRestart()` gained a `force` parameter (default false) so the
  two genuine-restart call sites (`signal_board_changed`, `onToggleAnalyzeMode(true)`) always still
  fire. +`tests/test_anlz07_analyze_restart_convergence.cpp` (3 cases).

- **ENG-03** (PR #20 squash `b8418c7`): the engine subprocess could be orphaned on WM close ("X")
  or a GUI crash — `signal_close_request` was never wired (only menu-Quit stopped the engine), the
  heap `MainWindow` was never `delete`d so no destructor ever ran, and there was no
  `PR_SET_PDEATHSIG` so the OS didn't reap a crash-orphaned child either. Fix: `onQuit()`'s body
  factored into `MainWindow::requestGracefulClose()`, now also driving a new `signal_close_request`
  handler (vetoes the first close, `stopEngine()`, re-issues `close()` from the completion
  callback, `closeInFlight_` guards re-entrancy); `EngineProcess::start()` refactored to
  `Gio::SubprocessLauncher` + `g_subprocess_launcher_set_child_setup()` arming
  `PR_SET_PDEATHSIG(SIGKILL)` on Linux. +`tests/test_eng03_close_request.cpp`
  +`tests/test_eng03_pdeathsig.cpp`. Known gap: no Windows/macOS PDEATHSIG equivalent (documented,
  out of scope — project targets Linux/GTK4). Manual live-engine/display smoke tier not run on any
  of the four items — no engine binary or display server on the build host; explicitly documented
  as skipped rather than claimed passing in each fix-log/todo detail.

## Lessons

- Regressions against a just-merged fix (ANLZ-06 against ANLZ-05, ANLZ-07 against ANLZ-06) are best
  filed and root-caused the same day, from the same user transcript, rather than batched — the
  `/systematic-debugging` Phase 1–2 trace was already half-done from investigating the prior fix.
- "Analysis-intent vs. move-intent" is a recurring seam in `EngineController` (ANLZ-06's
  `SearchIntent` gate, ANLZ-07's convergence cache keyed off the same intent) — future engine-
  protocol work in this file should check whether it needs to key off `SearchIntent` too before
  adding a new signal/state field.
- A destructor-based cleanup guarantee (`~EngineController`/`~EngineProcess`) is not a safety net
  against every exit path — GTK's `signal_close_request` and process-level crashes both skip
  destructors entirely; ENG-03's PDEATHSIG pattern (kernel-level, not app-level) is the model for
  any future "must clean up no matter how the process ends" requirement.
- Two of four items in this sprint (ENG-03's manual smoke, and the recurring "no engine/display on
  the build host" note across all four) confirm the build host cannot verify live-engine or
  display-dependent behavior — this is now a standing, expected gap for every sprint's release
  checklist, not something to keep re-discovering.

## Rolled over to Backlog

Nothing rolled over — all four committed items finished.

## Next sprint

Sprint 13 — run `/sprint open 13 "<goal>" <CODE...>` to commit its Backlog items. Release `v0.4.0`
cut for this sprint's close (see below).
