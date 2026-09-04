# Instruction — ANLZ-06: an Analyze-Mode search's best move must not be auto-played

Read `docs/todo/ANLZ-06-analyze-mode-search-plays-stray-move.md` first (full root-cause trace),
and the ANLZ-05 pair (`docs/todo/` + `docs/instruction/ANLZ-05-*.md`) — this closes a gap ANLZ-05
asserted but did not enforce.

`systematic-debugging` was run 2026-09-04 (transcript in the ANLZ-06 todo): Phase 1 root cause and
Phase 2 pattern are established. Root cause: `EngineController` relays **every** engine coordinate
line to `signal_engine_move` unconditionally (`engine_controller.cpp:68`), and `analyze()`'s
`YXNBEST` request culminates in the engine emitting its best move as a coordinate line
(`.../Rapfi/docs/protocol.md:383`). No fix has been written — start at Phase 3 (hypothesis +
minimal test), then Phase 4.

## The fix — `src/engine/engine_controller.{h,cpp}` only

1. **Search-intent member.** Add to `EngineController` (private):
   ```cpp
   enum class SearchIntent { None, Analysis, Move };
   SearchIntent searchIntent_ = SearchIntent::None;
   ```
   (A `bool moveRequested_` works too; the enum reads better next to `EngineState` and leaves room
   for a future third kind.)

2. **Set it at the two search entry points.**
   - `analyze()` (`~L226`): `searchIntent_ = SearchIntent::Analysis;` alongside the existing
     `setAnalyzing(true)` / `setState(Analyzing)`.
   - `requestEngineMove()` (`~L241`): `searchIntent_ = SearchIntent::Move;` alongside the same.

3. **Gate the emission in `connectProtocolSignals()`'s `protocol_->signal_move` handler**
   (`~L48-71`). Current body emits `signal_engine_move` unconditionally. New shape:
   ```cpp
   protocol_->signal_move.connect([this](Coord move) {
       const bool wasSearching = (state_ == EngineState::Analyzing);
       const SearchIntent intent = searchIntent_;
       searchIntent_ = SearchIntent::None;   // consumed

       if (wasSearching) {
           // UI-13 ordering preserved: settle the searched position's analysis
           // before the board can advance.
           gameState_.setAnalyzing(false);
           gameState_.flush();
       }

       if (intent == SearchIntent::Move)
           signal_engine_move.emit(move);
       // intent Analysis / None: the coordinate is a search-completion marker
       // only — never played. signal_state_changed(Idle) below re-enters
       // scheduleAnalyzeModeRestart() so Analyze Mode re-ponders.

       if (wasSearching)
           setState(EngineState::Idle);
   });
   ```
   Keep the existing UI-13 comment block; extend it to explain the intent gate.

4. **Reset intent on every stop path** so a trailing coordinate line for an aborted search is
   inert:
   - `stopAnalysis()` (`~L259`): `searchIntent_ = SearchIntent::None;` (unconditionally, next to
     the `sendLine(generateStop())`).
   - `stopEngine()` (`~L141`): same, next to `gameState_.setAnalyzing(false)`.
   - the `signal_process_died` lambda in the constructor (`~L16-22`): same.

## Do not

- change `generateAnalyzeRequest()` / the `YXNBEST` request shape — routing fix only.
- touch the `MainWindow` guards ANLZ-05 added, or its `signal_move_clicked` lambda. The
  `controller_.stopAnalysis()` call there now also clears the intent (step 4), which is what makes
  the click's own `makeMove()` the only move that lands.
- weaken `GameState::makeMove()`'s `analyzing_` guard.
- touch `MatchConfig` / `revertEnginePlaysToOff()` / the one-shot Analyze/Stop path / auto-move
  when `analyzeMode` is false. The `SearchIntent::Move` branch must behave byte-for-byte as today
  for ENG-02 / UI-06.

## Tests

`tests/test_anlz06_*.cpp` in `ranls-gui-tests` (model/engine layer, no gtkmm — like
`test_anlz05_stop_then_move.cpp`). The ANLZ-05 UI test only asserts on **outbound** protocol
lines; ANLZ-06 needs **inbound** coordinate lines fed to the controller. Options, pick what the
existing harness supports:
- drive `EngineProcess` over a fake engine script (`/bin/sh -c` printing canned lines) that, on
  receiving `YXNBEST`, prints a coordinate; assert no `signal_engine_move` while intent is
  Analysis;
- or feed `EngineController::onEngineLine()` / the protocol directly if a test seam exists.

Cases:
1. `analyze()` running → inbound `"7,7"` coordinate line → `signal_engine_move` **does not** fire;
   `engineState()` returns to `Idle`; the position's analysis was flushed.
2. `requestEngineMove()` running → inbound `"7,7"` → `signal_engine_move` **fires once** with
   `(7,7)`; state → `Idle`.
3. `analyze()` running → `stopAnalysis()` → then a late inbound `"7,7"` → `signal_engine_move`
   **does not** fire (intent was reset).
4. Regression for the reported double-move: `analyze()` running, `stopAnalysis()`, `makeMove(a4)`
   succeeds, then a late inbound engine coord for `b1` → board still has only the user's `a4`
   (no `b1`), move count is 1 past the pre-click position.

Keep `test_anlz05_*` green (they assert outbound behaviour, unaffected).

## Manual smoke (needs a human — no engine/display on the build host)

1. Engine running, Analyze Mode ON, mid-search: press Stop → search halts, **no stone placed**.
   Repeat for the hotkey, the analysis-panel Stop button, and toggling Analyze Mode off.
2. Analyze Mode ON, mid-search: click an empty point → **exactly one** stone (yours) lands, then
   analysis restarts. Watch for a second (engine) stone appearing a moment later — must not happen.
3. Analyze Mode ON, let a search run to its natural end (set a short turn time if needed) → the
   engine's best move is highlighted in the PV but **not** placed on the board.
4. Analyze Mode OFF, "Engine plays White", White to move → engine still auto-plays its move
   (ENG-02 / UI-06 path unchanged).
