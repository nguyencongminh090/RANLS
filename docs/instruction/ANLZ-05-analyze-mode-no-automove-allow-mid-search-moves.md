# Instruction — ANLZ-05: Analyze Mode never auto-moves, and a click mid-search plays

Read `docs/todo/ANLZ-05-analyze-mode-no-automove-allow-mid-search-moves.md` first, and
`features/analyze-mode/planning.md` (this task reverses its Q6).

Pure `MainWindow` orchestration again — same layer as ANLZ-01, no new model API, no protocol
path. Three small edits:

1. **`MainWindow::maybeStartAutoMove()`** — in the `Glib::signal_idle().connect_once` body,
   add `if (gameState_.viewConfig().analyzeMode) return;` alongside the existing
   `enginePlays == Off` / `!isRunning()` / `!= Idle` / `!isEnginesTurn` guards. Leave the
   outer `autoMoveScheduled_` coalescing alone.

2. **`MainWindow::scheduleAnalyzeModeRestart()`** — delete the
   `if (isEnginesTurn(gameState_.matchConfig().enginePlays, gameState_.board().sideToMove()))
   return;` block and its comment. The remaining body is: `analyzeMode` on → `isRunning()` →
   `engineState() == Idle` → `stopAnalysis(); analyze()`. (Keep the `stopAnalysis()` before
   `analyze()` — `analyze()` early-returns unless state is Idle.)

3. **Board-click handler** (`boardView_.signal_move_clicked` lambda, `connectSignals()`):
   ```cpp
   boardView_.signal_move_clicked.connect([this](Coord pos) {
       if (controller_.isAnalyzing())        // mid-search: stop, then place
           controller_.stopAnalysis();       // clears analyzing_ + →Idle synchronously
       gameState_.makeMove(pos);
   });
   ```
   `stopAnalysis()` is safe to call unconditionally when the engine is usable, but gate on
   `isAnalyzing()` so a click with no engine / no search stays a plain `makeMove()`. After
   `stopAnalysis()` returns, `analyzing_` is false so `makeMove()`'s guard passes; the
   `signal_board_changed` it emits re-enters `scheduleAnalyzeModeRestart()` for the new
   position.

**Do not**:
- weaken `GameState::makeMove()`'s `if (analyzing_) return false;` — keep the model invariant;
  the fix is that the caller now stops the search first.
- call `revertEnginePlaysToOff()` or write `MatchConfig` (ENG-02 orthogonality, planning Q8).
- change the one-shot Analyze/Stop path or auto-move behaviour when `analyzeMode` is false.
- add a `ViewConfig` field or Settings row.

## Tests (mirror ANLZ-01's two files)

- `tests/test_anlz05_*.cpp` in `ranls-gui-tests` (model / no gtkmm):
  - with `analyzing_` set, `makeMove()` still returns false (guard unchanged);
  - after `EngineController::stopAnalysis()`, `makeMove()` succeeds — proves the
    stop-then-move sequence the click handler relies on.
- `tests/test_anlz05_*_action.cpp` in `ranls-gui-ui-tests` (real `MainWindow`, links gtkmm,
  like `test_anlz01_analyze_mode_action.cpp`):
  - Analyze Mode on + `enginePlays` = the side to move + engine Idle → driving the idle
    callbacks does **not** call `requestEngineMove()` (no `signal_engine_move`), and Analyze
    Mode's `analyze()` restart still fires for that position;
  - Analyze Mode off + same setup → auto-move path still fires (no regression).
- Keep `test_anlz01_analyze_mode_coverage.cpp` / `_action.cpp` green — where they assert the
  Q6 "engine's turn is skipped by analyze-mode restart" behaviour, update those assertions to
  the new "analysed regardless of side" expectation and note the ANLZ-05 reversal in the test
  comment.

## Manual smoke (needs a human — no engine/display on the build host)

1. Engine running, Analyze Mode ON, "Engine plays Black", empty board (Black to move):
   analysis appears, no stone is auto-placed. Wait ~10 s — still no move.
2. Press Stop → search halts, no move placed. Toggle Analyze Mode off → same.
3. Analyze Mode ON, mid-search: click an empty point → stone lands, search restarts on the
   new position. Click an occupied point → nothing happens, no crash.
4. Analyze Mode OFF, "Engine plays White", White to move → engine still auto-plays (ENG-02
   path unchanged).
