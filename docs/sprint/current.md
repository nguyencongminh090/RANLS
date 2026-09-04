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
- ANLZ-05 and ENG-03 are independent of each other, but both touch `MainWindow::connectSignals()`
  and interact with the ENG-02 auto-play-revert path — whichever lands second rebases and re-runs
  the ENG-02 regression cases.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| ANLZ-05 | Analyze Mode never auto-moves; a board click mid-search stops the search, places the stone, restarts analysis | ANLZ-01 (shipped) | — | 🔲 Not started |
| ENG-03 | Engine subprocess no longer orphaned on WM-close ("X") or GUI crash: `signal_close_request` → graceful stop + `PR_SET_PDEATHSIG` | ENG-01 (shipped) | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–11).

**Lesson carried in from Sprint 11:**

- Re-defaulting or reversing a decision that a widely-read path depends on needs a grep of every
  reader/caller first — ANLZ-05 reverses planning Q6, so every guard that currently assumes
  "auto-move wins on the engine's turn" must be found before the change.
- A design decision the user owns can arrive mid-`/implement-task`; detail files scaffolded ahead
  of a sprint are worth a sanity pass against the code they name before the sprint commits to them.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
