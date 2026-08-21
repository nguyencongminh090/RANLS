# 2026-08-21 — UX-03: unlabelled icon buttons, missing destructive-action confirmations, and colour-only/contrast gaps

## Summary

Three bundled sub-problems from `docs/todo/UX-03-accessibility-and-destructive-actions.md`, plus the
colour/contrast follow-up items in its acceptance criteria.

## Fix

### 1. Icon-only toolbar buttons had no accessible name or tooltip

`src/main_window.cpp` (`buildToolbar`) created the nav group (`btnFirst_`/`btnUndo_`/`btnRedo_`/
`btnLast_`) as bare glyph-labelled `Gtk::Button`s with neither `set_tooltip_text()` nor
`Gtk::Accessible::Property::LABEL`. Added a `setButtonTooltipAndLabel()` helper that sets both —
`set_tooltip_text()` for sighted hover, plus `update_property(Gtk::Accessible::Property::LABEL, …)`
via a `Glib::Value<Glib::ustring>` for screen readers (GTK4 does not derive the accessible name from
the tooltip text automatically). Mirrors the tooltip half of the existing
`EngineStatusView::btnStart_`/`btnStop_`/`btnReload_` pattern (`src/ui/engine_status.cpp`); the
accessible-label half is new.

### 2. Custom-drawn widgets have no focus indicator or accessible role

Recorded as an accepted, explicitly-reasoned limitation instead of implemented — see
`docs/audit.md` → `docs/audit/2026-08-21-custom-drawn-widgets-no-keyboard-focus.md`. `BoardView`,
`TreeNodeView`, and `WinGraphView` have no keyboard focus mechanism at all today (only
`GestureClick`/`EventControllerMotion`), and UX-03's own scope boundary excludes building one from
scratch (that belongs to a separate keyboard-navigation feature). No code change for this sub-item.

### 3. No confirmation before destroying the current game

Added `MainWindow::confirmDiscardGame(action, onConfirmed)` (`src/main_window.h`/`.cpp`): if
`gameState_.history().moveCount() == 0` it runs `onConfirmed` immediately (empty board — nothing to
lose, no nag); otherwise it shows a modal `Gtk::MessageDialog` (WARNING, YES_NO) naming the action
("Starting a new game" / "Changing the board size") and only runs `onConfirmed` on `YES`. Wired into
both `MainWindow::onNewGame()` and the board-size Apply handler inside `MainWindow::onBoardSize()`
(previously unconditional `gameState_.newGame()` / `gameState_.newGame(size)` calls).

### Colour-only-meaning check (acceptance criterion)

- Database W/L/D markers (`BoardRenderer::drawDatabaseMarkers`) and MultiPV candidate markers
  (`drawCandidateMoves`) already pair the heat-map hue with a text label (`entry.label`/`boardText`,
  or a `%`/mate-score string) — confirmed no colour-only case here, no change needed.
- `WinGraphView`'s black/white series (`src/ui/win_graph_view.cpp`) were told apart by hue alone
  (blue vs. yellow). Added a dash pattern (`cr->set_dash({4.0, 3.0}, 0.0)`) to the white series so
  the two lines also differ by shape.

### Contrast check (acceptance criterion) — one real failure found and fixed, one improved

Computed WCAG contrast ratios (standard relative-luminance formula) against the board's own fixed
colours, since `src/ui/board_renderer.cpp` explicitly paints a hand-picked wood colour rather than
following the GTK theme:

- **Coordinate labels** (`kCoordR/G/B`, was `(0.60, 0.60, 0.55)`) against the board background
  `(0.87, 0.72, 0.53)`: **~1.55:1** — well under the 4.5:1 text minimum. Changed to
  `(0.20, 0.14, 0.08)`, a dark warm brown, measuring **~8.0:1**.
- **Database marker labels**: white text at alpha 0.9/0.95 directly on the heat-map-coloured diamond
  drops to **~1.54:1** in the yellow-green band (winrate ≈65%). Added the same black-shadow-behind-
  white-text treatment already used by `drawCandidateMoves` in the same file, keeping the label
  legible across the full heat-map hue range.
- **Win-graph axis labels** (`0%`/`50%`/`100%`): a fixed `(0.5, 0.5, 0.5)` gray measured **~3.9-4.0:1**
  against both a typical light and dark Adwaita background — just under 4.5:1, and no single fixed
  gray can clear 4.5:1 against both a near-white and a near-black background simultaneously. Since
  this widget's background *does* follow the GTK theme (unlike the board), switched the label draw
  color to `get_color()` (the widget's live theme foreground), the same mechanism `Gtk::Label` uses,
  so it inherits the theme's own contrast guarantees in both light and dark instead of a hardcoded
  guess.
- Grid lines, stone fills, and the last-move ring were checked but left unchanged (grid ~6.8:1;
  black stone ~9.4:1 vs. board; white stone ~1.7:1 and last-move red ~2.5:1 are non-text graphical
  marks already reinforced by shape/border, not the text-contrast criterion this item targets, and
  changing stone rendering was outside the reported scope).

## Verification

- **Build:** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` then `make -C build -j4` — clean build,
  zero warnings from any changed file (`main_window.h/.cpp`, `board_renderer.cpp`,
  `win_graph_view.cpp`). Also built the full project including `tests/` (`make -C build -j4`, no
  target restriction) — `rapfi-gui-tests` compiled and ran clean: **89/89 test cases, 859/859
  assertions passed**. No existing test touches `src/main_window.cpp`/`src/ui/*` (the `tests/`
  target is explicitly model/engine-layer only, no gtkmm/display server dependency per its own
  CMakeLists comment) — this UI-only change has no test infrastructure to extend; noting that
  explicitly rather than skipping silently, per this repo's bug-fix workflow rule.
- **Manual/interactive (headless Xvfb + `dbus-run-session`, GDK_BACKEND=x11, driven via `xdotool`):**
  - Placed a move (D15), clicked **New** → the confirmation dialog appeared with the exact wording
    "Starting a new game will discard the current game (board, move history, and variation tree).
    Continue? / This cannot be undone." and Yes/No buttons; **No** left the board and move log
    intact; **New** again then **Yes** correctly cleared both.
  - Clicked **New** on an already-empty board → no dialog, `onNewGame` ran straight through
    (confirmed via a temporary debug line logging `moveCount()`, since removed) — the "don't nag on
    an empty board" criterion holds.
  - Placed a move, opened **Game → Board Size**, clicked **Apply** → the same confirmation flow
    fired with the board-size-specific wording ("Changing the board size will discard…"), confirming
    the second call site is wired correctly and independently from New Game.
  - Screenshotted the board: coordinate labels are now clearly legible dark-brown-on-wood at normal
    render size (matches the ~8.0:1 computed ratio); the win-graph's `0%`/`50%`/`100%` axis labels
    render in the theme's light gray against the app's dark theme, consistent with using
    `get_color()`.
  - Tooltip/accessible-label calls on the 4 nav buttons were verified by code inspection (same
    `set_tooltip_text()` API already proven correct at `src/ui/engine_status.cpp:63,71,72`) and by
    successful compilation of the `Gtk::Accessible::Property::LABEL` call — GTK4 tooltip popup
    surfaces did not reliably composite in the headless/no-window-manager Xvfb rig used for this
    session (a rig limitation, not something exercised by the code path), so the tooltip's on-screen
    appearance specifically was not directly screenshotted.

## Scope notes

- Full keyboard board/tree/graph navigation was explicitly out of scope per UX-03's own boundary —
  not attempted; see the audit entry for the path forward.
- Board-size-extremes layout (UX-04) was not touched.
- Stone-fill/last-move-marker contrast against the wood board (white stone ~1.7:1, last-move red
  ring ~2.5:1) was measured but left unchanged — outside what was reported (text-label contrast),
  and both are non-text graphical marks already reinforced by an outline/border, not raised as a
  distinct concern in the source review.
