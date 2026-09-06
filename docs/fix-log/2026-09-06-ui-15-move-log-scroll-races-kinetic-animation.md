# UI-15: Move Log sticky-bottom races the GTK4 kinetic-scroll animation (the recurring "ui12 scroll flake")

**Status:** ✅ FIXED

## Prompt

Tracked task UI-15 (Sprint 15 Active). `test_ui12_move_log_scroll_target.cpp` line 136
(`CHECK(value >= maxValue - 4.0)`) has been cited as a "pre-existing UI-12 scroll flake" in ~5
fix-log entries (UI-14, ENG-03, ANLZ-07, PORT-02, PORT-03). PORT-03 `sigc::track_obj`-bound the
`BottomPanel` scroll-idle slots and fixed the intermittent *crash*, but not the *assertion* flake.
`/systematic-debugging` phases 0–3 were already completed in
`docs/todo/UI-15-move-log-scroll-races-kinetic-animation.md`; this entry records phase 5–6 (fix at
source + regression test).

## Investigation (phases 0–3, from the todo detail file)

`BottomPanel::scrollEngineLogToBottom()` and `BottomPanel::scrollMoveLogToEnd()` had diverged. UI-14
fixed the Engine Log by having `scrollEngineLogToBottom()` snap the vadjustment directly
(`vadj->set_value(vadj->get_upper() - vadj->get_page_size())`) — both immediately and on the deferred
idle, *in addition to* `scroll_to(mark)` — because GTK4's `GtkScrolledWindow` answers `scroll_to`
with a multi-frame kinetic animation (tens–150 ms) rather than an instant jump.

`scrollMoveLogToEnd()` never got that treatment: it still called only
`moveLogView_.scroll_to(moveLogEndMark_, 0.0, 0.0, 1.0)` (immediate + one deferred-idle re-issue). So
after a fast burst append the vadjustment is still mid-animation, and whether `value` has reached
`maxValue - 4.0` by the test's fixed `pump(300)` depends on frame-clock scheduling — hence the
load-dependent flake. It is a real product defect: on a real machine a fast game replay / paste can
leave the Move Log a few px short of the newest move.

Reproduced: `test_ui12_move_log_scroll_target` ("appended moves keep the Move Log scrolled to the
bottom") fails when the direct binary is run under `$(nproc)`-wide CPU load; passes under `ctest` and
on an idle machine.

## Fix

`src/ui/bottom_panel.cpp` — `scrollMoveLogToEnd()` now mirrors `scrollEngineLogToBottom()`. After
both the immediate `moveLogView_.scroll_to(moveLogEndMark_, 0.0, 0.0, 1.0)` and the `sigc::track_obj`
idle re-issue, it snaps the adjustment directly:

```cpp
if (auto vadj = scrolledMoveLog_.get_vadjustment()) {
    const double maxValue = vadj->get_upper() - vadj->get_page_size();
    if (maxValue > vadj->get_value())
        vadj->set_value(maxValue);
}
```

This makes the jump to bottom immediate instead of animated, closing the multi-frame window. The
persistent right-gravity `moveLogEndMark_` and the `track_obj`-bound `moveLogScrollIdlePending_` idle
are unchanged. Per the todo's hard-constraint scope item 3, nothing was changed in
`programmaticScroll_` / `stickToBottom_` semantics, the Engine Log path, the RT-02 buffer cap, or the
UI-05 gutter; `BottomPanel::appendMoveLog`'s deliberate "always follow to bottom" (no
remembered-intent for the Move Log, per UI-12) is untouched — so there is no "is the user at the
bottom" check here, matching `appendMoveLog`'s existing behaviour.

## Tests

`tests/test_ui12_move_log_scroll_target.cpp` — the fixed `pump(300)` + `CHECK(value >= maxValue - 4.0)`
is replaced with a condition-based wait (`systematic-debugging/condition-based-waiting.md`): pump in
10 ms steps until the vadjustment is within `epsilon` (4.0) of bottom *or* a generous 3000 ms
deadline elapses, then assert on the settled state (`REQUIRE(maxValue > 10.0)` +
`CHECK(value >= maxValue - epsilon)`). The case is kept as the permanent regression guard — it is
not deleted. Both UI-12 cases in the file otherwise unchanged.

## Verification

- `./build.sh` — clean from-scratch build, no new warnings; `src/ui/bottom_panel.cpp` recompiled
  alone, no warnings.
- `build_cmd/tests/ranls-gui-ui-tests --test-case="UI-12: appended moves keep the Move Log scrolled
  to the bottom"`: **20/20** consecutive passes while `$(nproc)` `yes` processes saturated every
  core; **5/5** `ctest -R ranls-gui-ui-tests` runs under the same load; passes under a plain `ctest`.
- `build_cmd/tests/ranls-gui-ui-tests` direct: **30/30** cases, 198 assertions
  (`test_anlz05_no_automove_action` passed this run too — no regression against the known-flaky
  baseline).
- `build_cmd/tests/ranls-gui-tests` direct: **209/209** cases, 2466 assertions.
- `ctest --test-dir build_cmd`: **4/4** — `port02-style-css-bundled`, `ranls-gui-tests`,
  `rel02-version-single-source`, `ranls-gui-ui-tests` all pass.
- `node scripts/check-task-structure.js` and `node scripts/check-tracking-sync.js` both pass.
- Manual "replay a long saved game, Move Log ends on the final move" smoke not run — no display
  server on the build host; substituted by the condition-based regression test above.
