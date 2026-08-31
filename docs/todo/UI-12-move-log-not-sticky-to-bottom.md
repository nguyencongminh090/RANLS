# UI-12 — Move Log doesn't auto-scroll to the newest move

**Status:** 🔲 ACTIVE (Sprint 8, pulled from Backlog 2026-08-31)
**Area:** bottom panel / Move Log (`src/ui/bottom_panel.cpp`)
**Priority:** P3
**Source:** found 2026-08-31 while fixing UI-10 (its second pass)

## Problem

`BottomPanel::appendMoveLog()` calls `scrollToEnd(moveLogView_)` after every
insert, but the Move Log `TextView` is wrapped in `moveLogOverlay_`
(`EmptyStateOverlay` / `Gtk::Overlay`) as `scrolledMoveLog_`'s child. `Gtk::Overlay`
is not `Gtk::Scrollable`, so `Gtk::ScrolledWindow` interposes an implicit
`GtkViewport` and `scrollToEnd()` (a `moveLogView_.scroll_to(...)` call) is a
silent no-op — identical to the UI-10 Engine Log bug. Result: after enough
moves, the newest move is off the bottom of the Move Log until the user scrolls.

## Fix sketch

Same as UI-10's second pass: `scrolledMoveLog_.set_child(moveLogView_)` directly
(the `TextView` implements `Gtk::Scrollable`). The Move Log has no batched flush
/ front-trim, so it needs none of UI-10's `programmaticScroll_` / sticky-intent
machinery — the plain `scrollToEnd()` will just work once the widget is
reachable. If "don't yank a user who scrolled up" is wanted here too, reuse the
`sticky_scroll.h` helper.

Consider also dropping the now-purposeless `moveLogOverlay_` /
`engineLogOverlay_` members and `updateMoveLogEmptyState()` /
`updateEngineLogEmptyState()` (all no-ops since UI-08) in the same change.

## Acceptance criteria

- Making moves keeps the latest move visible at the bottom of the Move Log.
- A regression case in `rapfi-gui-ui-tests` (mirror
  `tests/test_ui10_engine_log_scroll_target.cpp`).
