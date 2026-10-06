# ARCH-02 — Extract analyze/auto-move orchestration out of MainWindow

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/main_window.{h,cpp}`, new `src/engine/analysis_coordinator.{h,cpp}`
**Priority:** P2
**Source:** architecture review, 2026-10-05 — [docs/audit/2026-10-05-architecture-review.md](../audit/2026-10-05-architecture-review.md)
**Design:** [features/extract-mainwindow-orchestration/](../../features/extract-mainwindow-orchestration/planning.md) — RESOLVED 2026-10-06 (design gate cleared)
**Depends on / relates to:** ARCH-04 (characterization tests first); ARCH-01 (done); split of ARCH-05 (`GameFileService`)

## Problem

`MainWindow` (1329-line cpp, ~38 methods) mixes widget construction with application logic: analyze-mode restart debounce, auto-move, engine-plays state, file load/save orchestration, graceful close. Logic inside a GTK class is hard to unit-test and violates SRP.

## Scope (in order)

*Rescoped 2026-10-06 after the design gate (decisions: `features/extract-mainwindow-orchestration/planning.md` "Resolution"). Characterization tests moved to ARCH-04; file orchestration to ARCH-05.*

1. Confirm ARCH-04 is merged (the safety net).
2. Add `AnalysisCoordinator` in `src/engine/`, GTK-free, owning: Analyze-Mode restart decision + coalescing flags (`analyzeModeScheduled_`, `analyzeModeForce_`), engine-plays auto-move decision (`autoMoveScheduled_`), and the ENG-02 manual-override revert. It takes `GameState`, `EngineController` and an injected `postIdle` scheduler.
3. `MainWindow` keeps widgets, signal wiring, the Glib idle adapter, `sync*Menu()` and persistence (`persistGameSetup`); the revert must still not persist.
4. Preserve guard order exactly (ANLZ-05, ANLZ-07; `stopAnalysis()` before `analyze()`).
5. Add coordinator unit tests using a manual scheduler (no GTK, no main loop). Existing friend-probe tests stay unchanged.

## Reasoning

Orchestration rules (when to analyze/auto-move) are application logic and should be testable without GTK; the timer stays in the UI because tests have no main loop (RT-01). Rejected: leaving it and adding more tests through `MainWindow` (needs a display); a rewrite (too risky without characterization tests).

## Knowledge

- `docs/knowledge/principle-solid-overview.md` (SRP), `docs/knowledge/style-ui-mvc-family.md`, `docs/knowledge/pattern-facade.md`
- `.claude/skills/solid-single-responsibility`, `.claude/skills/software-architecture`
- Audit: `docs/audit/2026-10-05-architecture-review.md`

## Acceptance criteria

- Behavior unchanged; `test_anlz*`, `test_eng*`, `test_rt01*` pass.
- Extracted logic is unit-tested without GTK.
- `main_window.cpp` is materially smaller.

## Scope boundary

- Not a UI redesign; no menu/shortcut changes.
- Do not retire existing friend-probe tests in this task.
- `GameFileService` is ARCH-05, not here.
