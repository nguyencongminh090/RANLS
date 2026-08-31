# 2026-08-31 — Engine Log not sticky to bottom during streaming analysis (UI-10)

## Prompt

Tracked task UI-10: while the engine analyses and new lines stream into the Engine
Log, the view does not follow the newest line — it lags behind the true bottom so
the latest output is off-screen. Expected: the Engine Log stays pinned to the end
during analysis, *without* yanking a user who has scrolled up to read history.

## Investigation

Followed the `systematic-debugging` pipeline. No Gomocup engine binary is available
on this machine, so reproduction/instrumentation of a live stream was not possible;
the diagnosis is code + GTK4-semantics reasoning (confidence: high on the primary
cause).

`BottomPanel::flushPending()` runs on a 50 ms timer, captures
`wasAtBottom = isScrolledToBottom()` before inserting the batch, inserts, then
calls `scrollToEnd(engineLogView_)` only if `wasAtBottom`. Three suspects were
listed in the todo:

1. `isScrolledToBottom()` reading a stale adjustment `upper` → false negative.
2. `scrollToEnd()` creating a `Mark`, calling `scroll_to`, then **immediately
   `delete_mark`**.
3. The batched insert racing the layout pass.

Tracing backward from the symptom (newest line off-screen):

- GTK4 `gtk_text_view_scroll_to_mark` **defers** the scroll to the next
  size-allocate whenever the TextView's height isn't validated yet. Right after a
  batched multi-line insert during a fast stream, it never is — so the scroll is
  always queued, not immediate. The queued scroll is keyed on the passed mark.
  `scrollToEnd()` deletes that mark on the very next line, so when the deferred
  pass runs the target is gone and the scroll is dropped. The view only stayed
  pinned on ticks where layout happened to already be current (stream pauses).
  → **suspect #2 is the primary cause.**
- Secondary: after one short/dropped scroll, `value` sits below the new content
  bottom. Next tick `isScrolledToBottom()` computes `value + page_size >= upper − 1`
  against an `upper` that has since grown, reads `false`, and `wasAtBottom` latches
  off — auto-scroll then stays disabled for the rest of the run.
  → **suspect #1 compounds it**, converting an intermittent lag into a permanent one.
- Suspect #3 is a real race but a stable scroll target makes it moot; it is a
  symptom of #2, not an independent cause.

## Root cause

`scrollToEnd()`'s create-scroll-`delete_mark` triplet destroyed GTK's deferred-scroll
target before the post-relayout scroll pass could use it (primary), and
`isScrolledToBottom()` trusting a single mid-stream adjustment sample let a stale
"not at bottom" reading permanently disable stickiness (compounding).

## Fix

`src/ui/bottom_panel.{h,cpp}` + new `src/ui/sticky_scroll.h`:

- **Persistent end mark.** `engineLogEndMark_` (right-gravity) is created once in the
  ctor and never deleted. `scrollEngineLogToBottom()` scrolls to it and re-issues
  the identical `scroll_to` once on the next `Glib::signal_idle` (guarded by
  `scrollIdlePending_`), so GTK's deferred scroll always has a live target and a
  short landing is corrected after the relayout settles. The Move Log keeps the
  original `scrollToEnd()` helper untouched.
- **Remembered stick intent.** `stickToBottom_` starts `true`. `flushPending` calls
  the pure `sticky_scroll::shouldStickToBottom(stickToBottom_, preAppendGeometry,
  eps)` — the intent, OR a genuine at-bottom read; a lone stale "not at bottom"
  read can no longer turn stickiness off. The intent flips in exactly one place:
  the vadjustment `value_changed` handler
  (`sticky_scroll::updateStickOnSettle`), where the scroll has actually settled and
  the geometry is trustworthy. User scrolls up → intent cleared (not yanked back);
  user (or our own auto-scroll) returns to the bottom → intent restored.
  `clearEngineLog()` resets it to `true`.
- Sticky-bottom-**only** gate preserved: the idle re-scroll carries the pre-insert
  decision, never re-evaluates "is the user at the bottom".

Out of scope (untouched, per the instruction boundaries): bounded-buffer trimming
(`totalDropped` / `engineLogModel_` cap, RT-02), the gutter column (`drawGutter`,
`logLineClipboardText`, UI-05), the flush cadence/timer (RT-01/RT-02), the Move Log
scroll path.

## Verification

- `rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4`
  — clean, no new warnings.
- `cd build && ctest --output-on-failure` — 3/3 pass: `rapfi-gui-tests` (with 5 new
  UI-10 cases, 12 assertions), `rapfi-gui-ui-tests`, `rel02-version-single-source`.
- New regression test `tests/test_ui10_sticky_scroll.cpp` (added to the
  `rapfi-gui-tests` target) covers the pure decision helper: at-bottom within
  epsilon → stick; scrolled up → no stick; **stale-`upper` mid-stream → remembered
  intent keeps it stuck** (the regression case); `updateStickOnSettle` as the sole
  intent-flip point.
- Manual live-engine streaming test **not run** — no Gomocup/Yixin engine binary on
  this machine. `./build/rapfi-gui` launches and builds its widget tree cleanly
  with the new mark/idle wiring (no crash, no GTK warnings). The scroll-plumbing
  half of the fix (persistent mark + deferred re-scroll) therefore rests on GTK4
  API semantics, not observed behaviour; the decision half is unit-tested.
