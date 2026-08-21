# 2026-08-21 — RT-01: throttle/coalesce the realtime analysis signal path

## Summary

`GameState::setAnalysisData` (`src/model/game_state.cpp`) emitted `signal_engine_analysis`
unconditionally, once per call — 1:1 with `GomocupProtocol::signal_analysis`, which fires from six
sites (`REALTIME BEST`, `commitPV` per PV, `Speed`/generic token lines, `parseRealtimePV`,
`onPVDone`). With `multiPV=8`, `commitPV` alone drove 8 full synchronous UI rebuilds
(`BoardViewModel::update()`, `PVView::update()`, `evalHistory()` walking the whole variation tree,
`EngineStatusView::update()`) per depth iteration.

## Fix

Followed the preferred design in `docs/instruction/RT-01-throttle-analysis-signal.md`:

- `GomocupProtocol` is untouched — still emits `signal_analysis` per line/commit, unthrottled. This
  keeps the "do not touch protocol parsing semantics" boundary and leaves other consumers
  (`EngineController`/`BottomPanel`) free to observe every line.
- `GameState::setAnalysisData` (`src/model/game_state.h`/`.cpp`) now stores the new data and sets
  `analysisDirty_ = true` instead of emitting `signal_engine_analysis` synchronously.
- `GameState::tickAnalysis()` — call periodically; if dirty, clears the flag and emits
  `signal_engine_analysis` once with the latest coalesced data. Returns `true` if it emitted.
- `GameState::flush()` — immediately emits any pending update, bypassing the tick. Called from
  `EngineController::connectProtocolSignals`'s `protocol_->signal_move` handler (search completion)
  and from `EngineController::stopAnalysis()` (analysis-stopped), so the final update of a search is
  never delayed or dropped.
- `GameState::evalHistory()` is now cached (`evalHistoryCache_`/`evalHistoryDirty_`), invalidated at
  every position-changing operation (`newGame`, `loadPosition`, `makeMove`, `undoMove`, `redoMove`,
  `gotoPath`) and whenever `setAnalysisData` updates the current tree node's eval/depth/nodes. It was
  previously walking the whole variation tree on every call, invoked from 3 separate handlers in
  `AnalysisPanel` (`signal_engine_analysis`, `signal_board_changed`, `signal_config_changed`).

### Deliberate layering deviation from the instruction file

The instruction file says the throttle mechanism (including the timer) belongs in `GameState`. It
does NOT — the actual `Glib::signal_timeout` that drives `tickAnalysis()` lives in `MainWindow`
(`src/main_window.h`/`.cpp`, `analysisTickConn_`, ~75ms interval), disconnected in `~MainWindow()`.
Reason: `tests/CMakeLists.txt` deliberately builds `src/model/game_state.cpp` +
`src/engine/gomocup_protocol.cpp` standalone, without linking glibmm/gtkmm, specifically so the
model/engine layers stay testable without a GTK main loop (see that file's header comment — "if a
future change to src/model or src/engine starts pulling in gtkmm headers transitively, this target
will fail to compile and that is the intended signal"). Putting `Glib::signal_timeout` directly in
`game_state.cpp` would have broken that guard. The instruction file itself names `src/ui/` as the
documented fallback location, which is what was used. `GameState` itself stays glibmm-free — it only
exposes the dirty-flag/coalesce mechanism (`tickAnalysis()`/`flush()`), which is what the new unit
tests exercise without any GTK main loop.

Re-entrancy check (per instruction.md pitfalls): confirmed no `signal_engine_analysis` handler
(`main_window.cpp`, `analysis_panel.cpp`) mutates `GameState` — all of them only read
(`gameState_.pvLines()`/`evalHistory()`/etc.) and redraw/rebuild UI widgets — so adding the timer
does not risk re-entrant emission.

## Verification

- **Build:** `RUN_TESTS=1 bash build.sh` — full app (`rapfi-gui`) and test binary
  (`rapfi-gui-tests`) both build clean (only pre-existing unused-function warnings in
  `gomocup_protocol.cpp`, unrelated to this change).
- **Tests:** `ctest` / direct run of `build_cmd/tests/rapfi-gui-tests` — **48/48 test cases pass,
  197/197 assertions pass** (43 pre-existing + 5 new in `tests/test_rt01_throttle.cpp`, added to
  `tests/CMakeLists.txt`).
- **Measured before/after** (from `tests/test_rt01_throttle.cpp`, replaying canned
  `INFO PV n / DEPTH / EVAL / BESTLINE / PV DONE` sequences through the real `GomocupProtocol`
  parser wired to a real `GameState`, the same pattern `test_gomocup_protocol.cpp`/
  `test_game_state.cpp` use):

  | Workload | `signal_analysis` (wire, unthrottled) | `signal_engine_analysis` before RT-01 | `signal_engine_analysis` after RT-01 (1 tick) |
  |---|---|---|---|
  | multiPV=1, 20 depth iterations | 20 | 20 (1:1, unconditional emit in old code) | **1** |
  | multiPV=8, 20 depth iterations | 160 | 160 (1:1, unconditional emit in old code) | **1** |

  The "before" column is derived directly from the removed code (`setAnalysisData` previously ended
  with an unconditional `signal_engine_analysis.emit()`, and `EngineController` maps
  `signal_analysis` 1:1 to `setAnalysisData` calls — both verified by reading the pre-patch source),
  paired with the actual measured "after" counts from the passing test run.
- **Last-update-not-dropped:** `RT-01: last update is never dropped -- burst then silence still
  delivers final state` — asserts a dirty burst followed by quiet ticks does not re-emit, and a
  second burst is still delivered by the next `tickAnalysis()` call (mirrors `flush()` being called
  by `EngineController` on real search completion/stop). Passes.

## Scope boundaries respected

- Did not touch `PVView`'s rebuild strategy (`src/ui/pv_view.cpp`) — that's RT-03.
- Did not touch `GomocupProtocol` parse functions/semantics — PROTO-01/STATE-03 territory.
- Did not fold in RT-03.
