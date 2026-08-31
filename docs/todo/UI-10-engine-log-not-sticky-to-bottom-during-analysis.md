# UI-10 — Engine Log doesn't stay scrolled to the end while the engine is analysing

**Status:** ✅ FIXED (2026-08-31 — see the correction note directly below; the
first pass, PR #2, did not actually fix it)

## Correction — 2026-08-31 (PR: `ui-10/engine-log-scrolls-wrong-widget`)

User retested PR #2 and reported the Engine Log **still shows the first line,
not the last**, during analysis. The first pass never had live-engine
verification and its regression test only covered the pure `sticky_scroll`
helpers, so a wrong assumption slipped through.

**Actual root cause:** the Engine Log `TextView` was not the `Gtk::ScrolledWindow`'s
direct child — `BottomPanel` wrapped it in `EmptyStateOverlay` (a `Gtk::Overlay`,
a no-op passthrough since UI-08). `Gtk::Overlay` is not `Gtk::Scrollable`, so
`Gtk::ScrolledWindow` silently interposes a `GtkViewport`; the Viewport scrolls
while the `TextView` sits at full height inside it, making **every
`engineLogView_.scroll_to(...)` a no-op** — including the persistent-mark call
PR #2 added. `isScrolledToBottom()` always read "at bottom" (the viewport-side
adjustment barely moved) so nothing ever forced a scroll either.

**Fix (this pass):**
- `scrolledEngineLog_.set_child(engineLogView_)` directly — the `TextView`
  implements `Gtk::Scrollable`, so `scroll_to(mark, 0,0,1.0)` now actually moves
  the view. `engineLogOverlay_` stays a member only so the (no-op)
  `updateEngineLogEmptyState()` call site compiles.
- `value_changed` no longer re-derives the "follow the tail" intent while a new
  `programmaticScroll_` guard is set (across `flushPending`'s buffer mutation +
  RT-02 front-trim + auto-scroll), so a mid-flush transient can't latch
  stickiness permanently off now that the adjustment genuinely moves.
- Regression tests upgraded from pure-helper only to **real-widget behavioural**
  cases in `rapfi-gui-ui-tests` (`tests/test_ui10_engine_log_scroll_target.cpp`):
  TextView is the scroller's direct child; 400 streamed lines leave the view
  pinned to the bottom; a user scrolled to the top is not yanked down by new
  lines. All three fail on the pre-fix tree, pass after.

**Known follow-up (not in scope here):** the Move Log has the identical
structure (`scrolledMoveLog_` wraps `moveLogView_` in `moveLogOverlay_`), so
`scrollToEnd(moveLogView_)` is also a silent no-op — the Move Log does not
auto-scroll to the newest move. Not reported, left for a separate `CODE`.

---

_First-pass notes (PR #2) retained below for history — its stated root cause was
incomplete._

Fixed on branch — the orchestrator moves this line to Done post-merge.

**Real cause (of the three listed suspects): a combination of #2 (primary) and #1
(compounding).**

- **#2 — `scrollToEnd()`'s create-scroll-`delete_mark` triplet (primary).** Each
  flush tick created a fresh end-of-buffer `Mark`, called `view.scroll_to(mark,…)`,
  then **deleted the mark on the same line**. GTK4's `scroll_to_mark` defers the
  actual scroll to after the next size-allocate/relayout when the TextView height
  isn't yet validated (always true right after a batched insert during a fast
  stream); the pending scroll is keyed on that mark, and deleting it before the
  deferred pass runs drops the queued scroll. The view only caught up on the odd
  tick where layout happened to be current, so it lagged behind the newest line.
- **#1 — stale `upper` in `isScrolledToBottom()` (compounding).** Once lagging, the
  next tick's `value + page_size >= upper − 1px` check read against an `upper` that
  had already grown past the last `value` the (short) scroll left behind, so
  `wasAtBottom` latched `false` and auto-scroll switched itself off entirely —
  turning an intermittent lag into a permanent one.
- #3 (insert racing the layout pass) is real but is a symptom of #2, not a separate
  cause — a stable scroll target survives the race.

## Fix

- New persistent right-gravity `engineLogEndMark_`, created once in the ctor, never
  deleted. `flushPending` scrolls to it and re-issues the same `scroll_to` once on
  the next `Glib::signal_idle` (after GTK's relayout), so the deferred scroll
  always has a live target. Move Log keeps the old `scrollToEnd()` helper unchanged.
- New persisted `stickToBottom_` intent (starts `true`). `flushPending` decides via
  the pure `sticky_scroll::shouldStickToBottom(remembered, preAppendGeometry, eps)`
  — a lone stale "not at bottom" read on the flush tick can no longer disable
  stickiness. The intent is flipped in exactly one place: the vadjustment
  `value_changed` handler (`updateStickOnSettle`), where the geometry is settled
  and trustworthy — user scrolls up ⇒ stop sticking; user returns to bottom (or our
  auto-scroll lands there) ⇒ resume. `clearEngineLog()` resets it to `true`.
- Sticky-bottom-only gate preserved: the deferred idle re-scroll carries the
  pre-insert decision and never re-evaluates at-bottom.

## Verification

- `rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4`
  — clean, no new warnings.
- `ctest --output-on-failure` — 3/3: `rapfi-gui-tests` (incl. 5 new UI-10 cases),
  `rapfi-gui-ui-tests`, `rel02-version-single-source`.
- New `tests/test_ui10_sticky_scroll.cpp` (pure `sticky_scroll` helper): at-bottom ⇒
  stick; scrolled-up ⇒ no stick; **stale-`upper` mid-stream ⇒ intent keeps it
  stuck**; settle handler is the sole flip point.
- Manual live-engine streaming test **NOT run** — no Gomocup engine binary
  available on this machine (`pbrain-rapfi`/`rapfi` not on PATH). GUI launches
  cleanly with the new wiring (no crash, no GTK warnings). Diagnosis is
  code + GTK4-semantics reasoning; confidence high on suspect #2 as primary.

**Original task description follows.**
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
