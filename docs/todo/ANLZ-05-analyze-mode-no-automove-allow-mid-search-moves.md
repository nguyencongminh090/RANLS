# ANLZ-05 — Analyze Mode: never auto-move, and allow placing a stone mid-search

**Status:** 🔲 OPEN (Backlog) — filed 2026-09-04

Refinement of the shipped **ANLZ-01** (Analyze Mode). Two linked behaviour changes, both
scoped to when Analyze Mode is ON:

1. **Analyze Mode blocks auto-move entirely.** While Analyze Mode is on, the engine must
   never auto-play a move — not even on its own assigned turn under "Engine plays &lt;side&gt;".
   It only ever analyses the current position (including the engine's-turn position). This
   **reverses `features/analyze-mode/planning.md` Q6** ("Auto-move wins" on the engine's
   turn), which was the ANLZ-01 decision of record. Turning Analyze Mode off, or pressing
   Stop, just stops the search — it never triggers a move.
2. **A board click during an in-flight analysis places the stone.** Today
   `GameState::makeMove()` returns `false` whenever `analyzing_` is set, so a click on the
   board is silently swallowed while Analyze Mode's search is running. Expected: the click
   stops the current search, applies the move, and (Analyze Mode still on) the existing
   `scheduleAnalyzeModeRestart()` path re-analyses the new position.

**Source:** User report 2026-09-04 against shipped ANLZ-01, clarified same day:
> "On Analyze mode: 1. Engine do not allow to make moves. 2. User able to click during analyze."
> Q1 answer: "Analyze Mode blocks auto-move entirely. Stop just stops analyze, do not make
> move. (Only for Analyze Mode)."
> Q2 answer: "Allow placing stones mid-analysis."

**Area:**
- `src/main_window.cpp` — `maybeStartAutoMove()` (idle callback ~L1063-1077): bail when
  `gameState_.viewConfig().analyzeMode` is on. `scheduleAnalyzeModeRestart()` (idle callback
  ~L1124-1142): drop / rework the `isEnginesTurn(...)` early-return so the engine's-turn
  position is analysed too. The board-click lambda (`boardView_.signal_move_clicked`,
  ~L446-448): stop an in-flight search before `makeMove()`.
- `src/model/game_state.cpp` — `GameState::makeMove()` L86-110: the `if (analyzing_) return
  false;` guard is what blocks the click. Decide whether the guard is relaxed here or the
  caller is made responsible for calling `stopAnalysis()` first (see instruction file —
  keeping the model guard and fixing it at the MainWindow layer is the likely call, to keep
  `analyzing_` an honest invariant).
- Read-only reference: `src/engine/engine_controller.cpp` `analyze()` / `stopAnalysis()` /
  `requestEngineMove()`; `src/model/config.h` `isEnginesTurn()`.

**Priority:** P2

**Depends on / relates to:**
- **ANLZ-01** — this changes its Q6 resolution; `features/analyze-mode/planning.md` needs a
  revision note (doc-only, already added when this was filed).
- **ENG-02** — Analyze Mode stays orthogonal: do **not** call `revertEnginePlaysToOff()`,
  do not touch `MatchConfig::enginePlays`. When Analyze Mode is off, "Engine plays" auto-move
  and the ENG-02 revert behave exactly as today.
- **ANLZ-04** — with the engine now also analysing its-own-turn positions under Analyze Mode,
  fewer residual NaN plies reach the WinGraph; the dashed bridge still covers whatever remains
  (engine not running, Analyze Mode off at the time, interrupted search).

## Problem

`ANLZ-01` deliberately let the "Engine plays" auto-move path own the engine's-turn position
(planning Q6). In practice, with both Analyze Mode and "Engine plays &lt;side&gt;" on, the engine
keeps playing moves while the user is trying to study the position — Analyze Mode stops being
a pure study mode. Separately, `makeMove()`'s `analyzing_` guard means that while Analyze
Mode's background search is running (which, in Analyze Mode, is almost always), clicking the
board does nothing, with no feedback.

## Scope

1. **No auto-move under Analyze Mode.** In `maybeStartAutoMove()`'s idle callback, return
   early if `gameState_.viewConfig().analyzeMode`. Net effect: `requestEngineMove()` is never
   reached while Analyze Mode is on.
2. **Analyse the engine's-turn position too.** In `scheduleAnalyzeModeRestart()`'s idle
   callback, remove the `if (isEnginesTurn(...)) return;` bail (its only purpose was to hand
   the position to auto-move, which no longer runs). Guard order becomes: Analyze Mode on →
   engine running → engine Idle → `stopAnalysis(); analyze()`.
3. **Click during analysis places the move.** In the `signal_move_clicked` handler, if the
   controller is mid-search (`controller_.engineState() == Analyzing` /
   `controller_.isAnalyzing()`), call `controller_.stopAnalysis()` first, then
   `gameState_.makeMove(pos)`. The resulting `signal_board_changed` already reschedules
   Analyze Mode's restart for the new position.
4. **Keep `analyzing_` honest.** `stopAnalysis()` clears `analyzing_` + sets state Idle
   synchronously, so `makeMove()` runs after it with `analyzing_ == false` and its guard
   intact. Do not weaken the `GameState::makeMove()` guard itself unless the instruction file
   concludes otherwise.

## Acceptance criteria

- Analyze Mode ON + "Engine plays Black" + it is Black's turn → the engine shows analysis for
  that position and does **not** place a move. Toggling Analyze Mode off (or Stop) stops the
  search and still places no move.
- Analyze Mode OFF → "Engine plays &lt;side&gt;" auto-move and the ENG-02 revert are byte-for-byte
  unchanged.
- Analyze Mode ON, a search running → clicking an empty point places the stone, the search
  stops, and analysis restarts on the new position.
- Clicking an occupied / invalid point while analysing is still a no-op (existing `makeMove`
  validation), not a crash.
- `features/analyze-mode/planning.md` carries a dated note that Q6 is reversed by ANLZ-05.
- `./build.sh` clean; `ctest` both suites green; a regression test pins both behaviours (see
  instruction file — model-level for the guard/flow, `ranls-gui-ui-tests` for the
  MainWindow wiring, mirroring ANLZ-01's two test files).

## Scope boundary

- Only Analyze Mode changes. The one-shot Analyze/Stop path and "Engine plays" (with Analyze
  Mode off) are untouched.
- Whether a board click should also interrupt a **one-shot** (non-Analyze-Mode) analysis is
  **out of scope** — call it out separately if it comes up; this task is strictly the
  Analyze-Mode report.
- Whether undo/redo/jump during an Analyze-Mode search should likewise auto-stop-and-apply
  (they hit the same `analyzing_` guards in `undoMove`/`gotoPath`) is a related question but
  **not** part of this report — file separately if wanted.
- No new `ViewConfig` field, no Settings entry, no protocol change.
