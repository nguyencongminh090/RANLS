# ARCH-04: characterize analyze / auto-move / ENG-02 behaviour (2026-10-06)

**Status:** Active
**Related:** ARCH-04

## Prompt

Implement ARCH-04: before ARCH-02 moves the analyze-restart / auto-move / ENG-02 revert logic out of `MainWindow`, pin the current behaviour with tests (features/extract-mainwindow-orchestration/planning.md "Characterization first").

## Root cause

Not a bug fix. **Test-only task, no behaviour change; no `src/` file touched.** No bug was exposed: every new test passed on unchanged `main` (2b44535).

## Fix

Six new doctest cases in `tests/test_anlz05_no_automove_action.cpp` (4) and `tests/test_anlz07_analyze_restart_convergence.cpp` (2), both in `ranls-gui-ui-tests`. The existing `RanlsAnlz05Probe` / `RanlsAnlz07Probe` gained read-only accessors for the `autoMoveScheduled_` / `analyzeModeScheduled_` latches; nothing retired. Coverage: (a) auto-move turn/Idle negatives new; (b) ANLZ-05 already covered; (c) burst coalescing new (auto-move + analyze); (d) force latch new; (e) Analyze-off stop without touching `enginePlays` new; (f) ENG-02 revert not persisted new (settings file byte-compared against a snapshot, with a positive control showing `onSetEnginePlays` does write it). Uncovered gap: none for the six bullets.

## Verification

Release `RUN_TESTS=1 ./build.sh`: ctest 5/5; `ranls-gui-tests` 228/2625 (unchanged); `ranls-gui-ui-tests` 60 cases/460 assertions (was 54/393). All six new cases ran individually (display present, none skipped). `git diff --stat main` shows only `tests/` and tracking files. Limitation: coalescing is asserted via the private latch because a second idle callback would bail silently on non-Idle state.

## Knowledge

`features/extract-mainwindow-orchestration/planning.md`; `docs/fix-log/2026-09-04-anlz05-analyze-mode-no-automove-mid-search-click.md`.
