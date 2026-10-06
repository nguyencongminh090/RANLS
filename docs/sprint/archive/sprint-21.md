# Sprint 21 (closed 2026-10-06)

**Goal:** Extract MainWindow's analyze/auto-move/file orchestration into GTK-free, unit-tested classes (ARCH-02 split, characterization first)
**Dates:** 2026-10-06 to 2026-10-06.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| ARCH-04 | Characterization tests for analyze / auto-move behavior | ✅ DONE |
| ARCH-02 | Extract `AnalysisCoordinator` out of `MainWindow` | ✅ DONE |
| ARCH-05 | Extract GTK-free `GameFileService` | ✅ DONE |

Sprint 21 was opened mid-flight on 2026-10-06 after the ARCH-02 design gate was resolved with the user (`features/extract-mainwindow-orchestration/planning.md`): the original single 8-pt ARCH-02 was split into ARCH-04 (2) / ARCH-02 (4) / ARCH-05 (2), the new CODEs being filed straight into Active. Points came from the original estimate.

## What shipped

- **ARCH-04** (PR #43 squash `af9c6b2`): tests only, no `src/` change. Six new cases in `test_anlz05_no_automove_action.cpp` / `test_anlz07_analyze_restart_convergence.cpp` pin auto-move gating (engine's turn + Idle), one-restart/one-check coalescing, the ANLZ-07 force latch, Analyze-Mode-off leaving `enginePlays` alone, and the ENG-02 revert not writing the settings file. The existing friend probes gained read-only latch accessors; none retired. All passed on unchanged `main` — no bug exposed. `ranls-gui-ui-tests` 54 → 60 cases.
- **ARCH-02** (PR #44 squash `86c20a3`): new GTK-free `src/engine/analysis_coordinator.{h,cpp}` owning the Analyze-Mode restart decision + latches, the auto-move decision and the ENG-02 revert (never persists; fires `signal_engine_plays_reverted`). Logic moved verbatim with the same guard order; the idle scheduler is injected (`postIdle`) and each callback holds a `weak_ptr` liveness token. `MainWindow` keeps widgets, the Glib adapter, menu sync and persistence; thin forwarders and `const bool&` latch views kept the ARCH-04 probes byte-identical. `main_window.cpp` 1328 → 1228. 11 new display-free tests (`ranls-gui-tests` 228 → 239 cases).
- **ARCH-05** (PR #45 squash `a287fe0`): new GTK-free `src/model/game_file_service.{h,cpp}` (`save`/`load` returning `{ok, error}`) holding the default `.rdb` extension, writer lookup, RDB-03 engine-entry meta, graph build/write and load+apply; `onSaveGame`/`onLoadGame` reduced to dialog glue with byte-identical messages. `main_window.cpp` 1228 → 1191. 7 new display-free tests (239 → 246 cases).

## Lessons

- **Characterize first pays off.** ARCH-04's tests passed unchanged against the post-extraction code, so ARCH-02's "no behaviour change" was a test result. Keep the pattern: split a risky refactor into a tests-only task, then the move.
- **Keep probe-facing names stable during a move.** Leaving thin forwarders and `const bool&` views in `MainWindow` let the existing friend probes stay untouched; retiring or rewriting them is a separate follow-up.
- **A test-only task can still need test-file edits to existing probes** (read-only accessors). State that explicitly in the acceptance criteria next time ("no `src/` change", not "no existing test edit").
- **Live-engine / interactive checks are still human-owed.** Carried forward: Analyze Mode, Engine plays and Save/Load dialogs were not exercised by hand on this host.

## Rolled over to Backlog

Nothing rolled over — all committed items finished.

## Next sprint

Sprint 22 — not yet opened; the Backlog is empty. Possible follow-ups, none filed: retire friend probes made redundant by the coordinator tests (planning Q8), and the human smoke of Analyze Mode / Engine plays / Save-Load. Release `v0.8.2` cut at close (see `CHANGELOG.md`).
