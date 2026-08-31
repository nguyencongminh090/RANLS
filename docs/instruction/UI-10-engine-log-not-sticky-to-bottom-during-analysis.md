# UI-10 — engine-log-not-sticky-to-bottom-during-analysis (execution guidance)

This is a **bug fix**, not a feature. Run the `systematic-debugging` pipeline
(`CLAUDE.md` "Diagnosis pipeline") *before* changing any code. The todo detail
file (`docs/todo/UI-10-engine-log-not-sticky-to-bottom-during-analysis.md`)
already lists three suspect sites and a timing hypothesis — **confirm which one
actually fires with instrumentation before picking a fix. Do not assume.**

## Diagnosis first (phases 1-4)

1. **Reproduce.** Launch via the `run` skill, attach a real engine, start
   analysis on a position that streams many `MESSAGE depth …` lines fast. With
   the log already scrolled to the bottom, confirm the newest line drifts
   off-screen. Note board size / engine / whether it needs a fast stream.
2. **Instrument, run once, read data** — add temporary logging in
   `BottomPanel::flushPending()` (`src/ui/bottom_panel.cpp:215`):
   - value of `wasAtBottom` each tick (does a genuinely-bottomed view read
     `false` mid-stream?);
   - `adj->get_value() / get_page_size() / get_upper()` before *and* after the
     `buf->insert` loop (is `upper` stale when `isScrolledToBottom()` runs?);
   - whether `scrollToEnd()` is reached but lands short (compare `adj` after).
3. **Locate the real cause** among:
   - `isScrolledToBottom()` (`:207`) reading a stale `upper` → false negative →
     scroll suppressed;
   - `scrollToEnd()` (`:197`) creating a mark, calling `scroll_to`, then
     **immediately `delete_mark`** — GTK's pending-scroll may hold that mark;
     deleting it can drop the queued scroll, or the scroll runs against a
     not-yet-relaid-out TextView height and lands short;
   - batched insert in `flushPending` racing the layout pass.
4. **Hypothesis + minimal probe.** State "root cause is X because Y"; make the
   smallest change that confirms/kills it (e.g. a persistent member bottom-mark
   that is never deleted; or deferring the scroll to a
   `signal_size_allocate` / `Glib::signal_idle` one-shot; or recomputing
   at-bottom from the buffer/line geometry instead of the possibly-stale
   adjustment). One variable at a time.

## Fix (phase 5) — approach, once the cause is confirmed

- Fix at the source, one change. Likely shape: keep the sticky-bottom **intent**
  (`wasAtBottom` gate) but make the scroll reliable — a persistent
  `Glib::RefPtr<Gtk::TextBuffer::Mark>` at end-of-buffer (left_gravity=false,
  created once) that `flushPending` scrolls to *after* layout settles, rather
  than a create-scroll-delete triplet each tick.
- The at-bottom detection must survive a stale adjustment: prefer deriving it
  from `engineLogView_.get_iter_location(buf->end())` / visible-rect geometry, or
  re-check on the idle callback, rather than trusting `adj->get_upper()` on the
  same tick the batch was inserted.

## Pitfalls / boundaries — do NOT touch

- **Do not remove or weaken the "only stick if already at bottom" gate.** A user
  who scrolled up to read history must never be yanked back down (RT-02 behaviour
  / acceptance criterion 2). If you add an idle/deferred callback, re-capture or
  carry the `wasAtBottom` decision from *before* the insert — do not re-evaluate
  "is the user at the bottom" after you have already appended.
- **Do not touch the bounded-buffer trimming** (`totalDropped` / `engineLogModel_`
  cap) — RT-02. The front-trim and the model cap must stay in lockstep.
- **Do not touch the gutter column** (`drawGutter`, `logLineClipboardText`,
  `EngineLogLine.prefix`) — UI-05. Payload-only insertion stays as is.
- **Do not change the flush cadence / timer** (RT-01/RT-02 throttle). This is a
  scroll-position bug, not a throughput bug.
- Scope strictly to the Engine Log. The Move Log TextView shares `scrollToEnd()`
  — if you change that helper's signature/behaviour, verify the Move Log path
  still behaves (or give the Engine Log its own path). Call out anything beyond
  the reported bug as a separate Backlog item, don't fold it in.
- No new dependency on engine/`GameState` — this is view-local.

## Regression test

The scroll mechanics need a realized `Gtk::TextView` (display server) and can't
run in `rapfi-gui-tests` (no gtkmm) — say so if you can't get real coverage.
Options, best first:
- Extract the **sticky-scroll decision** into a pure helper (e.g.
  `bool shouldStickToBottom(double value, double pageSize, double upper, double eps)`
  plus whatever geometry input the fix ends up needing) in a header, and unit
  test it in `rapfi-gui-tests` with a new `tests/test_ui10_sticky_scroll.cpp`
  (add to `tests/CMakeLists.txt` `rapfi-gui-tests` sources, mirror the existing
  `test_ui05_*` style). Cover: at-bottom within epsilon → true; scrolled up →
  false; stale-`upper` case the fix addresses.
- If the fix is purely in GTK scroll plumbing with no extractable logic, add a
  headless case to `rapfi-gui-ui-tests` only if a TextView can be realized
  offscreen there; otherwise document the manual verification and why automated
  coverage isn't feasible.
- Never discard the test after writing it.

## Verification before marking this task done

Run and confirm **all** of these yourself:
1. `rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4`
   — clean build, no new warnings.
2. `cd build && ctest --output-on-failure` — **both** suites green
   (`rapfi-gui-tests` and `rapfi-gui-ui-tests`), plus `rel02-version-single-source`,
   plus the new UI-10 test.
3. Manual, via the `run` skill with a real engine:
   - bottomed log + fast streaming analysis → newest line stays pinned to the
     bottom edge the whole time;
   - scroll up mid-stream → view stays where the user put it, is NOT pulled down;
   - scroll back to the bottom → stickiness resumes;
   - buffer still caps (no unbounded growth), gutter tags still aligned.
   Report exactly what you observed for each.
4. Remove all temporary instrumentation added during diagnosis.

## Records

- `docs/fix-log.md` row + `docs/fix-log/<date>-engine-log-sticky-bottom.md` detail
  (Prompt / Investigation / Root cause / Fix / Verification).
- Flip `docs/todo/UI-10-*.md` `**Status:**` to `✅ FIXED` with summary + which of
  the three suspects was the real cause; flip the `TODO.md` Active line to `✅`
  (leave it in Active — orchestrator moves it post-merge).

[detail is this file]
