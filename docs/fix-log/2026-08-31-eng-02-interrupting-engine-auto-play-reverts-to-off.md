# ENG-02 — interrupting engine auto-play reverts "Engine plays" to Off

**Date:** 2026-08-31
**Tracked as:** `TODO.md` ENG-02 (Sprint 7) — [todo](../todo/ENG-02-engine-play-interrupted-reverts-to-manual.md) · [instruction](../instruction/ENG-02-engine-play-interrupted-reverts-to-manual.md)

## Problem

With "Engine plays" set to Black or White (UI-06), the engine auto-moves on its turn.
If the user interrupted that arrangement — pressed Stop, or asked the engine to analyze
its own assigned turn — the setting stayed armed and the engine would fire again on the
next turn. The user wants any such manual intervention to drop back to
`EnginePlaysSide::Off` (manual analyze), transiently — the persisted side is restored on
next launch.

## Action

Small additive transition on top of UI-06's auto-move path; auto-move mechanics
(`maybeStartAutoMove` scheduling, `requestEngineMove`) untouched.

- **`src/model/config.h`** — new pure free function
  `inline bool isEnginesTurn(EnginePlaysSide plays, Stone sideToMove)` next to
  `EnginePlaysSide`: `Off` is never the engine's turn; otherwise true iff the assigned
  side equals the side to move. Single source of truth for the check.
- **`src/main_window.cpp` / `.h`**
  - `MainWindow::maybeStartAutoMove()` refactored to call `isEnginesTurn(...)` instead of
    the inline predicate (mechanical, no behaviour change).
  - New private `MainWindow::revertEnginePlaysToOff()` — no-op if `enginePlays == Off`;
    else copy `MatchConfig`, set `enginePlays = Off`, `gameState_.setMatchConfig(mc)`,
    `syncEnginePlaysMenu()`. **No `SettingsStorage::save`** (this is the key difference
    from `onSetEnginePlays`). No status message / toast.
  - Called from: `onStopAnalysis()` (toolbar Stop button + Stop hotkey), the
    analysis-panel `engineStatus().signal_stop` lambda, and `onStartAnalysis()` — the
    Analyze path only reverts when `isEnginesTurn(...)` (computed from the board
    side-to-move, before the "engine not running" early return so both branches behave
    the same).
  - `syncEnginePlaysMenu()` added to the `GameState::signal_config_changed` handler so
    the menu radio also tracks `MatchConfig` changes that don't go through the
    menu-activate handler (the dispatcher path). This is a state-only
    `Gio::SimpleAction::change_state` — it does not emit `signal_activate`, so
    `onSetEnginePlays` does not re-enter or re-persist.
- **`src/command/command_dispatcher.{h,cpp}`** — new private
  `revertEnginePlaysIfEnginesTurn()` (in-memory `setMatchConfig` only, no persistence),
  called before `controller_.analyze()` at both dispatcher sites: the `analyze` command
  and `!play`.
- **`tests/test_eng02_revert_predicate.cpp`** (new, added to the `rapfi-gui-tests`
  non-gtkmm source list) — 3 cases / 9 assertions on `isEnginesTurn`: Off → always false;
  Black assigned + Black to move → true, + White to move → false; symmetric for White.
  The `MainWindow` / dispatcher call sites can't be unit-tested (test sources may not
  include `src/main_window.*`), so the extracted predicate is the regression guard.

Out of scope (per the todo "Do not touch"): a plain board move on the engine's turn does
not revert; UI-06 auto-move mechanics, `EngineController` state/stop plumbing (ENG-01),
WinGraph `enginePlays` coupling (UI-09), and `SettingsStorage` serialization are all
untouched.

## Verification

- `./build.sh` — clean, no errors or new warnings.
- `RUN_TESTS=1 ./build.sh` — 2/2 ctest suites pass (`rapfi-gui-tests`,
  `rapfi-gui-ui-tests`), including the new `ENG-02` predicate cases.
- Manual trace of the acceptance criteria: after Stop / Analyze-on-engine's-turn,
  `enginePlays == Off` and the menu radio reflects it; `maybeStartAutoMove` then returns
  early so the engine does not auto-move again; `SettingsStorage::save` is not reachable
  from either revert path (its only call sites are `persistGameSetup`, `onSettings`,
  `onSetEnginePlays`), so the settings file's `engine_plays` value is unchanged;
  one-shot Analyze/Stop still work in the reverted state (the helper is a no-op when
  already Off).

## Related

- UI-06 (`MatchConfig` / "Engine plays" menu + auto-move path this builds on).
- ENG-01 (engine lifecycle/state — not touched here).
- UI-09 (removes UX-06's WinGraph `enginePlays` coupling — separate).
