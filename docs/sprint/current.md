# Current sprint

## Sprint 10

**Goal:** Analyze Mode — continuous background analysis. When Analyze Mode is on, the engine
auto-re-analyses every position the user walks or plays into, so the WinGraph fills a real
(measured, non-NaN) point for every visited position without a manual per-position "Analyze". No
formula-derived value is ever plotted. Orthogonal to "Engine plays".

**Dates:** 2026-09-04 to — (open — no fixed end date set yet)

**Dependency graph:**
- **ANLZ-01** — sole item this sprint. Pure `MainWindow` orchestration on top of existing engine
  primitives; touches `src/model/config.h` (`ViewConfig::analyzeMode` flag),
  `src/model/settings_storage.cpp` (persist it — STATE-02 "save() rewrites the whole file" hazard:
  pass every config block), `src/main_window.{h,cpp}` (`signal_board_changed` wiring, new
  `scheduleAnalyzeModeRestart()` + `analyzeModeScheduled_` idle coalescing, checkable toggle action
  + `syncAnalyzeModeMenu()`). **Reuses `EngineController::analyze()` / `stopAnalysis()` unchanged** —
  no second analysis path in the protocol layer. Must stay orthogonal to "Engine plays" / ENG-02
  (never calls `revertEnginePlaysToOff()`, never writes `MatchConfig`). Must leave UI-13 candidate A
  and its regression test (`tests/test_ui13_wingraph_eval_coverage.cpp`) intact. Must not touch
  eval→win% maths, UI-01 attribution, UI-09 SingleSide, `buildWinGraphSeries`, RT-01 cadence, or
  WinGraph drawing/axes. Not `systematic-debugging` first, but the instruction file asks for one
  reproduction pass to record which plies are NaN in the repro flow before/after. Design fully
  resolved: `features/analyze-mode/planning.md` Q1–Q8 all accepted verbatim 2026-09-04 — no
  remaining "pick X with the user" calls. Copy `maybeStartAutoMove()`'s idle-coalescing structure
  verbatim; `stopAnalysis()` must precede `analyze()` or the restart no-ops.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| ANLZ-01 | Analyze Mode — continuous background re-analysis so the WinGraph fills a measured point for every visited position (the "Lizzie way") | — (builds on UI-06 patterns, orthogonal to ENG-02, leaves UI-13 candidate A intact) | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–9).

**Lesson carried in from Sprint 9:** a defect/feature about what the user sees on screen needs a
real-widget test (`ranls-gui-ui-tests`, links gtkmm) — ANLZ-01's core coverage assertion is
model-layer (correct), but the toggle action's checkable state + `syncAnalyzeModeMenu()` round-trip
belongs in the gtkmm target. Also carried: run `/sprint close` the same day the last item merges —
Sprint 9 landed both items 2026-09-03 but sat unclosed until 2026-09-04, which blocked this open.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
