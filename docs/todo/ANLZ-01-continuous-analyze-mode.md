# ANLZ-01 — Analyze Mode: continuous background analysis so WinGraph fills for every visited position

**Status:** 🔲 OPEN (Backlog) — **gated on `features/analyze-mode/planning.md` Q1–Q8
being resolved with the user before this leaves Backlog / enters a sprint.**

**Area:** `src/model/config.h` (`ViewConfig` — new `analyzeMode` flag),
`src/model/settings_storage.cpp` (persist it), `src/main_window.cpp`
(`signal_board_changed` wiring, new `scheduleAnalyzeModeRestart()` +
`analyzeModeScheduled_`, toggle action + `syncAnalyzeModeMenu()`),
`src/engine/engine_controller.cpp` (reuse `analyze()` / `stopAnalysis()` as-is).
Read-only reference: `src/model/game_state.cpp` `setAnalysisData` / `evalHistory`,
`src/ui/win_graph_view.cpp`.
**Priority:** P2
**Source:** User decision 2026-09-04 after the WinGraph-coverage discussion
(`docs/notes/2026-09-04-wingraph-analyze-mode-and-backfill.md`). Supersedes the
`GRAPH-xx` "evaluate the whole played line" idea noted in
`docs/fix-log/2026-09-03-wingraph-record-eval-regardless-of-side.md`.
**Design:** `features/analyze-mode/` (`user_story.md`, `diagram/flow.md`,
`planning.md`).
**Depends on / relates to:** UI-06 (`MatchConfig::enginePlays`, `maybeStartAutoMove`
idle-coalescing pattern to copy), ENG-02 (must stay orthogonal — no
`enginePlays → Off` revert), UI-13 (candidate A stays unchanged), RT-01 (restart
must not bypass the throttle), UI-01 (NaN gap semantics preserved).

## Problem

WinGraph is near-empty in the flow *user plays several moves → Analyze once → user
plays several more → Analyze once*: `setAnalysisData` writes an eval only onto the
`currentPath()` node during a search, so every ply walked past without a search on
that exact position stays a NaN gap. Formula backfill (`1 − parent`) is rejected —
no mature analysis GUI plots a derived point, and it destroys the move-to-move Δ
that makes a blunder visible (Lizzie computes `1 − parent` but stores
`playouts = 0` so `WinrateGraph` never plots it).

## Scope (in order — after planning Q1–Q8)

1. **`ViewConfig::analyzeMode`** (bool) + save/restore in `SettingsStorage`
   (STATE-02 "save() rewrites the whole file" hazard — pass every config block).
2. **`MainWindow::scheduleAnalyzeModeRestart()`** + `bool analyzeModeScheduled_`,
   connected to `gameState_.signal_board_changed`. Coalesce a burst (game load,
   undoAll/redoAll) into one `Glib::signal_idle().connect_once` check — copy the
   structure of `maybeStartAutoMove()` verbatim. Inside the idle callback:
   - bail unless `viewConfig().analyzeMode`, `engine_.isRunning()`,
     `controller_.engineState() == Idle`;
   - if `isEnginesTurn(enginePlays, board().sideToMove())` → do nothing (let
     `maybeStartAutoMove` play the move; the resulting `signal_board_changed`
     reschedules this);
   - else → `controller_.stopAnalysis(); controller_.analyze();` (analyse the new
     `currentPath()` as root).
3. **Toggle UI** — a checkable action (menu + the analysis-panel ON/▶/■ cluster),
   `syncAnalyzeModeMenu()` mirroring `syncEnginePlaysMenu()`. Turning it on while
   the engine is running kicks an immediate `analyze()`; turning it off calls
   `stopAnalysis()` (process stays up).
4. Keep one-shot **Analyze / Stop** buttons working exactly as now (additive).
5. (Optional, per Q5) a **text-only** `1 − parent` estimate in `AnalysisPanel` —
   never a graph point.
6. Regression + UI tests (see instruction file).
7. Manual smoke against the original repro screenshot: play 6 moves with Analyze
   Mode on → WinGraph has a continuous line, no isolated-point gaps.

## Acceptance criteria

- With Analyze Mode on, walking/playing through a line yields a real (non-NaN)
  WinGraph point for **every position the engine settled on**, with no manual
  per-position "Analyze".
- A weak / non-candidate user move still produces a truthful point for the
  resulting position (measured, not estimated) — the win% drop is visible.
- Analyze Mode off → behaviour identical to today (one-shot Analyze/Stop).
- No formula-derived value is ever plotted on the graph line. Genuinely
  unanalysed plies still render as NaN gaps (UI-01). UI-13 candidate A's derived
  reply-ply point is unchanged.
- "Engine plays <side>" + Analyze Mode: engine auto-moves on its turn, then
  ponders; no concurrent auto-move + ponder on one position; ENG-02 revert
  behaviour unchanged; toggling Analyze Mode never changes `enginePlays`.
- Game load / undoAll / redoAll fire **one** coalesced restart, not one per ply.
- `./build.sh` clean; `ctest` both suites green.

## Scope boundary

- Do not change eval→win% maths, UI-01 attribution, UI-09 SingleSide,
  `buildWinGraphSeries` perspective, RT-01 cadence, or WinGraph drawing/axes.
- Do not modify `EngineController::analyze()` / `stopAnalysis()` internals — reuse
  them. Do not add a second analysis code path in the protocol layer.
- Do not touch the save-game file format (that's ANLZ-03).
- "Analyze entire game" one-shot sweep is ANLZ-02, not this task.
