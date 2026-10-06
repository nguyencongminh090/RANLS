# ARCH-02: extract AnalysisCoordinator from MainWindow (2026-10-06)

**Status:** Active
**Related:** ARCH-02

## Prompt

Implement ARCH-02 (design resolved in features/extract-mainwindow-orchestration/planning.md): move analyze-restart / auto-move / ENG-02 revert logic out of `MainWindow` into a GTK-free, unit-tested class.

## Root cause

Not a bug fix. **Refactor, no behaviour change.** Orchestration rules lived inside a GTK class and could only be tested through a real window (SRP; hard to unit-test).

## Fix

New `src/engine/analysis_coordinator.{h,cpp}` (registered in `CMakeLists.txt` and the `ranls-gui-tests` target). Takes `GameState&`, `EngineController&` and an injected `postIdle`; owns the idle-coalescing latches (`autoMoveScheduled_`, `analyzeModeScheduled_`, `analyzeModeForce_`) and the ENG-02 revert (never persists; fires `signal_engine_plays_reverted`). Logic moved verbatim, same guard order. `MainWindow` supplies the Glib idle adapter, re-syncs the menu on the revert signal, keeps persistence, and delegates; thin forwarders and latch const-references keep the ARCH-04 friend probes unchanged. Dangling-callback safety: shared `weak_ptr` liveness token checked at the top of each posted callback; member order guarantees the coordinator dies before `GameState`/`EngineController`. Not touched: `command/`, ARCH-05 file code, `EngineController` internals.

## Verification

New `tests/test_arch02_analysis_coordinator.cpp`: 11 cases (burst => one restart, force latch, converged skip, ANLZ-05, auto-move turn/Idle/running, Analyze off, revert without persistence, post-destruction callback no-op) in the display-free `ranls-gui-tests`. Release ctest 5/5; `ranls-gui-tests` 239/2703 (baseline 228/2625); `ranls-gui-ui-tests` 60/460 executed with a display, unchanged. `main_window.cpp` 1328 -> 1228, `main_window.h` 269 -> 244 lines. Not verified: manual GUI smoke (Analyze Mode, Engine plays, Save/Load) with a live engine.

## Knowledge

`docs/knowledge/principle-solid-overview.md`, `docs/knowledge/principle-layering-dependency-rule.md`.
