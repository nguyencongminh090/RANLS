# CLEAN-01 — Leaked dialogs, dead signals, and leftover debug output

**Status:** open
**Area:** main window / general hygiene
**Priority:** P3
**Source:** codebase review, 2026-08-21

## Items

### 1. Dialogs allocated with `new` and never freed

| Site | Widget |
|---|---|
| `src/main_window.cpp:483` | `new Gtk::Window()` — the board-size dialog |
| `src/main_window.cpp:512` | `new SettingsDialog(...)` |
| `src/main_window.cpp:532` | `new Gtk::AboutDialog()` |

All three call `close()` (or are closed by the user) but the C++ objects are never deleted, so every
open leaks. Each also connects lambdas capturing `this` (`src/main_window.cpp:496`, `:513`) which
outlive the visible dialog.

Fix by managing lifetime explicitly — hold them as members, use `set_hide_on_close` plus reuse, or
delete on the close signal. Reusing a single settings dialog instance is probably simplest and also
preserves the user's scroll position.

### 2. Dead signals

- `GameState::signal_move_selected` (`src/model/game_state.h:85`) — connected at
  `src/main_window.cpp:308` but **never emitted anywhere**. See NAV-01.
- `TreeExplorer::signal_node_selected` (`src/ui/tree_explorer.h:17`) — never emitted, never
  connected. See UI-02.

Both are tracked as sub-points of their functional items; listed here so the cleanup is not lost if
those items are descoped.

### 3. Leftover debug output

`src/main_window.cpp:454`:

```cpp
std::cerr << "[DBG] onNewGame called" << std::endl;
```

### 4. Unused local

`src/main_window.cpp:154`:

```cpp
auto ruleSubmenu = Gio::MenuItem::create("Rule", ruleSection);   // never used
gameMenu->append_submenu("Rule", ruleSection);
```

### 5. Duplicated constant

`kCoordMargin = 24.0` is defined independently in `src/ui/board_renderer.cpp:40` and
`src/ui/board_view.cpp:6`. The two must stay equal for clicks to land where stones are drawn, but
nothing enforces that. See UX-04, which covers unifying the geometry calculation.

**Update 2026-08-21:** UX-04 fixed this — `BoardRenderer::computeGeometry()` is now the single
definition, and both `BoardRenderer::draw()` and `BoardView::pixelToCoord()` call it. This item's
"`kCoordMargin` defined once" acceptance criterion below is satisfied; the rest of CLEAN-01 (dialog
leaks, dead signals, debug output, unused local) is untouched and still open.

## Acceptance criteria

- No `new`-allocated dialog leaks.
- Debug `std::cerr` removed (or routed through the engine log if it was actually wanted).
- Unused local removed.
- `kCoordMargin` defined once.
- Build is warning-clean for these files.

## Scope boundary

Housekeeping only — no behaviour change. Do not bundle functional fixes into this item; if touching
one of these lines requires a behaviour change, that belongs to the functional item that owns it.

## Related

- NAV-01, UI-02 (the dead signals), UX-04 (geometry duplication)
