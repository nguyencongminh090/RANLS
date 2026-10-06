# ARCH-04 — Characterization tests for analyze / auto-move behavior

**Status:** 🔲 OPEN (Backlog)
**Area:** `tests/` only (no `src/` change)
**Priority:** P2
**Source:** `features/extract-mainwindow-orchestration/planning.md` "Characterization first" — 2026-10-06; split from ARCH-02
**Design:** [features/extract-mainwindow-orchestration/](../../features/extract-mainwindow-orchestration/planning.md) — RESOLVED 2026-10-06
**Depends on / relates to:** blocks ARCH-02

## Problem

ARCH-02 moves the analyze-restart / auto-move / ENG-02 decision logic out of `MainWindow`. Those rules were built up across ENG-02, ANLZ-01/05/06/07 and UI-06, and several orderings are subtle. Before moving anything, current behaviour must be pinned by tests.

## Scope (in order)

1. Audit existing coverage: `test_anlz01_*`, `test_anlz05_*`, `test_anlz06_*`, `test_anlz07_*`, `test_eng01/02/03_*`, `test_rt01_throttle`.
2. Add tests, against the current `MainWindow` (via the existing friend probes), for any gap in: auto-move only on the engine's turn and Idle; auto-move suppressed while Analyze Mode is on (ANLZ-05); a burst of `signal_board_changed` causes one restart; the force-latch is not downgraded by a later non-forced call (ANLZ-07); Analyze Mode off calls `stopAnalysis()` without touching `enginePlays`; the ENG-02 revert is not persisted.
3. Report which cases were already covered and which are new.

## Scope boundary

- Tests only: no `src/` edits, no behaviour change, no new test seams in production code.
- Do not retire or rewrite existing probes.

## Reasoning

Characterize before refactoring so "no behaviour change" is a test result. Rejected: writing the coordinator tests only after extraction (they would encode the new code, not the old behaviour).

## Knowledge

- `features/extract-mainwindow-orchestration/planning.md`
- `docs/fix-log/2026-09-04-anlz05-analyze-mode-no-automove-mid-search-click.md`
- Lesson from Sprint 20: capture the golden from pre-refactor code (ARCH-03).

## Acceptance criteria

- Every bullet in Scope step 2 is covered by a passing test, new or pre-existing, and the report says which.
- `RUN_TESTS=1 ./build.sh` green; no `src/` file changed.
