# ARCH-05: extract GameFileService from MainWindow (2026-10-06)

**Status:** Active
**Related:** ARCH-05

## Prompt

Implement tracked task ARCH-05: move the GTK-free statements of `onSaveGame` / `onLoadGame` into a display-free service so the RDB-03 meta logic and load/save error paths are unit-testable.

## Root cause

Not a bug fix -- refactor. The meta/graph building, `.rdb` default extension, writer lookup and load/apply logic lived inside GTK dialog callbacks and had no direct test.

## Fix

New `src/model/game_file_service.{h,cpp}` (`GameFileService::save` / `load`, returning `{ok, error}`); wired into the app, `ranls-gui-tests` and `ranls-gui-ui-tests` source lists. `MainWindow::onLoadGame` / `onSaveGame` now only do dialogs, discard confirmation, error dialogs and `controller_.sendConfig()`. No behaviour change; dialog texts byte-identical. Left alone: RDB codec, `GameIO`, `AnalysisCoordinator`, `EngineController`, filters, file format.

## Verification

New `tests/test_arch05_game_file_service.cpp` (+7 cases: engine entry present/absent, no-extension -> `.rdb`, non-`.rdb` -> exact error and no file, write failure, load round-trip, garbage/missing load leaves state unchanged). Release `RUN_TESTS=1 ./build.sh`: ctest 5/5; `ranls-gui-tests` 246/2760; `ranls-gui-ui-tests` 60/460, none skipped. Not verified: interactive Save/Load dialogs (human smoke step).

## Knowledge

`features/extract-mainwindow-orchestration/planning.md` (Q5), `docs/knowledge/pattern-facade.md`, `docs/knowledge/principle-layering-dependency-rule.md`.
