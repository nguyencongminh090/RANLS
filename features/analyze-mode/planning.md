# Analyze Mode — planning

See [user_story.md](user_story.md) · [diagram/flow.md](diagram/flow.md).

## Open questions (resolve with user before ANLZ-01 leaves Backlog)

| # | Question | Proposed default |
|---|---|---|
| Q1 | Where does the Analyze Mode flag live + is it persisted? | `ViewConfig` bool `analyzeMode`, persisted via `SettingsStorage::save` like other UI toggles. Restored on launch but engine only ponders once running. |
| Q2 | UI surface for the toggle? | A checkable menu item under an "Engine" / "Analysis" menu **and** reuse the analysis-panel play/stop/refresh control cluster already in the screenshot (the "● ON" row). No new toolbar button. |
| Q3 | Relationship to the one-shot "Analyze" button? | Keep both. One-shot = analyse current position until Stop/move. Analyze Mode = auto re-analyse on every position change. Turning Analyze Mode on implies analysis is running. |
| Q4 | Debounce interval for the restart after a position change? | Coalesce via a single idle callback (`analyzeModeScheduled_`), same as `maybeStartAutoMove`; no timer. If it feels twitchy on rapid stepping, add a ~150 ms `Glib::signal_timeout`. |
| Q5 | Keep UI-13 candidate A (derived child plotted) or switch to Lizzie text-only estimate? | **Keep candidate A unchanged.** With Analyze Mode on the real search overwrites it; with Analyze Mode off it is still the best available. Do not regress UI-13's test. |
| Q6 | On the engine's turn under "Engine plays", ponder or wait? | Auto-move wins: `requestEngineMove()` as today; the next `signal_board_changed` reschedules and ponders the resulting position. Never ponder and auto-move the same position concurrently (`engineState() != Idle` guard already covers this). |
| Q7 | Does turning Analyze Mode off stop the engine or just stop restarting? | Stop the current search (`stopAnalysis()`), leave the process running. |
| Q8 | Interaction with ENG-02 revert? | Analyze Mode is orthogonal — it must NOT call `revertEnginePlaysToOff()`, and Stop/one-shot-Analyze keep their existing ENG-02 behaviour. Toggling Analyze Mode off does not touch `enginePlays`. |

## Implementation sequencing (once Q1–Q8 resolved)

1. `ViewConfig::analyzeMode` + persistence (Q1) — model layer, unit-testable.
2. `MainWindow::scheduleAnalyzeModeRestart()` + `analyzeModeScheduled_` idle
   coalescing; wire to `gameState_.signal_board_changed`. Guard order:
   Analyze Mode on → engine running → engine Idle → (engine's turn ? skip, leave
   to `maybeStartAutoMove` : `stopAnalysis(); analyze()`).
3. Toggle UI (Q2) + `syncAnalyzeModeMenu()`, mirroring `syncEnginePlaysMenu()`.
4. Optional text-only `1 − parent` estimate in `AnalysisPanel` (Q5 says not
   plotted) — only if the user wants the number visible; can be deferred.
5. Regression tests (see instruction file).
6. Manual smoke against the original repro screenshot.

## Follow-ups already carved out

- **ANLZ-02** — "Analyze entire game" one-shot sweep.
- **ANLZ-03** — persist per-node win% into the save-game format.
- Prior `GRAPH-xx` idea from `docs/fix-log/2026-09-03-wingraph-*.md` ("evaluate the
  whole played line") is **superseded** by ANLZ-01 + ANLZ-02.
