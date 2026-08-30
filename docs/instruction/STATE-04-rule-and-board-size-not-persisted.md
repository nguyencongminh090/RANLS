# Instruction — STATE-04: persist the selected rule and board size

Detail: [docs/todo/STATE-04-rule-and-board-size-not-persisted.md](../todo/STATE-04-rule-and-board-size-not-persisted.md)

## Design decisions (resolved with the user 2026-08-30 — do not re-open)

- **Rule** is a **global preference**: saved to the settings file, restored on
  every launch, independent of any loaded game.
- **Board size**: the size chosen in **Board Size…** is saved and used as the
  **default for future launches and New Game**.
- Persist via a **new `GameSetupConfig` struct** in `src/model/config.h`
  (`GameRule rule`, `int boardSize`), threaded through `SettingsBundle` and
  `SettingsStorage::save()` exactly the way `MatchConfig` was for UI-06.
- No new UI. Do not touch `GameIO` (per-saved-game rule/size persistence is
  correct and separate).

## Approach

1. `src/model/config.h`: `#include "board_state.h"`; add
   `struct GameSetupConfig { GameRule rule = GameRule::Freestyle; int boardSize = DEFAULT_BOARD_SIZE; };`.
   (`board_state.h` is a leaf header — no include cycle; it already defines
   `GameRule`, `DEFAULT_BOARD_SIZE`, `MAX_BOARD_SIZE`.)
2. `src/model/settings_storage.h`: add `GameSetupConfig setup;` to
   `SettingsBundle`; change `save()` to
   `bool save(const EngineConfig&, const ViewConfig&, const MatchConfig& = {}, const GameSetupConfig& = {})`.
3. `src/model/settings_storage.cpp`:
   - In `load()`, after the `engine_plays` block
     ([settings_storage.cpp:175-182](../../src/model/settings_storage.cpp#L175-L182)),
     parse `rule` and `board_size`. Copy the exact validate-or-default idiom used
     for `engine_plays`: `rule` accepts only 0/1/2 (→ `GameRule::Freestyle/Standard/Renju`),
     anything else → `GameSetupConfig{}.rule`. `board_size` must be in
     `5..MAX_BOARD_SIZE` inclusive, else → `GameSetupConfig{}.boardSize`.
   - In `save()`, after `engine_plays`
     ([settings_storage.cpp:215](../../src/model/settings_storage.cpp#L215)),
     write `rule=<int>` and `board_size=<int>`.
4. `src/main_window.cpp`:
   - Startup block ([main_window.cpp:134-139](../../src/main_window.cpp#L134-L139)):
     after `gameState_.setMatchConfig(saved.match)` add
     `gameState_.setRule(saved.setup.rule);` and
     ```
     if (saved.setup.boardSize != gameState_.boardSize())
         gameState_.newGame(saved.setup.boardSize);
     ```
     Board is empty at startup so `newGame()` here discards nothing. `sendConfig()`
     is already issued elsewhere at startup — verify the engine sees the restored
     size/rule (a `controller_.sendConfig()` after the block is acceptable if not).
   - Add a private helper e.g. `void MainWindow::persistGameSetup()` that calls
     `SettingsStorage::save(gameState_.engineConfig(), gameState_.viewConfig(),
     gameState_.matchConfig(), {gameState_.rule(), gameState_.boardSize()});`
     and call it at the end of `onSetRule()`
     ([main_window.cpp:728-732](../../src/main_window.cpp#L728-L732)) and at the
     end of the Board Size dialog's Apply lambda
     ([main_window.cpp:770-780](../../src/main_window.cpp#L770-L780)), inside the
     `confirmDiscardGame` success callback (so a cancelled confirm does not save).
   - `onNewGame()` ([main_window.cpp:632-641](../../src/main_window.cpp#L632-L641)):
     `gameState_.newGame(gameState_.boardSize())` — keep the current size as the
     new-game size rather than snapping back to `DEFAULT_BOARD_SIZE`. (The
     persisted size already loaded into `gameState_` at startup, so this is
     equivalent to "new game uses the saved default".)
5. `tests/test_settings_storage.cpp`: add a `GameSetupConfig` round-trip case —
   see the existing `engine_plays` / `MatchConfig` test for the pattern. Assert:
   (a) a fresh `load()` with no file → `Freestyle`, size 15; (b) `save()` with
   `{Renju, 20}` then `load()` reads back `{Renju, 20}`; (c) a hand-written
   settings file with `rule=9` / `board_size=2` loads as the defaults; (d) a
   `save()` that passes only engine+view (defaulted `setup`) still reads back
   `Freestyle`/15 and does **not** corrupt the other blocks.

## Boundaries — do not touch

- No new dialog, menu, or settings-dialog control. (The Settings dialog is UX-06's;
  this task adds no UI.)
- Do not move `rule_` / `board_` out of `GameState` or into `GameSetupConfig` —
  `GameSetupConfig` is the *persistence* shape only; `GameState` keeps its
  existing members and `setRule()` / `newGame()` API.
- Do not change `GameIO::saveGame` / `loadGame` or the per-game file format.
- Do not alter `EngineConfig` / `ViewConfig` / `MatchConfig` fields or their keys.
- Do not add a rule/size control to `SettingsDialog`.
- Do not rework `docs/sprint/*` beyond this item's status/burndown update.

## Pitfalls

- **STATE-02 hazard:** `save()` truncates and rewrites the whole file. Every call
  site (`onSettings` at [main_window.cpp:805](../../src/main_window.cpp#L805), the
  new `persistGameSetup()`, and anywhere else) must pass the *current* value of
  **all four** config blocks, never a default-constructed one, or it wipes the
  others. `onSettings` must gain the 4th arg `{gameState_.rule(), gameState_.boardSize()}`.
- `newGame()` early-returns if `analyzing_` — the startup call is before any
  analysis so it's fine, but `persistGameSetup()` from the Board Size handler
  runs after `gameState_.newGame(size)` inside `confirmDiscardGame`, so read
  `gameState_.boardSize()` *after* that call.
- Board size bounds: the Board Size spin button is 5–22
  ([main_window.cpp:765-766](../../src/main_window.cpp#L765-L766)); `MAX_BOARD_SIZE`
  is 22. Keep the load-time clamp consistent with the spin range.
- `GameRule` lives in `src/model/board_state.h`, not `config.h` — the new include
  is required and must not introduce a cycle (board_state.h includes nothing from
  the model layer; safe).
- Keep the `rule` enum→int mapping explicit (`static_cast<int>(GameRule::…)`),
  same as the `engine_plays` block — don't rely on unchecked casts on load.

## Verification before marking this task done

1. **Build:** `./build.sh` (or the documented CMake invocation) clean, no new warnings.
2. **Unit tests:** `ctest` in the build dir — all pass, including the new
   `tests/test_settings_storage.cpp` `GameSetupConfig` round-trip cases (default,
   set-and-reload, out-of-range fallback, other-blocks-not-corrupted).
3. **Manual smoke (state it was done, or state the display server / engine was
   unavailable and it was skipped):** launch the app; set rule to Renju and board
   size to 17; quit; relaunch — confirm rule is Renju and the board is 17×17;
   confirm New Game keeps 17×17. Set rule back to Freestyle, size 15, confirm that
   persists too.
4. No regression in engine-path / theme / `engine_plays` persistence (open
   Settings, change theme, restart — still applied).

Passing unit tests alone is NOT sufficient — the build must be clean and the
manual smoke run or explicitly reported as skipped-with-reason.
