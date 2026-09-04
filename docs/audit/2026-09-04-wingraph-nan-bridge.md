# 2026-09-04 — WinGraph NaN-gap bridge: refinement of UI-01's "disjoint segments" rule

## Decision

UI-01 established that an unevaluated position (the `std::numeric_limits<double>::quiet_NaN()`
sentinel in `GameState::evalHistory`) must **never** be interpolated through or plotted as a
confident 50%. Its rendering consequence in `WinGraphView::onDraw` was: lift the pen at every NaN,
splitting the polyline into disjoint segments around any NaN run.

As of ANLZ-04 that rendering rule is **softened**:

- Where an *interior* NaN run separates two evaluated plies, `onDraw` now draws **one** connecting
  segment from the last real point before the run to the first real point after it, styled as a
  visually *subordinate* dashed bridge — dash `{4.0, 3.0}` (distinct in pitch from the UI-09 White
  series dash `{6.0, 4.0}`), the series colour at **0.4 alpha** (not grey — grey would read as an
  axis/guide, not "this line, uncertain"), line width `kSeries*W * 0.6` (thinner than the solid
  run).
- Everything else UI-01 guaranteed is **unchanged**: the bridged ply still gets **no dot** (the
  `!std::isnan(blackData_[currentIndex_])` current-move-dot guard is untouched), hover on it still
  reads `(no eval)`, and **no 0.5 / 50% value is ever synthesised** — the bridge is a straight line
  between two *real* points, drawn by the renderer, with nothing written back into the data.
- Leading / trailing NaN runs have no anchor on one side and are **not** bridged — the line still
  simply starts/ends at the first/last evaluated ply.

## Why

ANLZ-01 ("Analyze Mode") fills a real, measured point for every position the user *analyses*, so
the graph is now mostly continuous. But a ply that was played without a search landing on it
(engine not running, Analyze Mode off at the time, an interrupted search, or the engine's own turn
under "Engine plays") still carries the NaN sentinel. With the old rule, one such residual interior
gap fragmented the whole trace into disconnected pieces ("discontinued"), which the user found
harder to read than a single continuous line with a visibly weaker gap-spanning portion.

The bridge communicates "these two evaluated plies are still part of one trace; the span between
them was not measured" without ever fabricating data. It is deliberately weaker than both the solid
series and the UI-09 dashed White line (different dash pitch **and** lower alpha **and** thinner
width) so it can never be mistaken for real data or for the other series.

## Scope of the change

- Pure rendering change in `WinGraphView::onDraw` (`src/ui/win_graph_view.cpp`), plus a new pure
  helper `computeGapBridges()` in `src/ui/win_graph_bridge.h` (split out for unit testing, mirroring
  how `buildWinGraphSeries` was split for UX-06). Applies to both the `blackData_` series and the
  `whiteData_` series under `WinGraphMode::BothSide`; works in SingleSide and BothSide.
- **Not touched:** `buildWinGraphSeries`, the `evalHistory` NaN sentinel, the eval→win% maths, the
  UI-01 attribution, the UI-09 series colour/weight, the RT-01 cadence, the axes / Y labels /
  50%-line / current-move highlight / hover-box layout. No `ViewConfig` flag, no Settings entry, no
  gap-length cap (all settled with the user 2026-09-04).

## Verification

`./build.sh` clean (only the 3 pre-existing `-Wunused-function` warnings in `gomocup_protocol.cpp`);
`ctest` 3/3 (`ranls-gui-tests`, `ranls-gui-ui-tests`, `rel02-version-single-source`);
`tests/test_anlz04_wingraph_bridge.cpp` (8 cases, in `ranls-gui-tests`) pins the helper: interior
run → one `{fromIdx,toIdx}` pair, leading/trailing runs → none, multiple interior runs → one each,
`n == 0/1` and all-NaN → none, no crash.
