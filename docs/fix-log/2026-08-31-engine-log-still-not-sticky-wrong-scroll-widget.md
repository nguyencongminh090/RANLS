# 2026-08-31 — Engine Log still not sticky: it was scrolling the wrong widget

Follow-up to `2026-08-31-engine-log-sticky-bottom.md` (UI-10, PR #2), which did
**not** fix the reported bug.

## Prompt

User report: "UI-10: The slider is showing top instead of bottom. I want to see
the last line (see_end, bottom) during analysis, not first line."

## Investigation (systematic-debugging)

1. **Evidence / reproduce.** Engine Log pinned to the FIRST line for the whole
   analysis stream, never follows the tail. PR #2's persistent-mark +
   `Glib::signal_idle` re-scroll was in place and still failed.
2. **Localise.** `BottomPanel::scrollEngineLogToBottom()` calls
   `engineLogView_.scroll_to(engineLogEndMark_, 0.0, 0.0, 1.0)`. Traced the
   widget tree: `scrolledEngineLog_.set_child(engineLogOverlay_)` — the
   ScrolledWindow's child is `EmptyStateOverlay` (a `Gtk::Overlay`), **not** the
   `TextView`.
3. **Root cause.** `Gtk::Overlay` does not implement `Gtk::Scrollable`, so
   `Gtk::ScrolledWindow` interposes an implicit `GtkViewport`. The Viewport is
   what scrolls; the `TextView` is laid out at full height inside it. So
   `engineLogView_.scroll_to(...)` acts on the TextView's own (non-scrolling)
   view and is a **silent no-op**. `isScrolledToBottom()` — reading the
   viewport-side adjustment, which barely moves — also always returned true, so
   the "only stick if already at bottom" gate never forced a scroll either.
   Confirmed by reverting the fix: `gtk_scrolled_window_get_child()` returns a
   `GtkViewport` (post-fix: the `GtkTextView`).
   `EmptyStateOverlay` has been a pure no-op passthrough since UI-08 — it added
   nothing but the broken scroll indirection.

## Fix

`src/ui/bottom_panel.{h,cpp}`:

- `scrolledEngineLog_.set_child(engineLogView_)` — the `TextView` is the
  ScrolledWindow's direct child, so it drives the real scroll and
  `scroll_to(mark, 0,0,1.0)` moves the view. `engineLogOverlay_` kept as a
  member only so `updateEngineLogEmptyState()`'s no-op call compiles.
- New `programmaticScroll_` guard, set for the whole of `flushPending`'s buffer
  mutation + RT-02 front-trim + auto-scroll and cleared on that flush's trailing
  idle. The `value_changed` handler skips its "did the user scroll away?"
  re-derivation while it is set, so a mid-flush transient (our own scroll
  landing short, or the front-trim shifting content) can no longer latch
  `stickToBottom_` permanently off now that the adjustment genuinely moves.
- The persistent end-of-buffer mark and the `sticky_scroll.h` decision helpers
  from PR #2 are kept — they were correctly designed, just aimed at a TextView
  that could not scroll.

Move Log deliberately untouched: it has the same `scrolledMoveLog_` ➝
`moveLogOverlay_` ➝ `moveLogView_` structure and the same no-op
`scrollToEnd(moveLogView_)`, but that was not the reported bug. Noted as a
follow-up `CODE` in `docs/todo/UI-10-*.md`.

## Regression test

`tests/test_ui10_engine_log_scroll_target.cpp` (new, added to `rapfi-gui-ui-tests`
— links gtkmm, real widgets, self-skips with no display). PR #2's tests only
exercised the pure `sticky_scroll` helpers and so missed this entirely. Three
cases, each verified to FAIL on the pre-fix tree:

1. the Engine Log ScrolledWindow's direct child is a `GtkTextView`, not a
   `GtkViewport`;
2. 400 lines streamed via `appendSend` + real 50 ms flush timer ➝ the
   vadjustment ends within 4 px of `upper - page_size` (pinned to the bottom);
3. after the view is parked at the top, a second 200-line burst does **not**
   snap it back to the bottom.

`tests/test_ui10_sticky_scroll.cpp` (pure helpers, from PR #2) retained.

## Verification

- `rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4`
  — clean, no new warnings (3 pre-existing `gomocup_protocol.cpp` unused-function
  warnings, unrelated).
- `ctest --output-on-failure` — 3/3: `rapfi-gui-tests` (incl. 5 UI-10 helper
  cases), `rapfi-gui-ui-tests` (incl. 3 new UI-10 widget cases), `rel02-version-single-source`.
- Pre-fix teeth check: reverting `src/ui/bottom_panel.cpp` to HEAD fails all
  three new cases (`child` is a `GtkViewport`; `upper - page_size ≈ 0`; parked
  view value stays 0).
- Real app (`./build/rapfi-gui`) launches with no GTK warnings/criticals after
  the widget-tree change.
- Live-engine visual confirmation with `pbrain-rapfi` still not run in this
  session (the behavioural ui-tests drive the real BottomPanel + flush timer and
  cover the mechanism); worth an eyeball next GUI session.
