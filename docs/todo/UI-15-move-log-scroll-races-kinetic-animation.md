# UI-15 — Move Log sticky-bottom races the GTK4 kinetic-scroll animation (the recurring "ui12 scroll flake")

**Status:** 🔲 OPEN (Active — Sprint 15) — filed 2026-09-06; pulled from Backlog into Sprint 15 Active mid-sprint 2026-09-06

## Source

`test_ui12_move_log_scroll_target.cpp` line 136 (`CHECK(value >= maxValue - 4.0)`) has been cited as
a "pre-existing UI-12 scroll flake" in ~5 fix-log entries (UI-14, ENG-03, ANLZ-07, PORT-02, PORT-03).
PORT-03 attributed it to a `BottomPanel` scroll-idle use-after-free and `sigc::track_obj`-bound the
idle slots, claiming that "also resolves the 'ui12 scroll' flake". It did fix the intermittent
*crash*; it did **not** fix the *assertion* flake. Observed 2026-09-06: `test_ui12` fails 4/4 when
run directly under CPU load, passes under `ctest` and passes directly on an idle machine — a timing
race, not a deterministic failure. Reproduces identically on `44fc877` (before this session's other
fixes), so it is genuinely pre-existing and unrelated to PR #27 / #28.

## Root cause (`/systematic-debugging` phases 1–3 done)

`BottomPanel::scrollEngineLogToBottom()` and `BottomPanel::scrollMoveLogToEnd()` diverged.

UI-14 fixed the **Engine Log** by making `scrollEngineLogToBottom()` snap the adjustment directly —
`vadj->set_value(vadj->get_upper() - vadj->get_page_size())` — both immediately and on the deferred
idle, *in addition to* `scroll_to(mark)`, because GTK4's `GtkScrolledWindow` answers `scroll_to`
with a multi-frame kinetic animation (tens–150 ms) rather than an instant jump.

`scrollMoveLogToEnd()` ([bottom_panel.cpp:234](../../src/ui/bottom_panel.cpp)) never got that
treatment — it still calls **only** `moveLogView_.scroll_to(moveLogEndMark_, 0.0, 0.0, 1.0)`
(immediate + one deferred-idle re-issue). So after a 400-move burst append the vadjustment is still
mid-animation; whether `value` has reached `maxValue - 4.0` by the test's `pump(300)` depends on
frame-clock scheduling, hence the load-dependent flake. This is a real product defect, not only a
test-timing artefact: on a real machine a fast game replay / paste can leave the Move Log a few px
short of the newest move.

## Scope

1. **`src/ui/bottom_panel.cpp` `scrollMoveLogToEnd()`** — mirror `scrollEngineLogToBottom()`: after
   each `scroll_to(moveLogEndMark_, …)` (the immediate call and the `track_obj` idle re-issue), add
   `if (auto vadj = scrolledMoveLog_.get_vadjustment()) { double m = vadj->get_upper() - vadj->get_page_size(); if (m > vadj->get_value()) vadj->set_value(m); }`.
   Keep the persistent mark + `track_obj` idle exactly as they are.
2. **`tests/test_ui12_move_log_scroll_target.cpp`** — replace the fixed `pump(300)` +
   `CHECK(value >= maxValue - 4.0)` with a condition-based wait
   (`systematic-debugging/condition-based-waiting.md`): pump until `value >= maxValue - epsilon`
   *or* a generous deadline, then assert. Never discard the case.
3. Do **not** touch `programmaticScroll_` / `stickToBottom_` semantics, the Engine Log path, RT-02
   buffer cap, or UI-05 gutter. `BottomPanel::appendMoveLog` "always follow to bottom" (no
   remembered-intent for the Move Log) is deliberate per UI-12 — unchanged.

## Acceptance criteria

- `test_ui12_move_log_scroll_target` passes 20/20 direct-binary runs under `make -j$(nproc)` load
  *and* under `ctest`.
- `ranls-gui-ui-tests` otherwise unchanged; `ranls-gui-tests` + `port02` + `rel02` unaffected.
- Manual: replay a long saved game — Move Log ends on the final move, no residual gap.

## Notes

- Pairs with a broader question (not in scope here): every `scroll_to`-based sticky-bottom in the
  codebase has this GTK4 kinetic-animation hazard; UI-14 + this fix handle the two logs, but a
  shared helper would stop the next one recurring. File separately if wanted.
