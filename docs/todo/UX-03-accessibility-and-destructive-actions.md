# UX-03 — Unlabelled icon buttons, no focus indication, no confirmation before destroying a game

**Status:** ✅ DONE
**Area:** toolbar / custom-drawn widgets / destructive actions
**Priority:** P2
**Source:** UI/UX + codebase review, 2026-08-21

## Resolution (2026-08-21)

Full summary and verification: `docs/fix-log.md` → `docs/fix-log/2026-08-21-ux-03-accessibility-and-destructive-actions.md`.

- **#1 (icon-only buttons)** — fixed: `setButtonTooltipAndLabel()` added to all four nav buttons in
  `src/main_window.cpp` (`set_tooltip_text()` + `Gtk::Accessible::Property::LABEL`).
- **#2 (custom-drawn widgets, no focus indicator/accessible role)** — **not fixed, recorded as an
  accepted limitation** instead: `docs/audit.md` → `docs/audit/2026-08-21-custom-drawn-widgets-no-keyboard-focus.md`.
  None of `BoardView`/`TreeNodeView`/`WinGraphView` has any keyboard focus mechanism today, and
  building one is exactly the out-of-scope keyboard-navigation feature this item's own scope
  boundary excludes — so there is no existing focus to draw an indicator for.
- **#3 (no confirmation before destroying a game)** — fixed: `MainWindow::confirmDiscardGame()`
  guards both `onNewGame()` and the board-size Apply handler, skipping the prompt when the board is
  already empty. Verified interactively (Yes/No/empty-board-no-nag all confirmed working).
- **Colour-only-meaning check** — database/candidate markers already pair hue with text (no change
  needed); win-graph black/white series now also differ by line dash pattern, not hue alone.
- **Contrast check** — found and fixed a real failure: board coordinate labels measured ~1.55:1
  against the board's fixed wood background (need ≥4.5:1), now ~8.0:1. Also fixed database-marker
  label contrast (added shadow) and made win-graph axis labels theme-aware via `get_color()` instead
  of a fixed gray that measured ~3.9-4.0:1 in both themes.

Build verified clean (including `tests/`, 89/89 passing — no test infra covers `src/main_window.cpp`/
`src/ui/*`, noted explicitly). Manual interactive verification done via headless Xvfb + xdotool.

## Problem

Three related gaps from the `ui-ux-review` universal-UX checklist.

### 1. Icon-only toolbar buttons have no accessible name or tooltip

`src/main_window.cpp:223-226` creates the navigation group as bare glyph labels:

```cpp
btnFirst_ = Gtk::make_managed<Gtk::Button>("⏮");
btnUndo_  = Gtk::make_managed<Gtk::Button>("↶");
btnRedo_  = Gtk::make_managed<Gtk::Button>("↷");
btnLast_  = Gtk::make_managed<Gtk::Button>("⏭");
```

No `set_tooltip_text`, no `Gtk::Accessible::Property::LABEL`. A screen reader announces the raw
glyph; a sighted first-time user gets no hover confirmation.

`EngineStatusView` already does this correctly (`src/ui/engine_status.cpp:63`, `:71`, `:72`) — use it
as the in-repo reference pattern.

### 2. Custom-drawn widgets have no focus indicator and no accessible role

`BoardView`, `TreeNodeView`, and `WinGraphView` are plain `Gtk::DrawingArea`s that paint everything
themselves. GTK4 draws `:focus-visible` for standard widgets, but custom draw callbacks get nothing
for free:

- `BoardRenderer::draw` (`src/ui/board_renderer.cpp:65-87`) has eight layers, none of which draws a
  keyboard focus ring for a focused cell. There is also no keyboard board navigation at all — only
  `GestureClick` and `EventControllerMotion` (`src/ui/board_view.cpp:19-45`).
- `TreeNodeView::onDraw` (`src/ui/tree_node_view.cpp:151-226`) draws hover state but not focus.
- None of the three implements `Gtk::Accessible`, so assistive tech sees an unlabelled drawing area
  where the board is.

Flag as a known limitation if full keyboard/AT support is out of scope — but record it rather than
leaving it implicit.

### 3. No confirmation before destroying the current game

Both paths wipe the board, history, and variation tree with no prompt:

- `MainWindow::onNewGame` → `gameState_.newGame()` (`src/main_window.cpp:452-456`) — also reachable
  from the `Ctrl+N` hotkey (`src/main_window.cpp:101`) and the toolbar
- Board-size Apply → `gameState_.newGame(size)` (`src/main_window.cpp:496-501`), which discards the
  game as a side effect of changing a *setting*

The checklist requires confirmation before destructive, irreversible actions. The board-size case is
worse than New Game because losing the game is not what the user asked for.

## Acceptance criteria

- Every icon-only button has a tooltip **and** an accessible label.
- Custom-drawn widgets either draw a visible focus indicator and expose an accessible role, or the
  gap is documented explicitly in `docs/audit.md` as an accepted limitation with a reason.
- New Game and board-size change confirm before discarding a non-empty game (no prompt when the
  board is already empty — don't nag).
- Colour is never the sole carrier of meaning: check the database `W`/`L`/`D` labels
  (`src/model/board_view_model.cpp:75`), the winrate heat-map colouring
  (`src/ui/board_renderer.cpp:17-38`), and the win-graph series colours
  (`src/ui/win_graph_view.cpp:9-11`) — each should pair hue with shape or text.
- Contrast of Cairo-drawn text (coordinate labels at `src/ui/board_renderer.cpp:129`, marker labels,
  graph axis labels) verified ≥4.5:1 in both light and dark themes. Note the board is hand-painted
  wood (`src/ui/board_renderer.cpp:7`) and does **not** follow the GTK theme, so the light/dark
  combination needs checking against fixed board colours.

## Scope boundary

- Adding full keyboard board navigation is a feature, not part of this item — this item requires
  either the focus indicator for whatever focus exists, or an explicit recorded decision.
- Layout at extreme board sizes is UX-04.

## Related

- UX-01 (empty-state contrast), UX-04 (board size ergonomics)
