# UI-10 — Engine Log doesn't stay scrolled to the end while the engine is analysing

**Status:** 🔲 ACTIVE (Sprint 8, pulled 2026-08-31)
**Area:** bottom panel / Engine Log (`src/ui/bottom_panel.cpp`)
**Priority:** P2
**Source:** filed 2026-08-31 from a user bug report

## Problem

While the engine is analysing and new lines stream into the Engine Log, the view
does not follow the newest line — it lags behind the true bottom, so the latest
output is off-screen. Expected: the Engine Log always shows the end lines during
analysis (auto-scroll / "stick to bottom").

## Where to look

- `BottomPanel::flushPending()` (`src/ui/bottom_panel.cpp:215`) — captures
  `wasAtBottom = isScrolledToBottom()` *before* inserting the batch, then calls
  `scrollToEnd(engineLogView_)` only `if (wasAtBottom)`. The sticky-bottom intent
  is already there.
- `BottomPanel::isScrolledToBottom()` (`:207`) — compares
  `value + page_size >= upper - 1px` on the *current* vadjustment. During a fast
  stream the adjustment `upper` may not yet reflect the just-appended lines when
  this runs on the next tick, so a genuinely-at-bottom view can read as
  "not at bottom" and auto-scroll is suppressed.
- `BottomPanel::scrollToEnd()` (`:197`) — creates a mark at `buf->end()` and
  `scroll_to(..., yalign=1.0)` synchronously. GTK may not have re-laid-out the
  TextView height yet, so the scroll can land short of the real end. A common fix
  is to defer the scroll to an idle/`size-allocate` handler, or use
  `scroll_to_mark` after the layout settles.

## Hypothesis (to confirm before fixing)

Timing: the at-bottom check and/or the scroll run against a stale text-layout
height during rapid batched appends, so `flushPending()` either skips the scroll
or scrolls short. Needs measurement (does `wasAtBottom` go false mid-stream? does
a deferred scroll fix it?) — do not assume the fix.

## Acceptance criteria

- With the Engine Log scrolled to the bottom, streaming analysis output keeps the
  newest line visible at the bottom edge.
- A user who has scrolled *up* to read history is still NOT yanked back to the
  bottom (keep the existing sticky-bottom-only behaviour — RT-02 / current
  `isScrolledToBottom` gate).
- Buffer stays bounded (RT-02); gutter stays in sync (UI-05).

## Related

- RT-02 (engine log unbounded + batched per-tick flush) — same `flushPending` path.
- UI-05 (direction-tag gutter) — same widget.
