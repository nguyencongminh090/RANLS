# 2026-08-31 — Move Log doesn't auto-scroll to the newest move (UI-12)

Follow-up `CODE` from UI-10's second pass
(`2026-08-31-engine-log-still-not-sticky-wrong-scroll-widget.md`), which noted
the Move Log carried the identical latent no-op and left it as tracked work.

## Prompt

Tracked task UI-12: "Move Log doesn't auto-scroll to the newest move — same
`Gtk::Overlay`-breaks-`Gtk::Scrollable` no-op that UI-10's second pass fixed for
the Engine Log." The todo file's Fix sketch + Acceptance criteria were the spec
(no `docs/instruction/UI-12-*.md`, as expected).

## Investigation

1. **Evidence / reproduce.** `BottomPanel::appendMoveLog()` calls
   `scrollToEnd(moveLogView_)` after every insert, but after enough moves the
   newest move sits below the visible area until the user scrolls.
2. **Localise.** Widget tree: `scrolledMoveLog_.set_child(moveLogOverlay_)` —
   the `Gtk::ScrolledWindow`'s child was `EmptyStateOverlay` (a `Gtk::Overlay`),
   not the `TextView`.
3. **Root cause.** `Gtk::Overlay` does not implement `Gtk::Scrollable`, so
   `Gtk::ScrolledWindow` interposes an implicit `GtkViewport`. The Viewport
   scrolls; the `TextView` is laid out at full height inside it, so
   `moveLogView_.scroll_to(...)` acts on the TextView's own non-scrolling view
   and is a silent no-op. `EmptyStateOverlay` has been an inert passthrough
   since UI-08 — it added nothing but the broken scroll indirection. Identical
   to the UI-10 Engine Log bug.
4. **Second-order finding (during test bring-up).** Once the TextView is the
   direct child, a *plain* `scrollToEnd` (create end mark → `scroll_to` →
   `delete_mark` immediately) still lands short during a burst of appends (a
   saved game replays every move under one layout pass): GTK defers
   scroll-to-mark past the post-insert relayout, and the mark is gone by then.
   A naive "only scroll if the user was already at the bottom" pre-append check
   then latches auto-scroll permanently off after the first short landing.

## Fix

`src/ui/bottom_panel.{h,cpp}`:

- `scrolledMoveLog_.set_child(moveLogView_)` — the `TextView` is the
  ScrolledWindow's direct child (it is `Gtk::Scrollable`), so it drives the real
  scroll.
- `scrollToEnd(Gtk::TextView&)` (static) replaced by non-static
  `scrollMoveLogToEnd()`: scrolls to a persistent right-gravity
  `moveLogEndMark_` (created once in the ctor) and re-issues once on
  `Glib::signal_idle` after the relayout, guarded by
  `moveLogScrollIdlePending_`. Mirrors UI-10's `scrollEngineLogToBottom` minus
  the flush/front-trim machinery.
- `appendMoveLog` always follows to the bottom. The "don't yank a user who
  scrolled up" behaviour is **deliberately out of scope** (recorded in
  `docs/todo/UI-12-*.md`): UI-10's remembered-intent + `value_changed` settle
  machinery does not fall out cleanly for a burst-appended log, and the naive
  alternative self-disables.
- Cleanup (todo "Consider also"): removed `moveLogOverlay_` /
  `engineLogOverlay_` members, `updateMoveLogEmptyState()` /
  `updateEngineLogEmptyState()` and every call site, and the now-unused
  `#include "empty_state.h"` from `bottom_panel.h`. `EmptyStateOverlay` the
  type is kept — still used by `pv_view.h` and `tree_explorer.h`.

UI-10's Engine Log scroll path, RT-02 buffer cap, and the UI-05 gutter were not
touched.

## Regression test

`tests/test_ui12_move_log_scroll_target.cpp` (new, added to
`rapfi-gui-ui-tests`; mirrors `test_ui10_engine_log_scroll_target.cpp`). Two
cases, each verified to FAIL on the pre-fix tree:

1. the Move Log ScrolledWindow's direct child is a `GtkTextView`, not a
   `GtkViewport` (pre-fix: `GtkViewport`);
2. 400 moves appended via `appendMoveLog` (event loop pumped periodically, as a
   real game plays move by move) ➝ the vadjustment ends within 4 px of
   `upper - page_size` (pre-fix: value stays 0 against `upper - page ≈ 6987`).

No "parked-at-top not yanked" case — that behaviour is out of scope (see above).

## Verification

- `rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4`
  — clean, no new warnings (3 pre-existing `gomocup_protocol.cpp`
  unused-function warnings, unrelated).
- `ctest` — 3/3: `rapfi-gui-tests` (137 cases / 1081 assertions),
  `rapfi-gui-ui-tests` (11 cases / 70 assertions, incl. 2 new UI-12 cases),
  `rel02-version-single-source`.
- Pre-fix teeth check: `git stash` of `src/ui/bottom_panel.{cpp,h}`, rebuild,
  `./tests/rapfi-gui-ui-tests -tc="UI-12*"` ➝ both cases fail (child is
  `GtkViewport`; value `0 >= 6987` false). Stash restored, all green again.
- No live engine binary on this machine, so a manual GUI streaming check was
  not possible — same limitation UI-10's fix-log noted. The behavioural ui-test
  drives the real `BottomPanel`.
