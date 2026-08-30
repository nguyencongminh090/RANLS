# IO-01 — Load Game / Save Game are unimplemented stubs

**Status:** ✅ DONE

Implemented 2026-08-30. New gtkmm-free serializer `src/model/game_io.{h,cpp}`
(`GameIO::saveGame` / `GameIO::loadGame`) using a versioned `key=value` + `move=x,y`
text format in the `settings_storage.cpp` style — no JSON dependency, added to
both the app target and the gtkmm-free test target. `MainWindow::onSaveGame()`
and `onLoadGame()` wired via `Gtk::FileDialog` (same async pattern as
`SettingsDialog::onChooseEngine`); Load over a non-empty game routes through
`confirmDiscardGame("Loading a game", ...)` then rebuilds the model
(`newGame` → `setRule` → replay `makeMove` → `controller_.sendConfig()`).
Corrupt/failed I/O surfaces through a new self-deleting `showErrorDialog()`
(ERROR MessageDialog), never a silent no-op. No `GameState` API change needed.

**Verification:**
- `RUN_TESTS=1 ./build.sh` — app + test build clean, `ctest` 1/1 passed.
- New `tests/test_io01_game_io.cpp` (doctest): 4 cases / 48 assertions pass —
  save→load round-trip (board size, rule, move order), empty game, missing file,
  and 14 corrupt/truncated/garbage inputs each returning failure without crash.
- Model/test target still links no gtkmm (enforced by construction; it compiles).

Out of scope, deliberately omitted: recent-files list, auto-save, format
migration (the version field is checked-and-rejected only).
**Area:** main window, game persistence
**Priority:** P2
**Source:** surfaced during a 2026-08-30 leftover-task sweep of `src/`

## Context

`MainWindow::onLoadGame()` and `MainWindow::onSaveGame()`
([src/main_window.cpp:600-608](../../src/main_window.cpp#L600-L608)) are empty function bodies with
only a `// TODO: file chooser dialog` / `// TODO: file save dialog` comment. Both are wired to real
menu/toolbar actions, so clicking them today is a silent no-op — no dialog, no error, nothing.

## Scope boundary

- Implement a `Gtk::FileChooserDialog` (or GTK4 `Gtk::FileDialog`, whichever this codebase's other
  dialogs already use — check `gtk-ui-design` skill / existing dialog code first) for both open and
  save.
- Persistence format: check whether `GameState`/`MoveHistory` already has a serialization format
  (e.g. for the database/tree) before inventing a new one; reuse it if present.
- Follow `UX-03`'s precedent for destructive-action confirmation: Load Game over an unsaved,
  non-empty game should confirm discard the same way New Game does
  (`MainWindow::confirmDiscardGame`).
- Out of scope: any "recent files" list, auto-save, or format migration — file only what's asked.

## Acceptance criteria

- Save Game writes the current game (moves + rule + board size, at minimum) to a user-chosen path.
- Load Game reads it back into an equivalent `GameState`, confirming discard first if the current
  game is non-empty.
- Invalid/corrupt file on Load fails with a visible error, not a silent no-op or crash.

## Related

- UX-02 (settings-dialog validation pattern), UX-03 (destructive-action confirmation pattern) —
  reuse both conventions rather than inventing new ones.
