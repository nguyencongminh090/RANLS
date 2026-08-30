# UI-07 — PV panel still carries a stale row into the next position's analysis

**Timestamp:** 2026-08-30

## Prompt

Tracked task UI-07, dispatched via `/implement-task`. After UI-04 shipped, a user smoke test with
real Rapfi output (`INFO SHOW_DETAIL 0`, compact `MESSAGE depth N-M ev X n .. n/ms .. tm .. pv ..`
format) still showed the Principal Variations list holding one stale row from the previously
analysed position — row 1 = the 3-stone position's final line, row 2 = the current 4-stone
position's line. The UI-04 regression tests only exercised the synthetic
`MESSAGE (n) .. | .. | ..` format, which is why the gap shipped.

## Root cause

Not in the protocol layer. Two new regression tests (`tests/test_ui07_pv_cross_position.cpp`)
replay the exact two-position `MESSAGE depth` log from the task file through an unmodified
`GomocupProtocol` wired to a real `GameState` (the same isAnalyzing-gate +
`clearAnalysisState()`-on-`signal_board_changed` + `signal_move`→`makeMove` wiring
`EngineController`/`MainWindow` use). They pass against unmodified engine code:
`GomocupProtocol::commitPV`'s STATE-03 index-0 truncation keeps `currentPVs_` (and the mirrored
`GameState::pvLines_`) at size 1 with MultiPV=1, and a `pv`-less trailing status line cannot
resurrect a prior position's vector. So `pvLines()` is genuinely empty after every position change.

The defect is in the UI wiring. `AnalysisPanel::connectSignals` (`src/ui/analysis_panel.cpp`):

- the `signal_engine_analysis` handler refreshes `pvView_` and `engineStatus_`;
- the `signal_board_changed` handler refreshed the win-graph and both tree views **but never
  `pvView_` or `engineStatus_`**.

So the PV panel was only ever cleared as a side effect of `GameState::resetAnalysisState()`
emitting `signal_engine_analysis` — and that function deliberately early-returns *without emitting*
when the model side is already empty (`alreadyEmpty` guard, added by RT-01 to stop redundant
emissions during `undoAll`/`redoAll`). Whenever the view had been given a PV row for a position by
a path that then desynced from the model's final cleared state — e.g. RT-01's `flush()` on Stop
delivering the old position's last coalesced line immediately before the engine's own move changes
the position, followed by `resetAnalysisState()` finding `pvLines_` already empty — nothing
repainted `pvView_`, and its RT-03 in-place row widgets kept showing the stale row into the next
analysis.

## Fix

`src/ui/analysis_panel.cpp` — the `signal_board_changed` handler now refreshes the PV panel and the
engine-status readout straight from `gameState_`:

```cpp
pvView_.update(gameState_.pvLines(), gameState_.boardSize());
engineStatus_.update(gameState_.engineStatus(), gameState_.pvLines(), gameState_.boardSize());
```

Every position-changing `GameState` op (`makeMove`, `undoMove`, `redoMove`, `undoAll`, `redoAll`,
`gotoMove`, `gotoPath`, `newGame`, `loadPosition`) runs `resetAnalysisState()` while `!analyzing_`
and *before* emitting `signal_board_changed`, so `pvLines()` is guaranteed empty by the time this
handler runs — making the PV panel authoritative on position change, independent of whether
`resetAnalysisState()` chose to emit `signal_engine_analysis`. This mirrors how the tree and
win-graph views in the same handler already work.

`resetAnalysisState()`'s guards were left untouched (RT-01's `alreadyEmpty`, STATE-01's
`analyzing_` check both intact) — the model side is already correct; the fix is purely to stop the
view from depending on a signal the model is entitled to skip.

### Boundaries respected

- No change to the engine wire protocol or `generateAnalyzeRequest`/`generateStop`.
- `PVView::update` untouched — RT-03's in-place row reuse / hover preservation intact.
- `GomocupProtocol::commitPV` truncation (STATE-03), the isAnalyzing gate + `clearAnalysisState`
  wiring (UI-04), `resetAnalysisState` (STATE-01/RT-01) all unchanged.
- Menu bar, `MatchConfig`, settings dialog, win-graph mode logic (UI-06/UX-06) untouched.

## Tests

`tests/test_ui07_pv_cross_position.cpp` (new, wired into `tests/CMakeLists.txt`), 2 cases:

1. *compact MESSAGE-depth format, MultiPV=1 keeps exactly one row per position* — replays the real
   two-position log (3-stone analysis → Stop → engine plays `10,10` → 4-stone analysis); asserts
   the UI PV vector ends at exactly one row, starting with position 2's leading move (J4), never
   position 1's (K5), and that no position-1 coordinate leaks in.
2. *a trailing status-only line for the previous position cannot repaint stale PV rows* — after a
   position change, a fresh analysis whose first engine line carries no `pv` token must not
   resurrect the old vector.

These are permanent regression guards for the real Rapfi output format the UI-04 tests missed.

`RUN_TESTS=1 ./build.sh` — build clean, no new warnings. `ctest --test-dir build_cmd`:
`100% tests passed`. Direct run: **125 test cases / 1032 assertions, 0 failed** (was 123 / 1026).

## Verification

- `./build.sh` — clean, no new warnings (only the pre-existing `-Wunused-function` in
  `gomocup_protocol.cpp`).
- `ctest --test-dir build_cmd --output-on-failure` — 1/1 passed; `rapfi-gui-tests` 125/125.
- Live engine, partial: launched `build_cmd/rapfi-gui` against the real
  `gomoku-portal-ui-distribute/pbrain-rapfi` + `config.toml`. Confirmed the rebuilt binary drives
  the real engine and that with MultiPV=1 the PV panel shows **exactly one** row during analysis
  (screenshot `/tmp/claude-1000/.../shots/10_pv1.png`). The post-position-change "panel drops to
  zero stale rows" capture could not be completed in this environment: no input-injection tool is
  available (`ydotool`/`wtype`/`dotool` absent), the GTK4-Wayland window is opaque to `xdotool`,
  board-click moves are not exposed as GActions, and other running apps focus-steal over scripted
  KWin window raising, so a clean screenshot of the exact reproduction sequence was not obtained.
  Left **Active** with this progress note per the UI-06/UX-06 precedent — the code fix and its
  regression tests are complete.
