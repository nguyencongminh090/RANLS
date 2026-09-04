# ANLZ-04 — WinGraph: bridge NaN gaps with a faint dashed connector instead of breaking the line

**Status:** 🔲 OPEN (Active — Sprint 10, pulled from Backlog 2026-09-04)

**Area:** `src/ui/win_graph_view.cpp` (`WinGraphView::onDraw` — the Black series
loop ~L135-145, the BothSide White series loop ~L159-168). Read-only reference:
`src/ui/win_graph_series.h` (NaN propagation — **not** changed here),
`src/model/game_state.cpp` `evalHistory` (NaN sentinel origin).
**Priority:** P2
**Source:** User decision 2026-09-04 (WinGraph-coverage discussion follow-up,
`docs/notes/2026-09-04-wingraph-analyze-mode-and-backfill.md`). ANLZ-01 fills a
real point for every position the user *analyses*, but a ply that was played
without a search on it (engine not running, Analyze Mode off at the time, an
interrupted search, or the engine's own turn under "Engine plays") still lands a
NaN — and `onDraw` currently lifts the pen at every NaN, so one missing interior
point fragments the graph into disconnected segments ("discontinued").
**Design:** none needed — single-widget rendering change. Options settled with the
user 2026-09-04: **always on** (no `ViewConfig` toggle), **no gap-length cap**.
**Depends on / relates to:** UI-01 (this deliberately refines its "break into
disjoint segments around any NaN run" rule — audit entry required), UI-09
(thicker/high-contrast series — bridge must be visually *sub*-ordinate to it),
UX-06 (WinGraph modes — bridge applies in both SingleSide and BothSide).

## Problem

`WinGraphView::onDraw` draws each series by walking the points and calling
`penDown = false` on every `std::isnan(...)`, so the polyline restarts after any
gap. With ANLZ-01 shipped the graph is *mostly* filled, but any residual NaN ply
(see Source above) still splits the line. The user wants the graph to read as one
continuous trace: connect the last evaluated ply before a gap straight to the
first evaluated ply after it.

## Scope

1. In each series loop (`blackData_`, and the `whiteData_` loop under
   `WinGraphMode::BothSide`), track the last non-NaN point. When the pen was lifted
   by a NaN run and a new non-NaN point arrives, draw the connecting segment
   **from the last non-NaN point to this one** as a distinct style:
   - dashed (`cr->set_dash({4.0, 3.0}, 0.0)` — narrower than the UI-09 White dash
     so the two never read the same),
   - ~40 % alpha of the series colour (reuse `kBlack*` / `kWhite*` with
     `set_source_rgba(..., 0.4)`),
   - line width ≤ the solid series width (e.g. `kSeriesW * 0.6`).
   Then restore solid full-alpha for the next genuinely-consecutive segment.
2. Real consecutive-point segments stay exactly as now (solid, full width/alpha).
3. Leading / trailing NaN runs: nothing to bridge (no anchor on one side) — the
   line simply starts/ends at the first/last evaluated ply, as today.
4. Gap plies still get **no dot** (keep the `!std::isnan(blackData_[currentIndex_])`
   guard on the current-move dot) and the hover tooltip still shows `(no eval)`
   for them (keep that branch unchanged).
5. `docs/audit/2026-<mm-dd>-wingraph-nan-bridge.md` + one `docs/audit.md` row:
   record that UI-01's "disjoint segments, never interpolate through NaN" is being
   softened to "connect with a visually subordinate dashed bridge; still no dot,
   still no false 50%", and why (ANLZ-01 residual gaps fragmenting the trace).

## Acceptance criteria

- A single interior NaN ply (or a run of them) no longer breaks the series: the
  graph shows one continuous trace, with the gap-spanning part dashed + faint.
- The dashed bridge is unmistakably weaker than both the solid Black line and the
  UI-09 dashed White line (different dash pitch + lower alpha + thinner).
- No dot is drawn on an unevaluated ply; hover on a bridged ply still says
  "(no eval)"; no `0.5` / 50 % value is ever synthesised.
- Works in SingleSide and BothSide; the White series is bridged the same way.
- Leading/trailing gaps behave as today (line starts/ends at real data).
- `docs/audit.md` + detail entry added for the UI-01 refinement.
- `./build.sh` clean; `ctest` both suites green; a `ranls-gui-ui-tests` case
  asserts the bridge behaviour (see instruction file).

## Scope boundary

- Do **not** change `buildWinGraphSeries` or the NaN sentinel in `evalHistory` —
  the data still carries NaN for unevaluated plies; only the *drawing* changes.
- Do not change the eval→win% maths, UI-01 attribution, UI-09 series
  colour/weight, RT-01 cadence, or the axes/labels/50 %-line/hover-box layout.
- No `ViewConfig` flag, no Settings entry, no gap-length cap (settled with user).
- Not a data-coverage change — that was ANLZ-01. This is purely how residual gaps
  are rendered.
