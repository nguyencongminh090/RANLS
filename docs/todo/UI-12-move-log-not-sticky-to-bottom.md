# UI-12 — Move Log doesn't auto-scroll to the newest move

**Status:** ✅ FIXED (2026-08-31)

Fixed on branch `ui-12/move-log-not-sticky-to-bottom`. Root cause exactly as
sketched: `scrolledMoveLog_.set_child(moveLogView_)` directly (the `TextView`
is `Gtk::Scrollable`) replaces the `EmptyStateOverlay` wrapper that forced an
implicit `Gtk::Viewport` and made every `scroll_to` a no-op. `scrollToEnd` is
replaced by a non-static `scrollMoveLogToEnd()` scrolling to a persistent
right-gravity `moveLogEndMark_` and re-issuing once on idle (mirrors UI-10's
`scrollEngineLogToBottom` minus the flush machinery) — a plain immediate
scroll + immediate mark-delete still lands short during a burst append (a saved
game replays every move at once). `appendMoveLog` always follows to the bottom:
the "don't yank a user who scrolled up" behaviour is **deliberately out of
scope** — a naive pre-append at-bottom check latches permanently off after one
short-landed scroll mid-burst, and UI-10's full remembered-intent +
`value_changed` machinery does not fall out cleanly here.

Also cleaned up in the same change (the "Consider also"): removed
`moveLogOverlay_` / `engineLogOverlay_` members, `updateMoveLogEmptyState()` /
`updateEngineLogEmptyState()` and all their call sites, and the now-unused
`#include "empty_state.h"` from `bottom_panel.h`. `EmptyStateOverlay` itself is
kept — still used by `pv_view.h` and `tree_explorer.h`.

**Verification:** clean Debug build, no new warnings (3 pre-existing
`gomocup_protocol.cpp` unused-function warnings only). `ctest` 3/3 green:
`rapfi-gui-tests` (137 cases / 1081 assertions), `rapfi-gui-ui-tests` (11 cases
/ 70 assertions, incl. 2 new UI-12 cases), `rel02-version-single-source`. Both
new UI-12 cases confirmed to FAIL against the pre-fix tree (stash src fix,
rebuild: Move Log ScrolledWindow child is a `GtkViewport`; vadjustment value
stays 0 vs `upper - page ≈ 6987`). No live-engine binary on this machine, so a
manual GUI streaming check was not possible — same limitation UI-10's fix-log
noted; the behavioural ui-test drives the real `BottomPanel`.

See `docs/fix-log/2026-08-31-ui-12-move-log-not-sticky-to-bottom.md`.

---

**Original status:** 🔲 ACTIVE (Sprint 8, pulled from Backlog 2026-08-31)
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
