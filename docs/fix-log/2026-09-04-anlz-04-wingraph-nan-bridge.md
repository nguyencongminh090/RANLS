# 2026-09-04 — ANLZ-04: WinGraph bridges interior NaN gaps with a faint dashed connector

Tracked work (`docs/todo/ANLZ-04-wingraph-bridge-nan-gaps.md`), a rendering refinement rather than
a bug fix — the design rationale and the UI-01 rule change are in
`docs/audit/2026-09-04-wingraph-nan-bridge.md`. This fix-log row records the code change and its
regression test, per the todo/instruction files (CLAUDE.md "every fix carries a regression test").

## Prompt

`/implement-task ANLZ-04` — with ANLZ-01 shipped the WinGraph is mostly continuous, but any
residual NaN ply (a ply played without a search on it) still lifts the pen in `onDraw` and
fragments the trace into disconnected segments. Connect across such a gap with a visually
subordinate dashed bridge; keep every other UI-01 guarantee.

## Action

- **New pure helper** `computeGapBridges(const std::vector<double>&)` in `src/ui/win_graph_bridge.h`
  — returns one `{fromIdx, toIdx}` pair per *interior* NaN run (last real index before, first real
  index after). Leading/trailing runs (no anchor on one side) and `n == 0/1` / all-NaN produce
  none. Split out so it is unit-testable without gtkmm, mirroring the UX-06 `buildWinGraphSeries`
  split.
- **`WinGraphView::onDraw`** (`src/ui/win_graph_view.cpp`) — two-pass structure per series:
  - **Pass 1** (new): for each `computeGapBridges()` pair, draw one connector from the last real
    point to the first real point after the gap. Style: `set_source_rgba(kBlack*/kWhite*, 0.4)`
    (series colour, not grey), `set_line_width(kSeriesW * 0.6 / kSeriesWWhite * 0.6)`,
    `set_dash({4.0, 3.0}, 0.0)` — distinct in pitch from the UI-09 White dash `{6.0, 4.0}`. All
    connectors accumulated into one path, one `stroke()`, then `unset_dash()` — dash state is never
    set mid-path (Cairo applies it at `stroke()` time).
  - **Pass 2**: the pre-existing solid-run loop, byte-for-byte unchanged (solid, full width, full
    alpha). Applied to `blackData_` and, under `WinGraphMode::BothSide` with the existing size
    guard, `whiteData_`.
- The current-move-dot guard (`!std::isnan(blackData_[currentIndex_])`) and the hover `(no eval)`
  branch are untouched — a bridged ply is still unevaluated for every purpose except the line.
- Nothing written back to the series data; no 0.5 synthesised.

**Not touched:** `buildWinGraphSeries`, the `evalHistory` NaN sentinel, eval→win% maths, UI-01
attribution, UI-09 series colour/weight, RT-01 cadence, axes / Y labels / 50%-line / current-move
highlight / hover-box layout. No `ViewConfig` flag, Settings entry, or gap-length cap.

## Verification

1. `./build.sh` — clean; only the 3 pre-existing `-Wunused-function` warnings in
   `src/engine/gomocup_protocol.cpp` (`parseBoolToken`, `signedIntText`, `evalDisplayText`).
2. `ctest --test-dir build_cmd` — 3/3: `ranls-gui-tests`, `rel02-version-single-source`,
   `ranls-gui-ui-tests` all green.
3. **New** `tests/test_anlz04_wingraph_bridge.cpp` (8 cases / 21 assertions, wired into
   `ranls-gui-tests`) — pins `computeGapBridges`: `[0.5, 0.6, NaN, NaN, 0.55]` → one pair `{1,4}`;
   single interior NaN → bridged; consecutive reals → none; leading-NaN run → none; trailing-NaN
   run → none; leading+interior+trailing together → only the interior pair; two interior gaps → one
   pair each; `{}` / `{0.5}` / `{NaN}` / all-NaN → none, no crash.
4. **Manual smoke not possible** (no display/engine on the build host). Substituted: the helper unit
   test confirms interior gaps produce exactly one bridge pair spanning the correct real indices and
   that leading/trailing gaps produce none — which is the decision the Cairo pass-1 loop consumes
   verbatim. The dashed/faint styling itself is constant Cairo state (`set_dash` / `set_source_rgba`
   / `set_line_width` literals) and not data-dependent.
5. `docs/audit.md` row + `docs/audit/2026-09-04-wingraph-nan-bridge.md` written (UI-01 refinement).

## Files

- `src/ui/win_graph_bridge.h` (new)
- `src/ui/win_graph_view.cpp`
- `tests/test_anlz04_wingraph_bridge.cpp` (new), `tests/CMakeLists.txt`
- `docs/audit/2026-09-04-wingraph-nan-bridge.md` (new), `docs/audit.md`
- `docs/todo/ANLZ-04-wingraph-bridge-nan-gaps.md`, `TODO.md`
