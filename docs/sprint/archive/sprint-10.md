# Sprint 10 (closed 2026-09-04)

**Goal:** Analyze Mode — continuous background analysis. When Analyze Mode is on, the engine
auto-re-analyses every position the user walks or plays into, so the WinGraph fills a real
(measured, non-NaN) point for every visited position without a manual per-position "Analyze". No
formula-derived value is ever plotted. Orthogonal to "Engine plays".
**Dates:** 2026-09-04 to 2026-09-04.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| ANLZ-01 | Analyze Mode — continuous background re-analysis so the WinGraph fills a measured point for every visited position (the "Lizzie way") | ✅ DONE |
| ANLZ-04 | WinGraph: faint dashed "bridge" across residual NaN runs instead of breaking the line into disjoint segments | ✅ DONE |

ANLZ-01 was the sole item at sprint open (2026-09-04). ANLZ-04 was filed to Backlog and **pulled
into Active mid-sprint 2026-09-04**, after ANLZ-01 had already shipped — the rendering-side
complement to ANLZ-01's data-side coverage. Points not estimated (consistent with Sprints 3–9).
Both items landed the same day the sprint opened.

## What shipped

- **ANLZ-01** (PR #9, squash `0ae2b8a`): a persisted `ViewConfig::analyzeMode` toggle (menu action
  in the "Engine plays" menu + an "∞" `Gtk::ToggleButton` in the EngineStatus ▶/■/↻ cluster,
  `analyze_mode=` in the settings file). While on, `MainWindow::scheduleAnalyzeModeRestart()` —
  a verbatim copy of `maybeStartAutoMove()`'s single `Glib::signal_idle` coalescing — restarts the
  engine's analysis on the new `currentPath()` after every position change, so `setAnalysisData`
  writes a measured eval for each ply and `evalHistory()` has no NaN gap on visited positions.
  Guards: `analyzeMode && isRunning() && engineState()==Idle`; bails on `isEnginesTurn(...)` (lets
  `maybeStartAutoMove` play); else `stopAnalysis(); analyze()`. Reuses
  `EngineController::analyze()` / `stopAnalysis()` unchanged — no second protocol path. Strictly
  orthogonal to "Engine plays" / ENG-02 (never calls `revertEnginePlaysToOff()`, never writes
  `MatchConfig`); UI-13 candidate A and its test untouched; one-shot Analyze/Stop additive. No
  formula-derived value is ever plotted — genuinely unanalysed plies still render as NaN gaps
  (UI-01). Regression tests: `test_anlz01_analyze_mode_coverage.cpp` (3 model cases),
  `test_anlz01_analyze_mode_action.cpp` (real `MainWindow` toggle-action round-trip), +1
  `test_settings_storage.cpp` round-trip case. Q5 text-only `1 − parent` estimate deferred
  (optional, not trivially in-bounds). Manual live-engine smoke still needs a human (no engine
  binary / display on the build host) — checklist in `docs/fix-log/2026-09-04-analyze-mode.md`.

- **ANLZ-04** (PR #10, squash `9fb7317`): a pure `WinGraphView::onDraw` rendering change plus a new
  gtkmm-free helper `computeGapBridges()` (`src/ui/win_graph_bridge.h`) returning one
  `{fromIdx, toIdx}` pair per *interior* NaN run. `onDraw` gained a two-pass structure per series:
  pass 1 strokes one faint dashed connector per gap (series colour at 0.4 alpha, dash `{4,3}` —
  distinct pitch from the UI-09 White dash `{6,4}`, width `kSeriesW * 0.6`; one accumulated path,
  one `stroke()`, then `unset_dash()` so dash state never toggles mid-path), pass 2 is the
  pre-existing solid-run loop unchanged. Leading/trailing runs and `n <= 1` / all-NaN produce no
  bridge. The current-move-dot guard and hover `(no eval)` branch are untouched; nothing is written
  back to the data, no `0.5` synthesised. `buildWinGraphSeries`, the `evalHistory` NaN sentinel,
  eval→win% maths, UI-01 attribution, UI-09 colour/weight, RT-01 cadence and the axes/labels are
  all off-limits and untouched; no `ViewConfig` flag / Settings entry / gap cap. Regression test:
  `test_anlz04_wingraph_bridge.cpp` (8 cases / 21 assertions). This deliberately refines UI-01's
  "break into disjoint segments around any NaN run" rule — recorded in
  `docs/audit/2026-09-04-wingraph-nan-bridge.md`. Fix-log:
  `docs/fix-log/2026-09-04-anlz-04-wingraph-nan-bridge.md`. Visual smoke needs a human (no display).

## Lessons

- Carried from Sprint 9 and still true: **a defect/feature about what the user sees on screen needs
  a real-widget test** (`ranls-gui-ui-tests`, links gtkmm). ANLZ-01's core coverage assertion is
  correctly model-layer, but the toggle action's checkable-state round-trip went into the gtkmm
  target — the right split.
- Carried from Sprint 9: **run `/sprint close` the same day the last item merges.** Sprint 9 slipped
  this; Sprint 10 held to it — both items merged 2026-09-04 and the close ran the same day.
- New: **split a pure helper out of a Cairo/GTK draw path for testability.** ANLZ-04's
  `computeGapBridges()` (mirroring the UX-06 `buildWinGraphSeries` split) let the gap-detection
  logic get real unit coverage despite the build host having no display — the dashed styling itself
  is constant Cairo state and needs no test.
- New: **a CPU alpha-beta engine can't be pondered the Lizzie way.** ANLZ-02 ("Analyze entire game"
  sweep) and "Toggle Ponder" were both dropped this sprint — keeping a CPU engine searching every
  position continuously is too expensive. ANLZ-01's on-navigation restart + ANLZ-04's connected
  graph cover the motivating discontinuity instead. `ANLZ-02` is a retired code, not to be reused.
- Carried, still relevant: two manual smoke checks remain outstanding (no engine binary / display
  on the build host) — ANLZ-01's live-engine flow and ANLZ-04's visual bridge. Tracked in their
  fix-log files for a human pass.

## Rolled over to Backlog

Nothing rolled over — both committed items (ANLZ-01, ANLZ-04) finished. ANLZ-03 (persist per-node
win% into the save file) stays in the Backlog, where it was filed 2026-09-04; its detail +
instruction files were scaffolded at this close ahead of a Sprint 11 pull.

## Next sprint

Sprint 11 — goal: persist per-node win% into the save-game file so a reloaded game keeps its
WinGraph (ANLZ-03), the durable-storage follow-up to ANLZ-01's in-memory coverage. Run
`/sprint open 11 "<goal>" ANLZ-03` to commit it. Release `v0.2.0` cut at this close (MINOR bump —
Analyze Mode is a new user-visible feature; see `CHANGELOG.md`).
