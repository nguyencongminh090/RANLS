# STATE-04 — Rule and board size are never persisted; reset on every launch

**Status:** ✅ FIXED (Sprint 6, 2026-08-30) — design questions resolved with user 2026-08-30; dispatched via `/implement-task`

## Resolution

- `src/model/config.h`: new `struct GameSetupConfig { GameRule rule; int boardSize; }`
  (`#include "board_state.h"` for `GameRule` / `DEFAULT_BOARD_SIZE` / `MAX_BOARD_SIZE`).
- `src/model/settings_storage.{h,cpp}`: `GameSetupConfig setup;` added to
  `SettingsBundle`; `save()` gained a `const GameSetupConfig& = {}` 4th param;
  `load()`/`save()` now round-trip `rule` (accepts only 0/1/2, else Freestyle)
  and `board_size` (accepts 5..MAX_BOARD_SIZE inclusive, else DEFAULT_BOARD_SIZE)
  — same validate-or-fallback idiom as the `engine_plays` block.
- `src/main_window.{h,cpp}`: startup restores `saved.setup.rule` via
  `setRule()` and, if it differs, `newGame(saved.setup.boardSize)` (board empty
  at startup, discards nothing; engine sees it via `onStartAnalysis`'s
  `sendConfig()`). New `persistGameSetup()` helper called from `onSetRule()` and
  the Board Size dialog Apply handler (inside the confirm-success callback).
  `onNewGame()` now does `newGame(gameState_.boardSize())` instead of the bare
  `newGame()`. All three `SettingsStorage::save()` call sites (`onSettings`,
  `onSetEnginePlays`, `persistGameSetup`) pass all four current config blocks
  (STATE-02 hazard).
- `tests/test_settings_storage.cpp`: 4 new cases — defaults on missing file
  (Freestyle/15), set-and-reload round-trip (Renju/20 + default-reads-back),
  out-of-range rule/size fallback + boundary values (5 and MAX_BOARD_SIZE
  accepted), and other-blocks-not-corrupted.

## Verification

- Build: `RUN_TESTS=1 ./build.sh` — clean; the only warnings are the three
  pre-existing unused-function warnings in `gomocup_protocol.cpp`, unrelated.
- Unit tests: `ctest` — both targets pass (129 cases / 1058 assertions in
  `rapfi-gui-tests`, including the 4 new STATE-04 cases).
- Manual: launched `./build_cmd/rapfi-gui` on the live display with a
  hand-written `rule=2 / board_size=17` settings file — app starts cleanly and
  consumes both keys without rewriting or corrupting the file. The menu-driven
  set-rule / resize / quit / relaunch click-through could not be scripted in
  this environment (no GTK UI-automation driver); the write path is covered by
  the new unit cases and the wiring verified by source reading.
- No regression: engine-path / theme / `engine_plays` round-trip cases still
  green; the three `save()` call sites all thread the full four-block state.

**Area:** `src/model/settings_storage.cpp` (save/load), `src/model/config.h`, startup + change-handler wiring in `src/main_window.cpp`
**Priority:** P3
**Source:** reported by user during the UI-06 (`Engine plays`) end-to-end smoke pass.

## Problem

Selecting a rule (Renju / Standard / Freestyle) via the menu, or changing the
board size via **Board Size…**, is not written to the settings file. On next
launch `GameState` comes up with its compiled-in defaults — `GameRule::Freestyle`
([src/model/game_state.h:126](../../src/model/game_state.h#L126)) and
`DEFAULT_BOARD_SIZE` (15) — so the rule always reverts to Freestyle and the board
size resets.

### Root cause

`SettingsStorage::save()` / `load()`
([src/model/settings_storage.cpp:117-218](../../src/model/settings_storage.cpp#L117-L218))
serialize the `EngineConfig`, `ViewConfig` and (since UI-06) `MatchConfig` blocks
only. There is **no `rule=` or `board_size=` key** in the on-disk format at all.
`MainWindow::onSetRule()` ([src/main_window.cpp:728](../../src/main_window.cpp#L728))
and the Board Size dialog's Apply handler
([src/main_window.cpp:770-780](../../src/main_window.cpp#L770-L780)) mutate
`GameState` but never call `SettingsStorage::save()`, and the startup block
([src/main_window.cpp:134-139](../../src/main_window.cpp#L134-L139)) seeds only
engine/view/match config from `SettingsStorage::load()`.

Not a UI-06 regression — UI-06's `engine_plays` key *is* round-tripped correctly;
this gap predates it and was merely found during its smoke test.

## Resolved decisions (with user, 2026-08-30)

- **Rule → global preference.** The last-selected rule is written to the settings
  file and restored on every launch, independent of any loaded game. (Same model
  as `ViewConfig` / `EngineConfig`.)
- **Board size → persist last size as the new-game default.** The size chosen in
  **Board Size…** is saved and becomes the default for future launches and
  New Game. (`GameIO` already records board size *per saved game* — that path is
  unchanged.)
- No new UI. The existing menu + Board Size dialog are the only entry points.

## Scope

1. `src/model/config.h`: add
   `struct GameSetupConfig { GameRule rule = GameRule::Freestyle; int boardSize = DEFAULT_BOARD_SIZE; };`
   (include `board_state.h` for `GameRule` / `DEFAULT_BOARD_SIZE`).
2. `src/model/settings_storage.{h,cpp}`: add `GameSetupConfig setup;` to
   `SettingsBundle`; serialize/deserialize `rule` and `board_size` keys next to
   the `engine_plays` block, with the same "value out of range → struct default"
   guard used for `engine_plays` (rule ∈ {0,1,2}; board size clamped to
   `5..MAX_BOARD_SIZE`). Extend `save()`'s signature with
   `const GameSetupConfig& = {}` (mirrors how `MatchConfig` was threaded for UI-06).
3. `src/main_window.cpp`:
   - Startup ([main_window.cpp:134-139](../../src/main_window.cpp#L134-L139)):
     after `load()`, `gameState_.setRule(saved.setup.rule)` and, if
     `saved.setup.boardSize != gameState_.boardSize()`, `gameState_.newGame(saved.setup.boardSize)`
     then `controller_.sendConfig()`.
   - `onSetRule()` and the Board Size dialog Apply handler: after mutating
     `GameState`, persist via `SettingsStorage::save(gameState_.engineConfig(),
     gameState_.viewConfig(), gameState_.matchConfig(), {gameState_.rule(), gameState_.boardSize()})`.
   - `onNewGame()` ([main_window.cpp:632-641](../../src/main_window.cpp#L632-L641)):
     pass the persisted default size to `newGame()` instead of the bare
     `newGame()` (which hardcodes `DEFAULT_BOARD_SIZE`).
4. `tests/test_settings_storage.cpp`: round-trip case — default reads back
   `Freestyle` + size 15; a set-to-Renju + size-20 value survives save+load; an
   out-of-range rule / size falls back to the default.

## Acceptance criteria

- Rule chosen in the menu persists across an app restart.
- Board size chosen in **Board Size…** persists across a restart and is the
  default for the next New Game.
- Existing `engine_plays` / `EngineConfig` / `ViewConfig` persistence is
  untouched (STATE-02 regression guard).
- No change to `GameIO` per-game save/load.

Execution guidance: [docs/instruction/STATE-04-rule-and-board-size-not-persisted.md](../instruction/STATE-04-rule-and-board-size-not-persisted.md)

## Related

- UI-06 (found during its smoke pass), STATE-02 (settings dialog dropping config
  fields — same "don't drop the other blocks on save" hazard), IO-01 (`GameIO`
  already persists rule + board size *per saved game*).
