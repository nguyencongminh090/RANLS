# UI-06 — replace the redundant "Analysis" menu with an "Engine plays" side selector

**Date:** 2026-08-30
**Tracked as:** `TODO.md` UI-06 (Sprint 6) — [todo](../todo/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md) · [instruction](../instruction/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md)

## Problem

The menu bar's **Analysis** submenu offered only Analyze / Stop — the exact two
actions already on the toolbar (`▶ Analyze` / `■ Stop`). Pure duplication, no
unique entries.

## Action

Repurposed the menu to control which side (if any) the engine auto-plays.

- **`src/model/config.h`** — `enum class EnginePlaysSide { Off, Black, White }`
  and `struct MatchConfig { EnginePlaysSide enginePlays = Off; }`, kept separate
  from `EngineConfig` / `ViewConfig` (governs auto-play, not search params or
  presentation).
- **`GameState`** — `matchConfig_` member + `matchConfig()` / `setMatchConfig()`
  (emits `signal_config_changed`), mirroring `setViewConfig`.
- **`src/model/settings_storage.{h,cpp}`** — `SettingsBundle::match`; `engine_plays`
  key round-tripped with a value-out-of-range → `Off` fallback; `save()` gained a
  `const MatchConfig& = {}` param so a Settings Apply can't reset it.
- **`src/main_window.cpp` `buildMenuBar()`** — the **Analysis** submenu is now
  **Engine plays**, a radio group (`Gio::SimpleAction::create_radio_string`
  `"engine-plays"`) with `::black` / `::white` / `::off`. `onSetEnginePlays()`
  updates `MatchConfig` + persists; `syncEnginePlaysMenu()` seeds the radio state
  from the loaded config at startup (kept in sync both directions). Toolbar
  `win.analyze` / `win.stop` untouched.
- **Auto-move path** — new `IEngineProtocol::generateMoveRequest()`;
  `GomocupProtocol` emits `BOARD …/DONE` (or `BEGIN` on the empty board) that
  *starts a search* and returns one committed move (unlike the analysis-only
  request). `EngineController::requestEngineMove()` sends it;
  `MainWindow::maybeStartAutoMove()` (on `signal_board_changed` and on the engine
  reaching `Idle`) fires it via a coalesced idle callback, gated on
  `enginePlays != Off`, engine running + `Idle`, and `enginePlays` == side to
  move. No loop — the engine's own move flips side-to-move.
- **`tests/test_gomocup_protocol.cpp`** — `generateMoveRequest` BOARD/DONE + BEGIN
  cases. **`tests/test_settings_storage.cpp`** — `engine_plays` round-trip
  (default `Off`, set-to-Black survives, default save reads back `Off`).

## Verification

- `./build.sh` clean, no new warnings; `ctest` 100% pass (new `MatchConfig` +
  protocol cases included).
- The user ran the interactive menu-click + engine self-move smoke on a live
  engine — the "Engine plays" selector and auto-move behave as specified. That
  same pass surfaced the pre-existing rule/board-size persistence gap, filed and
  fixed separately as STATE-04 (`engine_plays` itself round-trips correctly — not
  a UI-06 regression). UI-06 closed by the user 2026-08-30.

## Related

- STATE-04 (rule/board-size persistence — surfaced by this task's smoke pass),
  ENG-01 (engine lifecycle/state), UX-06 (WinGraph "Auto" perspective reads
  `MatchConfig::enginePlays`).
