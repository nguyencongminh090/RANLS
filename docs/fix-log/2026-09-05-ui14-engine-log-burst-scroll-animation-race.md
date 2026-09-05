# UI-14: Engine Log stops following the tail after a SEND burst is immediately answered by a RECV burst

**Status:** ✅ FIXED

## Prompt

A user-supplied report (produced by a separate "Gemini Agent" systematic-debugging pass) claimed
that sending a burst of protocol lines (e.g. `sendConfig()`'s dozen or so `INFO`/`START` lines),
immediately followed by the engine's reply lines, leaves the Engine Log short of the bottom and
disables auto-scroll for the rest of the session — a further, more specific case than UI-10 (which
fixed the general "engine log doesn't follow the tail" bug) or UI-10's second/third passes (wrong
scroll-widget wrapper). The report's own diagnosis was produced entirely in an out-of-tree scratch
harness (`/home/ngmint/.gemini/.../scratch/*.cpp`), so it was verified from scratch against this
repo before any fix was made, per the `systematic-debugging` skill.

## Investigation

Read the current `BottomPanel`/`sticky_scroll.h` source first (`src/ui/bottom_panel.cpp:263-352`,
`src/ui/sticky_scroll.h`) — UI-10's fix (persistent end-of-buffer mark + remembered `stickToBottom_`
intent + `programmaticScroll_` guard around the flush) is already in place, so the question was
whether a further race survives it.

Added a new `TEST_CASE` in `tests/test_ui10_engine_log_scroll_target.cpp` reproducing "SEND burst,
pump 60ms (one flush tick), RECV burst, pump 600ms (full settle)". It **passed** against the
unmodified `ranls-gui-ui-tests` real-widget harness — a false negative. Root cause of the false
negative: `BottomPanel` is a `Gtk::Notebook`; a hidden Notebook page is never given a real GTK
allocation, so the Engine Log `ScrolledWindow`'s vadjustment `page_size` stayed `0.0` for the whole
test. With `page_size == 0`, `isAtBottom()` (`value + page_size >= upper - epsilon`) is satisfied by
`value == upper`, which every `insert()` already guarantees trivially — the existing UI-10 tests in
the same file have the identical gap and were passing vacuously, not because the scroll behavior is
correct. None of the other UI-10 test cases were touched to fix this (out of scope for this bug);
the new UI-14 case adds `panel->set_current_page(1)` before pumping so its own assertions are real.

With the Engine Log tab actually visible (`page_size` now the widget's real pixel height), a
standalone instrumented harness (`gtk_init`, a real `Gtk::Window`, the real `bottom_panel.cpp.o`
from the build tree, an `unsigned` connection to the vadjustment's own `value-changed`) reproduced
the bug deterministically: after `scrollEngineLogToBottom()` calls `engineLogView_.scroll_to(mark)`,
GTK4's `GtkScrolledWindow` answers with a genuine multi-frame *kinetic scroll animation* of the
vadjustment (observed: 8 `value_changed` events over ~150ms, e.g. `632→716→778→823→853→871→881→884→885`,
never reaching the true bottom at `1425`). `programmaticScroll_` — the guard meant to make the
vadjustment's `value_changed` handler ignore BottomPanel's own scroll — is cleared on the very next
`Glib::signal_idle()` tick (a few ms later), i.e. long before that animation settles. Every
subsequent animation frame therefore fires `value_changed` with the guard already down, and
`sticky_scroll::updateStickOnSettle()` reads the mid-flight geometry as "user scrolled away",
latching `stickToBottom_ = false` before the view ever reaches the real bottom. A second burst
landing in that ~150ms window (a `flushPending()` tick 50ms later, exactly the SEND→RECV timing
described in the report) then sees `stickToBottom_ == false` and skips its own auto-scroll,
matching the reported symptom.

**Root cause is X because Y** (Phase 3): the view under-scrolls because `programmaticScroll_`'s
guard window is shorter than GTK4's own scroll-to animation duration, letting the animation's
intermediate frames be misread as a user scroll. Confirmed by a single minimal test: pinning the
vadjustment's value directly (bypassing the animation) made the harness's `value_changed` sequence
collapse to one settle, and the burst-then-burst regression case passed on 5/5 runs.

## Fix

`src/ui/bottom_panel.cpp` (`scrollEngineLogToBottom()`, both the immediate call and the deferred-idle
re-issue): after the existing `engineLogView_.scroll_to(engineLogEndMark_, …)`, also directly
`vadj->set_value(upper - page_size)` on `scrolledEngineLog_`'s adjustment when that is greater than
the current value. This makes the jump to bottom immediate instead of animated, so there is no
multi-frame window for an intermediate frame to be misread as the user scrolling away.
`scroll_to(mark)` is kept (unchanged) as the mechanism that keeps the TextView's own internal
scroll-to-mark bookkeeping and the eventual real layout in sync; the direct `set_value` only removes
the animated *transition* to the target the mark scroll would eventually reach anyway.

No change to `sticky_scroll.h`'s decision logic, `programmaticScroll_`'s scope, the persistent-mark
mechanism, RT-02's batching/trim, or the Move Log path (`scrollMoveLogToEnd()` — UI-12, deliberately
out of scope, has its own known unrelated scroll-timing flake, see below).

## Tests

New `TEST_CASE("UI-14: a SEND burst immediately followed by a RECV burst still ends pinned to the
bottom")` in `tests/test_ui10_engine_log_scroll_target.cpp` — real-widget, `ranls-gui-ui-tests`
target: switches to the Engine Log tab (`set_current_page(1)`, needed for a real, non-zero
`page_size` — see Investigation), sends a 60-line SEND burst, pumps only 60ms (one flush tick,
deliberately shorter than the observed ~150ms scroll-animation window), sends a 30-line RECV burst,
pumps 600ms to fully settle, then asserts the vadjustment is within 4px of true bottom.
- **Before the fix:** fails deterministically, 5/5 runs (`885 >= 1421` false; final position ~540px
  short of bottom).
- **After the fix:** passes, 5/5 runs.

Full-suite verification: `ranls-gui-tests` (model/engine, no gtkmm) 194/194 cases, 2392/2392
assertions, clean. `ranls-gui-ui-tests` 23/25 cases pass; the 2 failures
(`ANLZ-05: Analyze Mode blocks auto-move…` and `UI-12: appended moves keep the Move Log scrolled to
the bottom`) were reproduced identically with the fix reverted (`git stash` on
`src/ui/bottom_panel.cpp` + the new test) — both are pre-existing, unrelated flakes (UI-12's is the
same one called out as already-known in the ANLZ-07 fix-log entry; ANLZ-05 depends on a live wire
race with no engine binary on this build host), not a regression from this change.

No live-engine manual smoke (no engine binary on this build host, same limitation noted in several
prior UI-10/UI-12/ANLZ entries).
