# 2026-08-30 — IO-01: implement Load Game / Save Game (were empty stubs)

## Prompt

Implement tracked task IO-01. `MainWindow::onLoadGame()` / `onSaveGame()` were
empty function bodies wired to real menu/toolbar actions — clicking Load or Save
was a silent no-op. Scope boundary (hard): a `key=value`-style text format in the
`settings_storage.cpp` style (no JSON), a gtkmm-free serializer under `src/model/`
so it is unit-testable, `Gtk::FileDialog` for both pickers, discard-confirmation
on Load over a non-empty game, a visible error on corrupt input, and a regression
test. Out of scope: recent-files list, auto-save, format migration.

## Action

- New `src/model/game_io.h` / `game_io.cpp` — `GameIO::saveGame()` and
  `GameIO::loadGame()`. Format: a `# comment` header, `yxgame_version=1`,
  `board_size=`, `rule=` (0/1/2), then one `move=x,y` line per ply in play order.
  Parser is tolerant of blank lines / comments / unknown keys but rejects
  (returns `std::nullopt` + error string, never throws) on: missing/unreadable
  file, absent or non-matching version, missing or out-of-range board size
  (5..22) or rule (0..2), a `move` before `board_size`, a malformed / non-numeric
  / out-of-range / duplicated move, or a non-comment line with no `=`. Mirrors
  `settings_storage.cpp`'s `trim` / strict integer parsing; no gtkmm/glibmm
  include, so it builds in the gtkmm-free test target.
- `MainWindow::onSaveGame()` — `Gtk::FileDialog::save()` (async, same pattern as
  `SettingsDialog::onChooseEngine`) → `GameIO::saveGame(path, boardSize, rule,
  history().moves())` → write failure shows the new error dialog.
- `MainWindow::onLoadGame()` — if the game is non-empty, wrap in
  `confirmDiscardGame("Loading a game", ...)` exactly like `onNewGame()`; inside
  the confirmed callback `Gtk::FileDialog::open()` → `GameIO::loadGame()` → on
  success `gameState_.newGame(boardSize)` / `setRule()` / replay `makeMove()` for
  each move / `controller_.sendConfig()`; on parse failure show the error dialog
  and leave the current game untouched.
- New `MainWindow::showErrorDialog()` — self-deleting modal `Gtk::MessageDialog`
  (ERROR / OK), same heap+delete-on-response idiom as `confirmDiscardGame()`.
- CMake: `game_io.cpp` added to `GUI_SOURCES` (`CMakeLists.txt`) and to both the
  test executable and its under-test source list (`tests/CMakeLists.txt`).
- New `tests/test_io01_game_io.cpp` (doctest): save→load round-trip identity,
  empty-game round-trip, missing file, and 14 corrupt/truncated/garbage inputs.

No `GameState` / `VariationTree` API change was required. Only the current game
line (`history().moves()`) is persisted — full variation-tree round-trip is not
in the acceptance criteria.

## Summary

`RUN_TESTS=1 ./build.sh`: app + test targets compile clean (`-Wall -Wextra
-Wpedantic`), `ctest` 1/1 passed. Direct run of the new cases: 4 cases / 48
assertions pass. The model/test target still links no gtkmm (enforced by
construction — it compiled). Round-trip reasoning against the three acceptance
criteria: (1) Save writes moves+rule+board size to the chosen path; (2) Load
confirms discard first when non-empty, then reconstructs an equivalent
`GameState` by replaying moves; (3) invalid/corrupt input fails with a visible
ERROR dialog, current game left intact.

Detail / status: [docs/todo/IO-01-load-save-game.md](../todo/IO-01-load-save-game.md)
