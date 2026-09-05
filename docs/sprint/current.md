# Current sprint

## Sprint 12

**Goal:** Post-ANLZ-01 Analyze Mode fixes plus engine-subprocess cleanup on exit

**Dates:** 2026-09-04 to — (open — no fixed end date set yet)

**Dependency graph:**

- **ANLZ-05** — `MainWindow` orchestration only (same layer as ANLZ-01); no new model API, no
  protocol path. Three small edits: `maybeStartAutoMove()` gains an `analyzeMode` guard,
  `scheduleAnalyzeModeRestart()` drops its engine's-turn early-return, and the board-click handler
  stops an in-flight search before `makeMove()`. **Reverses `features/analyze-mode/planning.md` Q6.**
  Must not weaken `GameState::makeMove()`'s `analyzing_` guard, must not write `MatchConfig` or touch
  the one-shot Analyze/Stop path (ENG-02 orthogonality). No `systematic-debugging` gate — known
  cause, guidance in `docs/instruction/ANLZ-05-*.md`.
- **ENG-03** — `src/engine/engine_process.{h,cpp}` + `src/main_window.cpp` (`connectSignals`,
  `onQuit`), possibly `src/application.cpp`. Two independent changes: wire `signal_close_request` →
  graceful stop (factor `onQuit()`'s body into a shared `requestGracefulClose()` helper, guard
  re-entry with a `closeInFlight_` flag), and add `PR_SET_PDEATHSIG` via `Gio::SubprocessLauncher`
  child-setup (`#ifdef __linux__`). Builds on ENG-01's `stop()`/`stopAsync()` split; must not
  regress ENG-02. No `systematic-debugging` gate — the lifecycle gap is already traced in
  `docs/todo/ENG-03-*.md`.
- **ANLZ-06** — `src/engine/engine_controller.{h,cpp}` only. Regression found against the merged
  ANLZ-05: `analyze()`'s `YXNBEST` search ends by emitting a bestmove coordinate, which
  `EngineController` relays to `signal_engine_move` unconditionally (no analysis-vs-move intent) —
  so Stop drops a stone and a mid-search click double-moves. Add a `SearchIntent` flag, gate the
  emission, reset on every stop path. `/systematic-debugging` Phase 1–2 done (in the todo file).
  Don't touch the `YXNBEST` request, the ANLZ-05 `MainWindow` guards, or ENG-02 / UI-06.
- ANLZ-05 and ENG-03 are independent of each other, but both touch `MainWindow::connectSignals()`
  and interact with the ENG-02 auto-play-revert path — whichever lands second rebases and re-runs
  the ENG-02 regression cases. ANLZ-06 is downstream of ANLZ-05 (fixes a gap it left) and touches
  `EngineController`, not `MainWindow` — independent of ENG-03.
- **ANLZ-07** — downstream of ANLZ-06 (same user transcript, next symptom): once the ANLZ-06 fix
  correctly discards the analysis-intent coordinate, `scheduleAnalyzeModeRestart()`
  (`main_window.cpp:1129`) re-arms unconditionally on every Idle transition with no check that the
  position/result changed, so a fast-converging search (a forced mate in the report) busy-loops
  `STOP`→redump→search→discard forever. **Design resolved with the user 2026-09-04:
  skip-restart-if-unchanged only** (compare the completed result — best move + eval — to the
  previous completed result on the same position; deliberately no minimum-interval backstop — a
  still-changing result means still-converging, not a bug). `MainWindow`/`EngineController` only,
  no protocol change; must not regress ANLZ-01's "every visited position gets a WinGraph point"
  guarantee or ENG-02/UI-06 (`requestEngineMove()` doesn't self-restart, so it's already
  unaffected — verify that stays true). `/systematic-debugging` Phase 1–2 already done (in the todo
  file) — ready for `/implement-task ANLZ-07`.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| ANLZ-05 | Analyze Mode never auto-moves; a board click mid-search stops the search, places the stone, restarts analysis | ANLZ-01 (shipped) | — | ✅ Done — PR #15 squash-merged to `main` (`f3bad66`) 2026-09-04 |
| ENG-03 | Engine subprocess no longer orphaned on WM-close ("X") or GUI crash: `signal_close_request` → graceful stop + `PR_SET_PDEATHSIG` | ENG-01 (shipped) | — | ✅ Done — PR #20 squash-merged to `main` (`b8418c7`) 2026-09-05 |
| ANLZ-06 | Analyze-Mode search's best move must not be auto-played (Stop drops a stone; mid-search click double-moves) — `EngineController` search-intent gate | ANLZ-05 (merged) | — | ✅ Done — PR #16 squash-merged to `main` (`576b25a`) 2026-09-04 |
| ANLZ-07 | Analyze-Mode restart busy-loops (STOP/redump/search/discard forever) once a search converges quickly | ANLZ-06 (merged) | — | ✅ Done — PR #17 squash-merged to `main` (`82be450`) 2026-09-05 |

Points not yet estimated (consistent with Sprints 3–11).

**Lesson carried in from Sprint 11:**

- Re-defaulting or reversing a decision that a widely-read path depends on needs a grep of every
  reader/caller first — ANLZ-05 reverses planning Q6, so every guard that currently assumes
  "auto-move wins on the engine's turn" must be found before the change.
- A design decision the user owns can arrive mid-`/implement-task`; detail files scaffolded ahead
  of a sprint are worth a sanity pass against the code they name before the sprint commits to them.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
