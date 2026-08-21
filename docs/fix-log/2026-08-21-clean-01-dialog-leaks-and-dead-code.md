# 2026-08-21 — Dialog leaks, dead signals, leftover debug output (CLEAN-01)

## Summary

Housekeeping-only pass over `docs/todo/CLEAN-01-dialog-leaks-and-dead-code.md`'s five items. No
behaviour change intended or introduced.

## Action

1. **Leaked dialogs** (`src/main_window.cpp`, `onBoardSize`/`onSettings`/`onAbout`) — the
   board-size `Gtk::Window`, `SettingsDialog`, and `Gtk::AboutDialog` were each `new`-allocated with
   no matching `delete`. Fixed by calling `set_hide_on_close(true)` on each (so both the explicit
   `dialog->close()` call and the user clicking the native close button route through `hide()`) and
   connecting `signal_hide()` to `delete dialog` — the standard gtkmm idiom for self-destroying
   standalone dialogs (the `Gtk::Dialog` equivalent is deleting on `signal_response()`). A fresh
   dialog is still constructed on every open, so `SettingsDialog` continues to reflect the current
   `EngineConfig`/`ViewConfig` rather than showing stale data from a reused instance — deliberately
   not the "reuse a member instance" option the todo file also allowed, to avoid any risk of a
   visible behaviour difference.

2. **Dead signals** — verified via grep before touching anything: `GameState::signal_move_selected`
   is emitted in `GameState` at `src/model/game_state.cpp:238` (wired up by NAV-01) and
   `TreeExplorer::signal_node_selected` is emitted at `src/ui/tree_explorer.cpp:75` and connected in
   `src/ui/analysis_panel.cpp:167` (wired up by UI-02). Both already ✅ DONE in `TODO.md`. **Skipped
   — already fixed**, nothing left to do here.

3. **Leftover debug output** — removed `std::cerr << "[DBG] onNewGame called" << std::endl;` from
   `MainWindow::onNewGame()` (`src/main_window.cpp`). The now-unused `<iostream>` include was also
   dropped (no other `std::cerr`/`std::cout` use remained in the file).

4. **Unused local** — removed `auto ruleSubmenu = Gio::MenuItem::create("Rule", ruleSection);` in
   `MainWindow::buildMenuBar()` (`src/main_window.cpp`); only `gameMenu->append_submenu("Rule", ...)`
   was ever used. UI-03's parallel rule-menu work had not touched this line by the time this landed,
   so no conflict/adaptation was needed.

5. **Duplicated constant** — `kCoordMargin = 24.0` was independently defined in
   `src/ui/board_renderer.cpp` and `src/ui/board_view.cpp`. UX-04 (the broader geometry-unification
   item) had not landed yet, so per the todo file's fallback instruction this was fixed minimally:
   added `src/ui/board_geometry.h` with the single `static constexpr double kCoordMargin = 24.0;`
   definition, included from both `.cpp` files, removing each file's own copy. If UX-04 supersedes
   this with a broader geometry refactor later, this header is a trivial thing to fold in or remove.

## Verification

- `cmake -S . -B build -DCMAKE_CXX_FLAGS="-Wall -Wextra"` then `cmake --build build -j4`: full clean
  build, no warnings in `main_window.cpp`, `board_renderer.cpp`, or `board_view.cpp` (the only
  warnings in the build are pre-existing unused-function warnings in `src/engine/gomocup_protocol.cpp`,
  unrelated to this change and out of scope).
- `ctest` in `build/`: `rapfi-gui-tests` — 100% passed.
- Manual read-through of `onBoardSize`/`onSettings`/`onAbout`: Apply/Cancel/native-close paths all
  still call `close()` or are user-closed, which now route through `hide_on_close` → `signal_hide` →
  `delete`; no change to when dialogs open, what they show, or what Apply/Cancel do.
