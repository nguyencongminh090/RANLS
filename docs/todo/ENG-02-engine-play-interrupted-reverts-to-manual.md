# ENG-02 — Interrupting engine auto-play reverts "Engine plays" to Off (Manual Analyze)

**Status:** ✅ DONE (Sprint 7, 2026-08-31) — design resolved with user 2026-08-31, dispatched

Implemented as a small additive transition on top of UI-06's auto-move path (auto-move
mechanics untouched):

- New pure predicate `isEnginesTurn(EnginePlaysSide, Stone)` in `src/model/config.h` — single
  source of truth for the "engine's turn" check; `MainWindow::maybeStartAutoMove` refactored to
  use it (mechanical, no behaviour change).
- New private helper `MainWindow::revertEnginePlaysToOff()` — no-op when `enginePlays == Off`;
  otherwise sets `MatchConfig::enginePlays = Off`, `gameState_.setMatchConfig(mc)`, and
  `syncEnginePlaysMenu()`. **Does NOT call `SettingsStorage::save`** (transient session revert).
  No status message / toast.
- Trigger points: `MainWindow::onStopAnalysis()` (toolbar Stop + Stop hotkey), the
  analysis-panel `signal_stop` lambda, and `MainWindow::onStartAnalysis()` /
  `CommandDispatcher` `analyze` + `!play` sites — the Analyze paths only revert when
  `isEnginesTurn(...)` is true (side-to-move == the engine's assigned side).
- The dispatcher path reverts `MatchConfig` in-memory only via
  `CommandDispatcher::revertEnginePlaysIfEnginesTurn()`; `MainWindow` re-syncs the menu radio
  from a new `syncEnginePlaysMenu()` call added to the `GameState::signal_config_changed`
  handler (state-only `change_state`, does not re-enter `onSetEnginePlays`).
- Plain board moves on the engine's turn deliberately do NOT revert (out of scope).

**Verification (2026-08-31):**
- `./build.sh` — clean, no errors/warnings.
- `RUN_TESTS=1 ./build.sh` — `rapfi-gui-tests` and `rapfi-gui-ui-tests` both pass (2/2 ctest).
- New `tests/test_eng02_revert_predicate.cpp` (3 cases / 9 assertions): Off → always false;
  Black assigned + Black to move → true, + White to move → false; symmetric for White. Added
  to the `rapfi-gui-tests` (non-gtkmm) source list.
- Manual trace: `SettingsStorage::save` is not reachable from either revert path (call sites
  are `persistGameSetup`/`onSettings`/`onSetEnginePlays` only); `setMatchConfig` →
  `signal_config_changed` → `syncEnginePlaysMenu` uses `Gio::SimpleAction::change_state`, which
  does not emit `signal_activate`, so `onSetEnginePlays` does not re-enter / re-persist.
**Area:** engine auto-move path (`src/main_window.cpp` auto-move / `src/engine/engine_controller.cpp`),
`MatchConfig` (`src/model/config.h`), "Engine plays" menu (`src/ui/`)
**Priority:** P2
**Source:** UI review request, 2026-08-30. Builds on UI-06 (`MatchConfig::enginePlays`).

## Problem / request

With "Engine plays" set to Black or White (UI-06), the engine auto-moves on its turn. The user
wants: if the user **interrupts** that arrangement — presses Stop, or manually asks the engine to
analyze the opponent's turn instead of letting it play its own — the app should drop back to
`EnginePlaysSide::Off` ("Mode None" / manual analyze). I.e. any manual intervention cancels
auto-play rather than leaving it armed to fire again on the next turn.

## Resolved with the user (2026-08-31)

1. **Exact triggers — exactly two:**
   - Toolbar/hotkey **Stop** (`MainWindow::onStopAnalysis`) pressed while `enginePlays != Off`.
   - User invokes **Analyze** (`MainWindow::onStartAnalysis` / dispatcher `analyze`) while it is
     the engine's assigned turn (side-to-move matches the assigned side).
   - A plain board move on the engine's turn does **not** trigger the revert (out of scope).
2. **Whole setting, not per-side.** Revert the entire `enginePlays` to `Off` regardless of whether
   the other side was also (implicitly) assigned — one enum, one value.
3. **Feedback: quiet.** Just update `enginePlays -> Off` and sync the menu radio. No status-bar
   message, no toast.
4. **No persistence.** Update the in-memory `MatchConfig` only. Do **not** call
   `SettingsStorage::save` — unlike `onSetEnginePlays`, this revert is a transient session action.
   On next launch the persisted (user-chosen) side is restored.

**Rationale (user):** disabling auto-play on manual intervention keeps the engine flow in sync and
avoids conflicts — e.g. engine assigned White, user asks the engine to play a Black move.

## Acceptance criteria

- After Stop (while a side was assigned) or Analyze-on-engine's-turn, `MatchConfig::enginePlays ==
  Off` and the "Engine plays" menu radio reflects Off.
- The engine does not auto-move again until the user re-selects a side.
- `SettingsStorage::save` is **not** called by the revert path; the settings file's `engine_plays`
  value is unchanged.
- One-shot Analyze/Stop still works normally in the reverted state.

## Scope boundary

- Do not change UI-06's auto-move mechanics themselves, only add the revert-on-interrupt transition.

## Related

- UI-06 (`docs/todo/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md`)
- UX-06 (WinGraph SingleSide currently reads `enginePlays`; see UI-09 which removes that coupling)
