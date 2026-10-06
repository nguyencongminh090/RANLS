# Current sprint

## Sprint 21

**Goal:** Extract MainWindow's analyze/auto-move/file orchestration into GTK-free, unit-tested classes (ARCH-02 split, characterization first)

**Dates:** 2026-10-06 to — (open — no fixed end date set yet)

**Dependency graph:**
- **ARCH-04** — `tests/` only. Pin current analyze/auto-move/ENG-02 behaviour via the existing friend probes. Must not touch `src/`. If a test exposes a bug, stop and use `systematic-debugging` + `.claude/rules/bugfix.md`. Do first.
- **ARCH-02** — new `src/engine/analysis_coordinator.{h,cpp}` + `src/main_window.{h,cpp}`. Move analyze-restart, auto-move and ENG-02 revert decisions + coalescing flags; inject `postIdle`; keep guard order (ANLZ-05/07), menu sync and persistence in `MainWindow`; do not retire existing probes. Depends on ARCH-04. Design resolved (`features/extract-mainwindow-orchestration/planning.md`, Q1 engine/, Q2 injected postIdle).
- **ARCH-05** — GTK-free `GameFileService` from `onSaveGame`/`onLoadGame`; dialogs and message text unchanged; no gtk in the service (rule 8). Relates to ARCH-02 (do after, to avoid `main_window.cpp` conflicts); layer to be picked in implementation per rule 8.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| ARCH-04 | Characterization tests for analyze / auto-move behavior | — | 2 | ✅ Done (PR #43, squash `af9c6b2`) |
| ARCH-02 | Extract `AnalysisCoordinator` out of `MainWindow` | ARCH-04 | 4 | 🔲 Not started |
| ARCH-05 | Extract GTK-free `GameFileService` | ARCH-02 (soft) | 2 | 🔲 Not started |

Points: the original single ARCH-02 estimate of 8 split 2 / 4 / 2.

**Lesson carried in from Sprint 20:** Capture the golden before refactoring — pin behaviour from the pre-change code first, so "no behaviour change" is a test result (ARCH-04 exists for exactly this). Write acceptance greps as the exact pattern, and create/verify the sprint's GitHub labels at open.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `.claude/rules/sprint-cadence.md`.
