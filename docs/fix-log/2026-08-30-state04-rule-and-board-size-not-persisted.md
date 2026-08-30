# 2026-08-30 — Rule and board size are never persisted (STATE-04)

## Prompt

STATE-04, dispatched via `/implement-task`. Selecting a rule (Renju / Standard /
Freestyle) from the menu or changing the board size via **Board Size…** was never
written to the settings file, so on the next launch `GameState` came up with its
compiled-in defaults (`GameRule::Freestyle`, `DEFAULT_BOARD_SIZE` 15). Found
during UI-06's `engine_plays` end-to-end smoke pass — pre-existing, not a UI-06
regression (`engine_plays` itself round-trips fine).

Design resolved with the user 2026-08-30: rule → global preference persisted to
the settings file and restored every launch; board size → persisted as the
new-game default; new `GameSetupConfig` persistence struct in `config.h`; no new
UI; `GameIO` (per-saved-game rule/size) untouched.

## Action

- **`src/model/config.h`** — `#include "board_state.h"`; added
  `struct GameSetupConfig { GameRule rule = GameRule::Freestyle; int boardSize = DEFAULT_BOARD_SIZE; };`.
  `board_state.h` is a leaf header (only `<array>` / `<cstdint>`), no include cycle.
- **`src/model/settings_storage.h`** — `GameSetupConfig setup;` added to
  `SettingsBundle`; `save()` signature extended with
  `const GameSetupConfig &setup = {}` (mirrors how `MatchConfig` was threaded for
  UI-06).
- **`src/model/settings_storage.cpp`** —
  - `load()`: after the `engine_plays` block, parse `rule` (switch on 0/1/2 →
    `GameRule::Freestyle/Standard/Renju`, any other value → `GameSetupConfig{}.rule`)
    and `board_size` (accepted only when `5 <= n <= MAX_BOARD_SIZE`, else
    `GameSetupConfig{}.boardSize`). Same validate-or-fallback idiom as
    `engine_plays`.
  - `save()`: after `engine_plays`, write `rule=<int>` and `board_size=<int>`.
- **`src/main_window.h`** — declared `void persistGameSetup();`.
- **`src/main_window.cpp`** —
  - Startup block (after `setMatchConfig(saved.match)`): `gameState_.setRule(saved.setup.rule);`
    and `if (saved.setup.boardSize != gameState_.boardSize()) gameState_.newGame(saved.setup.boardSize);`.
    Board is empty at startup so `newGame()` discards nothing; the engine is not
    started until the user first analyses, and `onStartAnalysis()` issues
    `sendConfig()` right after `startEngine()`, so the engine sees the restored
    size/rule then.
  - New `persistGameSetup()` helper — `SettingsStorage::save(engineConfig(),
    viewConfig(), matchConfig(), {rule(), boardSize()})`. Called at the end of
    `onSetRule()` and inside the Board Size dialog Apply handler's
    `confirmDiscardGame` success callback (after `newGame(size)`, so
    `boardSize()` already reflects the change; a cancelled confirm saves nothing).
  - `onNewGame()` now calls `gameState_.newGame(gameState_.boardSize())` instead
    of the bare `newGame()` (which hardcodes `DEFAULT_BOARD_SIZE`).
  - STATE-02 hazard: the other two `SettingsStorage::save()` call sites
    (`onSettings`, `onSetEnginePlays`) now also pass
    `{gameState_.rule(), gameState_.boardSize()}` as the 4th arg so a save never
    wipes rule/board_size.
- **`tests/test_settings_storage.cpp`** — 4 new `TEST_CASE`s:
  1. defaults on a missing file — `Freestyle` / 15;
  2. set-and-reload — `{Renju, 20}` survives save+load, and a default-`setup`
     save reads back `Freestyle` / 15;
  3. out-of-range `rule=9` / `board_size=2` fall back to defaults, an unrelated
     block (`max_depth`) still loads, over-large size rejected, boundary values
     (5 and `MAX_BOARD_SIZE`) accepted;
  4. saving a non-default `GameSetupConfig` does not corrupt engine / view /
     match blocks.

## Verification

- **Build:** `RUN_TESTS=1 ./build.sh` — clean. Only warnings are three
  pre-existing `-Wunused-function` in `gomocup_protocol.cpp`, unrelated to this
  change (confirmed by rebuilding just the touched files).
- **Unit tests:** `ctest` — `rapfi-gui-tests` 129 cases / 1058 assertions pass
  (incl. the 4 new STATE-04 cases), `rapfi-gui-ui-tests` passes. Existing
  `engine_plays` / theme / `multiPV` / `customParams` round-trip cases still
  green (no persistence regression).
- **Manual:** launched `./build_cmd/rapfi-gui` on the live display with a
  hand-written `rule=2` / `board_size=17` settings file — app starts cleanly,
  consumes both keys, does not rewrite or corrupt the file on startup. The full
  menu-driven click-through (set rule Renju + size 17 → quit → relaunch → confirm
  17×17 Renju board; New Game keeps 17) could not be scripted here — no GTK
  UI-automation driver in this environment. The save path is covered by the new
  unit cases and the startup-restore wiring verified by source reading + the live
  launch against a real settings file.

## Out of scope (per instruction Boundaries)

- No new dialog / menu / settings-dialog control.
- `GameState` keeps its own `rule_` / board members and `setRule()` / `newGame()`
  API — `GameSetupConfig` is the persistence shape only.
- `GameIO::saveGame` / `loadGame` and the per-game file format untouched.
- `EngineConfig` / `ViewConfig` / `MatchConfig` fields and keys untouched.
