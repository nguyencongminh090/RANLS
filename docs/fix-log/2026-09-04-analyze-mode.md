# 2026-09-04 — ANLZ-01: Analyze Mode (continuous background analysis)

## Prompt

Implement tracked task **ANLZ-01** end-to-end on its own branch: a persisted
"Analyze Mode" toggle that, while on, restarts the engine's analysis on the new
current position after every position change so the WinGraph fills a real
(measured, never formula-derived) point for every visited ply — the "Lizzie
way". Stay orthogonal to "Engine plays" / ENG-02; reuse
`EngineController::analyze()` / `stopAnalysis()` unchanged; copy
`maybeStartAutoMove()`'s idle-coalescing structure; do not touch the WinGraph
maths / UI-01 / UI-09 / UI-13 candidate A / RT-01 / the save-game format.

## Reproduction (systematic-debugging pass, pre-change tree)

Flow: play 6 moves on an empty 15×15 board, press **Analyze** once at the end.

`EngineController::analyze()` roots the search at `gameState_.currentPath()` (all
6 plies) and `GameState::setAnalysisData()` writes `bestPv.score` onto the single
node at that path — the 6th ply's node. UI-13 candidate A would fill the child of
the search root, but with the cursor at the last ply there is no child on the
line. Result from `evalHistory()` over the 6 plies:

| ply | 0 | 1 | 2 | 3 | 4 | 5 |
|-----|---|---|---|---|---|---|
| pre-change | NaN | NaN | NaN | NaN | NaN | real |

Five of six plies render as UI-01 NaN gaps — the near-empty graph in the
original screenshot. (Encoded as the mode-off baseline case in
`tests/test_anlz01_analyze_mode_coverage.cpp`.)

With Analyze Mode ON every position the reviewer lands on becomes `currentPath()`
while a search runs, so each ply's node gets a real eval:

| ply | 0 | 1 | 2 | 3 | 4 | 5 |
|-----|---|---|---|---|---|---|
| post-change | real | real | real | real | real | real |

No derived/`1 − parent` value is ever plotted — genuinely unanalysed plies still
render as NaN gaps.

## Action

Feature — all orchestration, no new analysis path.

- **`src/model/config.h`** — `ViewConfig::analyzeMode` (`bool`, default `false`).
- **`src/model/settings_storage.cpp`** — `save()` writes `analyze_mode=`,
  `load()` reads it (`parseBool`, default off). STATE-02: every `save()` call
  site already passes all four config blocks.
- **`src/main_window.{h,cpp}`**
  - `scheduleAnalyzeModeRestart()` + `bool analyzeModeScheduled_` — a verbatim
    copy of `maybeStartAutoMove()`'s single-`Glib::signal_idle().connect_once`
    coalescing. Idle callback guards: bail unless `viewConfig().analyzeMode`
    && `engine_.isRunning()` && `engineState() == Idle`; if
    `isEnginesTurn(enginePlays, board().sideToMove())` → return (let
    `maybeStartAutoMove` play; its `signal_board_changed` reschedules us);
    else → `controller_.stopAnalysis(); controller_.analyze();` (stop before
    analyze — `analyze()` no-ops unless state == Idle, RT-01 flush on stop).
  - Wired to `gameState_.signal_board_changed` (next to `maybeStartAutoMove()`)
    and to the engine→`Idle` state transition (start-engine-after-enabling, and
    resume-after-one-shot).
  - `analyze-mode` checkable (`create_bool`) menu action in a new section of the
    "Engine plays" menu; handler uses `set_state` (not `change_state`, which
    re-emits `change-state` and would recurse).
  - `syncAnalyzeModeMenu()` mirrors `viewConfig().analyzeMode` onto the action
    (`set_state`) and the panel button — same two-way pattern as
    `syncEnginePlaysMenu()`; also called from the `signal_config_changed`
    handler and once at startup after `SettingsStorage::load()`.
  - `onToggleAnalyzeMode(bool)` — writes `ViewConfig`, persists via
    `persistGameSetup()` (all four blocks, STATE-02), syncs both surfaces, then
    `scheduleAnalyzeModeRestart()` (on) or `controller_.stopAnalysis()` (off,
    process stays up — Q7). Never touches `MatchConfig` /
    `revertEnginePlaysToOff()` (Q8 / ENG-02).
- **`src/ui/engine_status.{h,cpp}`** — a `Gtk::ToggleButton` ("∞") in the
  existing ▶/■/↻ cluster, `signal_analyze_mode_toggled(bool)` +
  `setAnalyzeModeActive(bool)` (suppressed-signal setter for the sync path).
- One-shot Analyze / Stop buttons unchanged (additive only).
- Q5 text-only `1 − parent` estimate: **deferred** (todo item marks it
  optional; not trivially in-bounds — kept out to avoid touching AnalysisPanel
  eval display). UI-13 candidate A and its test untouched.

### Tests

- `tests/test_anlz01_analyze_mode_coverage.cpp` (model, `ranls-gui-tests`):
  continuous feed (root PV at every played ply) → `evalHistory()` has no NaN;
  feed only at plies 0 & 2 → gaps remain at 1/3/4/5 (distinguishes the modes);
  `ViewConfig::analyzeMode` default-off + round-trip through `GameState`.
- `tests/test_anlz01_analyze_mode_action.cpp` (`ranls-gui-ui-tests`): real
  `MainWindow` — `analyze-mode` action exists, has boolean state type, defaults
  off, and its state round-trips through `onToggleAnalyzeMode` → `ViewConfig`
  → `syncAnalyzeModeMenu()` (on then off leaves it off).
- `tests/test_settings_storage.cpp`: `+1` case — `ViewConfig::analyzeMode`
  save/load round-trip, default off, no corruption of the other blocks.

## Verification

- `./build.sh` (fresh `build_check`): clean — only the 3 known pre-existing
  `-Wunused-function` warnings in `gomocup_protocol.cpp`, no new ones.
- `ctest` — 3/3 green:
  - `ranls-gui-tests` — Passed (includes the 3 new ANLZ-01 model cases +
    the new settings round-trip case).
  - `rel02-version-single-source` — Passed.
  - `ranls-gui-ui-tests` — Passed (includes the new toggle-action case).

## Manual smoke — NEEDS A HUMAN PASS

The build machine has no Gomoku engine binary and no interactive display, so the
live flow was not exercised. Reasoning trace for the "after" behaviour:

1. Analyze Mode ON, play 6 moves → each `makeMove` emits `signal_board_changed`
   → `scheduleAnalyzeModeRestart()` coalesces to one idle check per settled
   position → `stopAnalysis(); analyze()` roots the search at that position →
   `setAnalysisData` writes its node's eval → `evalHistory()` point per ply →
   continuous WinGraph line (verified in the model test).
2. Toggle OFF → `stopAnalysis()`, process stays up; one-shot Analyze/Stop
   behave exactly as before (no code path of theirs changed).
3. "Engine plays White" + Analyze Mode ON: on White's turn the idle callback
   sees `isEnginesTurn(...)` and bails; `maybeStartAutoMove` plays the move;
   the resulting `signal_board_changed` reschedules and the engine then ponders
   the new position. `engineState() == Idle` guard prevents a concurrent
   auto-move + ponder on one position.

Human smoke checklist: (a) 6-move play with Analyze Mode ON → continuous line,
no isolated-point gaps; (b) toggle OFF → one-shot Analyze/Stop unchanged;
(c) "Engine plays White" + Analyze Mode ON → engine moves on its turn then
ponders, no hang, no double-search.

## Caveat noted

The engine→`Idle` transition also triggers `scheduleAnalyzeModeRestart()`. A
proper ponder runs until `STOP`, so this normally only fires on our own
stop-then-analyze blip (idle callback then sees `Analyzing` and bails). An
engine that self-terminates an analyze request would be re-issued one — which is
the intended "keep pondering" behaviour, but worth watching in the human smoke
pass for CPU thrash on very fast self-terminating searches (Q4 fallback: add a
~150 ms `Glib::signal_timeout` debounce).
