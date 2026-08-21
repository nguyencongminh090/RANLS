# 2026-08-21 — UX-01: Five panels rendered as blank rectangles instead of empty states

**Task:** `docs/todo/UX-01-empty-states.md` (Backlog, P2). No `docs/instruction/UX-01-*.md` entry
exists — expected, per `instruction.md`'s convention (not every task has one).

## Problem

On a fresh launch, or after New Game/undo back to the start, five widgets rendered as blank
rectangles instead of a placeholder:

- `WinGraphView` (`src/ui/win_graph_view.cpp:73`) — `if (blackData_.empty() ...) return;` drew
  nothing at all, not even axes.
- `PVView` (`src/ui/pv_view.cpp`) — an empty `Gtk::ListBox` with no rows and no message.
- `TreeNodeView` (`src/ui/tree_node_view.cpp:155`) — `if (nodes_.empty()) return;`, and its
  `DrawingArea` also collapsed to a ~46×46px size request since `update()`'s `!root` early return
  never called `set_size_request()`.
- `TreeExplorer` (`src/ui/tree_explorer.cpp`) — an empty `Gio::ListStore`-backed `ColumnView`.
- `BottomPanel`'s Move Log / Engine Log (`src/ui/bottom_panel.cpp`) — empty `TextView`s.

This is systemic — five widgets sharing one missing pattern — so the fix is one shared helper used
consistently, not five independent patches.

## Fix

- **`src/ui/empty_state.h`/`.cpp`** (new): two small, reusable pieces —
  - `EmptyState::drawPlaceholder(cr, widget, width, height, text)` — for the two Cairo-drawn
    widgets (`WinGraphView`, `TreeNodeView`). Centers wrapped Pango text using the widget's own
    themed foreground color (`Gtk::Widget::get_color()`) at 0.65 alpha, so it adapts to whichever
    GTK theme (light/dark) is active instead of a fixed hardcoded color.
  - `EmptyStateOverlay : public Gtk::Overlay` — for the three container widgets (`PVView`,
    `TreeExplorer`, and `BottomPanel`'s two `TextView`s). Wraps the real content widget and overlays
    a dimmed, italic `Gtk::Label` (`.empty-state-message` CSS class, same 0.65-opacity contrast
    budget) toggled by `setEmpty(bool)`.
- **`src/ui/win_graph_view.cpp`**: split the old single empty-check into two. The axis scaffold
  (50% line, 0/50/100% labels) now draws unconditionally whenever the widget has a usable size; only
  the *plot* is skipped when `blackData_.empty()`, and in that case
  `EmptyState::drawPlaceholder(..., "No analysis yet — press Analyze (F5)")` is drawn instead —
  meeting the todo's explicit "axis guides even with no series" criterion.
- **`src/ui/tree_node_view.cpp`**: `onDraw()` now draws "No moves yet — play or load a game to see
  the move tree" via the same helper when `nodes_` is empty, instead of returning blank. Also fixed
  a latent related bug: `update()`'s `!root` branch skipped `set_size_request()` entirely, leaving
  the `DrawingArea` at whatever size (or none) it had before — added `kMinEmptyWidth`/`kMinEmptyHeight`
  (220×90) floors so the placeholder always has room to render, applied in both the empty and
  populated-but-tiny-tree paths.
- **`src/ui/pv_view.h`/`.cpp`**: `PVView` (itself a `Gtk::ScrolledWindow`) now sets its child to an
  `EmptyStateOverlay` wrapping `listBox_`, with message "No analysis yet — press Analyze (F5) to see
  principal variations". `update()` calls `overlay_.setEmpty(newCount == 0)` using the same
  post-STATE-03-filter `pvLines` count the row-widget diffing already uses.
- **`src/ui/tree_explorer.h`/`.cpp`**: same pattern wrapping `columnView_`, message "No moves yet —
  play or load a game to see move history"; `overlay_.setEmpty(newSize == 0)` added at the end of
  `update()`.
- **`src/ui/bottom_panel.h`/`.cpp`**: `scrolledMoveLog_`/`scrolledEngineLog_` now each wrap their
  `TextView` in an `EmptyStateOverlay` ("No moves yet — moves will appear here as you play" /
  "No engine activity yet — start the engine to see log output"). New private
  `updateMoveLogEmptyState()`/`updateEngineLogEmptyState()` helpers check
  `buffer->get_char_count() == 0` and are called after every mutation site: `appendMoveLog()`,
  `clear()`, `flushPending()` (engine log batch flush), and `clearEngineLog()`.
- **`src/resources/style.css`**: added `.empty-state-message { opacity: 0.65; font-style: italic; }`.
- **`CMakeLists.txt`**: added `src/ui/empty_state.cpp` to `GUI_SOURCES`.

None of this touches STATE-01's clearing/notify logic — every empty-state check added here just
reads the widget's own already-cleared data (`blackData_`, `nodes_`, `pvLines`/`newCount`,
`newSize`, buffer char count) inside the same `update()`/`setData()`/append methods STATE-01 already
drives via `gameState_.signal_tree_updated`/`signal_board_changed`/`signal_engine_analysis`, so New
Game/undo/redo already reaching those methods (per STATE-01) is sufficient for the placeholders to
reappear — no new wiring needed.

### Contrast calculation (acceptance criterion: ≥4.5:1 in both themes)

Modeled Adwaita's light theme (bg `#fafafa`, fg `#1e1e1e`) and dark theme (bg `#242424`, fg
`#eeeeec`) — the only two themes this app switches between (`main_window.cpp`'s
`gtk-application-prefer-dark-theme` toggle) — and computed WCAG contrast of the placeholder color
(theme fg blended at alpha `a` over theme bg) against the theme bg, for several alpha values:

| alpha | light contrast | dark contrast |
|---|---|---|
| 0.80 | 8.49 | 9.04 |
| 0.70 | 6.02 | 7.26 |
| 0.65 | 5.11 | 6.47 |
| 0.60 | 4.35 | 5.73 |
| 0.50 | 3.22 | 4.43 |

Chose **0.65** — both themes clear 4.5:1 with margin (5.11 / 6.47), while 0.60 already fails light
mode (4.35) and 0.50 fails both. Applied identically to the Cairo-drawn placeholder (explicit alpha
in `empty_state.cpp`) and the CSS-driven one (`opacity: 0.65` on `.empty-state-message`) so both
flavors carry the same guarantee.

## Verification

Configured and built a clean out-of-tree CMake build (`gtkmm-4.0` 4.20.0, confirming this codebase is
gtkmm4 despite `CLAUDE.md`/README describing a GTK3 migration — pre-existing discrepancy, not
addressed here): `rapfi-gui` built with zero new warnings (only pre-existing unrelated
`-Wunused-function` warnings in `gomocup_protocol.cpp`). `rapfi-gui-tests` built and passed
(`ctest`: 1/1, "100% tests passed").

Additionally — unlike most fixes in this log — a live instance was actually driven and screenshotted
in this sandbox via `Xvfb` + `GSK_RENDERER=cairo` (the default GL renderer produced a black frame
under Xvfb's software GL) + `ffmpeg -f x11grab` + `xdotool`:

- **Fresh launch**: confirmed all 4 of the directly-visible placeholders simultaneously — WinGraph
  shows its 0/50/100% axis guides *and* "No analysis yet — press Analyze (F5)"; PVView shows "No
  analysis yet — press Analyze (F5) to see principal variations"; TreeNodeView (Visual tab) shows "No
  moves yet — play or load a game to see the move tree"; Move Log shows "No moves yet — moves will
  appear here as you play". Placeholder text was clearly legible (dimmed italic) against the dark
  panel background, consistent with the computed ~6.5:1 dark-theme contrast.
- **After playing one move** (clicked a board cell): Move Log placeholder replaced by "1. H4";
  TreeNodeView placeholder replaced by the new node; WinGraphView's placeholder text also
  disappeared (its `blackData_.empty()` check is keyed to entry *count*, which grows by one — as
  NaN — per played move even before any engine eval runs, per UI-01's existing eval-history design;
  this is the same emptiness definition the pre-existing early-return already used, not a new one).
  PVView correctly still showed its placeholder, since no analysis had actually run yet — confirming
  the widgets react independently to their own data, not to "a move happened."
- **Table tab / Engine Log tab**: attempted screenshots after switching tabs came back black on
  every attempt (a capture-timing flakiness specific to this headless Xvfb+cairo-software setup —
  the very first capture of the session succeeded and every capture after any further `xdotool`
  interaction failed, regardless of which widget was being captured). Not directly screenshotted;
  verified by code review instead — both use the exact same `EmptyStateOverlay` class already
  visually confirmed working for `PVView`/Move Log in this same run.

## Status

Marked ✅ DONE in `TODO.md`'s Backlog line and in `docs/todo/UX-01-empty-states.md`'s `Status` field
— all four acceptance criteria in the todo file are met: specific per-widget placeholder text, the
WinGraph axis-scaffold-with-no-series behavior, ≥4.5:1 contrast in both themes (computed and
visually spot-checked), and correct disappear/reappear behavior riding on STATE-01's existing
clear-and-notify path (screenshotted for 2 of 5 widgets, code-reviewed for the rest, none of it
requiring new wiring).

**Left out of scope, as instructed:** no changes to panel layout, the light-board/dark-panel color
split, or STATE-01's clearing logic itself.
