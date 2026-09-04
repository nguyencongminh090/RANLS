# Current sprint

## Sprint 10

**Goal:** Analyze Mode — continuous background analysis. When Analyze Mode is on, the engine
auto-re-analyses every position the user walks or plays into, so the WinGraph fills a real
(measured, non-NaN) point for every visited position without a manual per-position "Analyze". No
formula-derived value is ever plotted. Orthogonal to "Engine plays".

ANLZ-04 was pulled into Active mid-sprint 2026-09-04 (after ANLZ-01 shipped): where a residual NaN
ply still remains (engine not running, Analyze Mode off at the time, an interrupted search, the
engine's own turn), the WinGraph renders a faint dashed bridge across the gap instead of fragmenting
into disjoint segments — the rendering-side complement to ANLZ-01's data-side coverage.

**Dates:** 2026-09-04 to — (open — no fixed end date set yet)

**Dependency graph:**
- **ANLZ-04** — pulled into Active mid-sprint 2026-09-04 (after ANLZ-01 shipped). Pure
  `WinGraphView::onDraw` rendering change in `src/ui/win_graph_view.cpp` — a faint dashed connector
  drawn across residual NaN runs so the trace reads as one continuous line. **No** model/series/config
  change: `buildWinGraphSeries`, the `evalHistory` NaN sentinel, eval→win% maths, UI-01 attribution,
  UI-09 series colour/weight, RT-01 cadence, and the axes/labels/50 %-line/hover-box layout are all
  off-limits. Two-pass draw so `set_dash` never toggles mid-path; the bridge style must sit clearly
  below both the solid Black line and the UI-09 dashed White line (different dash pitch + lower alpha
  + thinner). Gap plies keep no dot and hover keeps "(no eval)". No `systematic-debugging` needed
  (rendering refinement, not a bug). Design settled with the user 2026-09-04: always on, no gap-length
  cap, no `ViewConfig` toggle. Requires a `docs/audit/` entry recording the UI-01 "disjoint segments,
  never interpolate through NaN" refinement, and a bridge-behaviour test (consider factoring a pure
  `computeGapBridges()` helper). See `docs/todo/ANLZ-04-wingraph-bridge-nan-gaps.md` +
  `docs/instruction/ANLZ-04-wingraph-bridge-nan-gaps.md`.
- **ANLZ-01** — original sole item this sprint (✅ shipped 2026-09-04). Pure `MainWindow` orchestration on top of existing engine
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
| ANLZ-01 | Analyze Mode — continuous background re-analysis so the WinGraph fills a measured point for every visited position (the "Lizzie way") | — (builds on UI-06 patterns, orthogonal to ENG-02, leaves UI-13 candidate A intact) | — | ✅ Done — shipped 2026-09-04 (PR #9, squash `0ae2b8a`); build clean, ctest 3/3, +2 test files; manual live-engine smoke needs a human |
| ANLZ-04 | WinGraph: faint dashed "bridge" across residual NaN runs instead of breaking the line into disjoint segments (always on, no gap cap; still no dot / "(no eval)" for gap plies) | relates to UI-01 (refines its "disjoint segments" rule — audit entry required), UI-09, UX-06; follows ANLZ-01 | — | ✅ Done — shipped 2026-09-04 (PR #10, squash `9fb7317`). `computeGapBridges()` helper + two-pass `onDraw`; orchestrator re-verified on the branch: build clean, ctest 3/3, `test_anlz04_wingraph_bridge.cpp` 8 cases / 21 assertions; audit + fix-log written. Visual smoke needs a human (no display) |

Points not yet estimated (consistent with Sprints 3–9).

**Lesson carried in from Sprint 9:** a defect/feature about what the user sees on screen needs a
real-widget test (`ranls-gui-ui-tests`, links gtkmm) — ANLZ-01's core coverage assertion is
model-layer (correct), but the toggle action's checkable state + `syncAnalyzeModeMenu()` round-trip
belongs in the gtkmm target. Also carried: run `/sprint close` the same day the last item merges —
Sprint 9 landed both items 2026-09-03 but sat unclosed until 2026-09-04, which blocked this open.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
