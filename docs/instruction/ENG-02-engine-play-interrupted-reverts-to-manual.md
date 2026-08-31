# ENG-02 — execution guidance

## Approach

Small, additive transition on top of UI-06's auto-move path. Do **not** touch the auto-move
mechanics themselves (`MainWindow::maybeStartAutoMove`, `EngineController::requestEngineMove`) —
only add a "manual intervention cancels auto-play" revert.

Add one private helper on `MainWindow`, e.g. `void revertEnginePlaysToOff()`:

- If `gameState_.matchConfig().enginePlays == EnginePlaysSide::Off` → return (no-op).
- Otherwise: copy `MatchConfig`, set `enginePlays = Off`, `gameState_.setMatchConfig(mc)`, then
  `syncEnginePlaysMenu()`.
- **Do NOT call `SettingsStorage::save`** — this is the key difference from
  `onSetEnginePlays` (`src/main_window.cpp:889-901`), which persists. The revert is transient.
- No status message / toast (user: "just quiet update").

Call it from exactly two sites:

1. **`MainWindow::onStopAnalysis()`** (`src/main_window.cpp:883-886`) — call
   `revertEnginePlaysToOff()` unconditionally at the top (the helper's own Off-guard makes it a
   no-op when nothing was assigned). This covers both the toolbar button
   (`src/main_window.cpp:310`) and the Stop hotkey (`src/main_window.cpp:160`), which both route
   here. Note `analysisPanel_.engineStatus().signal_stop` (`src/main_window.cpp:600`) calls
   `controller_.stopEngine()` directly, not `onStopAnalysis` — decide with the pattern below
   whether that path also counts as "the Stop button"; the user named the toolbar Stop, so
   routing that panel signal through `onStopAnalysis()` (or adding the same call) is in scope and
   consistent. Keep it minimal.
2. **The Analyze path when it is the engine's assigned turn** — `MainWindow::onStartAnalysis()`
   (`src/main_window.cpp:863-881`) and the dispatcher `analyze` command
   (`src/command/command_dispatcher.cpp:294`, `:453`). Only revert when side-to-move matches the
   assigned side. Reuse the exact predicate from `maybeStartAutoMove`
   (`src/main_window.cpp:934-938`):
   `(plays == Black && toMove == Black) || (plays == White && toMove == White)`.
   Put that predicate in a **pure free function** so it can be unit-tested (see Testing).

## Testing

`tests/CMakeLists.txt` forbids test sources from including `src/main_window.*` /
`src/application.*` ("enforced by construction", `tests/CMakeLists.txt:1-5`), so the `MainWindow`
call sites cannot be tested directly. Therefore:

- Extract the "is it the engine's turn" check into a pure predicate — e.g.
  `bool isEnginesTurn(EnginePlaysSide plays, Stone sideToMove)` in `src/model/config.h` (next to
  `EnginePlaysSide`) or a small new header. Refactor `maybeStartAutoMove` to call it too (keeps
  one source of truth — mechanical, not a behaviour change).
- New `tests/test_eng02_revert_predicate.cpp`, added to the `rapfi-gui-tests` source list in
  `tests/CMakeLists.txt` (the non-gtkmm target, alongside `test_eng01_engine_state.cpp` at
  `:29`). Cover: Off → never engine's turn; Black assigned + Black to move → true; Black assigned
  + White to move → false; symmetric for White.
- This is a permanent regression guard — never delete it (`/CLAUDE.md` bug-fix workflow).

## Pitfalls

- **Do not persist.** Calling `SettingsStorage::save` in the revert path is the single most likely
  wrong move — acceptance criteria explicitly checks `engine_plays` in the settings file is
  unchanged.
- `syncEnginePlaysMenu()` uses `enginePlaysAction_->change_state(...)`; `onSetEnginePlays` is wired
  via `signal_activate` with an explicit `change_state` guard (`src/main_window.cpp:220-228`).
  Setting the action state programmatically must **not** re-enter `onSetEnginePlays` and re-persist
  — verify the menu sync path is state-only and does not fire the activate handler. If it does,
  set `enginePlays` in `MatchConfig` directly and only update the visible radio, without a round
  trip through the action's activate signal.
- Don't revert on a plain board move on the engine's turn — the user explicitly scoped that out.
- `onStartAnalysis` may start the engine first then analyze; the revert decision uses the board
  side-to-move, which is available regardless of engine state — compute it before the early
  returns for the "engine not running" branch so both branches revert consistently.
- The dispatcher `analyze` sites are a scripting/command path; keep the change there mechanical —
  call the same helper predicate, don't reimplement.

## Do not touch

- UI-06 auto-move mechanics (`maybeStartAutoMove` scheduling/idle-coalescing,
  `requestEngineMove`) beyond extracting the shared predicate.
- `EngineController` state enum / stop plumbing — ENG-01.
- WinGraph `enginePlays` coupling — that's UI-09.
- `SettingsStorage` serialization format.
