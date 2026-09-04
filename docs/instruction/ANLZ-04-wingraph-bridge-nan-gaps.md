# Instruction — ANLZ-04: WinGraph NaN-gap bridge

## Approach

Pure rendering change in `WinGraphView::onDraw`. No model/series changes, no
config. The current per-series loop is:

```cpp
bool penDown = false;
for (int i = 0; i < n; ++i) {
    if (std::isnan(data[i])) { penDown = false; continue; }
    double x = ..., y = ...;
    if (!penDown) { cr->move_to(x, y); penDown = true; }
    else          cr->line_to(x, y);
}
cr->stroke();
```

Change it to keep the index of the last non-NaN point. When `penDown` is false
and a non-NaN point arrives *and* there was a previous non-NaN point, that segment
spans a gap → stroke it separately with the faint dashed style, then start a fresh
solid sub-path at the current point. Sketch:

```cpp
int lastReal = -1;
auto strokeSolidRun = [&]{ /* set solid style; stroke(); */ };
// accumulate solid runs as today; when resuming after a gap:
//   flush the current solid run,
//   draw ONE dashed faint line lastReal -> i,
//   begin a new solid run at i.
```

Simplest correct structure: two passes — pass 1 draws every gap-spanning segment
(iterate, remember `lastReal`, whenever `lastReal >= 0` and `i` is the next real
point with `i > lastReal + 1`, draw the dashed connector); pass 2 is the existing
loop unchanged for the solid runs. Two passes avoid interleaving `set_dash` /
`unset_dash` inside one path.

Do this for the `blackData_` loop and, guarded by
`mode_ == WinGraphMode::BothSide && whiteData_ size checks`, the `whiteData_` loop.

## Pitfalls

- **Don't `set_dash` mid-path.** Cairo applies dash state at `stroke()` time for
  the whole current path. Stroke the solid run, then set dash, then stroke the
  bridge, then `unset_dash()`.
- **UI-09 White line is already dashed** (`{6.0, 4.0}`). The bridge must use a
  clearly different pitch (e.g. `{4.0, 3.0}`) *and* lower alpha *and* thinner
  width, or a bridged Black segment will look like the White series.
- The bridge colour is the *series* colour at ~0.4 alpha — not grey. A grey bridge
  reads as an axis/guide, not "this line, uncertain".
- Keep the current-move dot guard (`!std::isnan(blackData_[currentIndex_])`) and
  the hover `(no eval)` branch **exactly as they are** — a bridged ply is still
  unevaluated for every purpose except the connecting line.
- `n == 1` and all-NaN cases: no bridge, no crash — the existing
  `blackData_.empty()` early-return plus `lastReal == -1` guards cover it; verify.
- Leading gap (`data[0..k]` NaN, `data[k+1]` real): `lastReal == -1` at that point
  → no bridge, line starts at `k+1`. Trailing gap: loop just ends. Both correct,
  add test cases.

## Verification before marking this task done

1. `./build.sh` — clean, only the 3 known pre-existing `-Wunused-function`
   warnings in `gomocup_protocol.cpp`.
2. `ctest` — `ranls-gui-tests`, `ranls-gui-ui-tests`, `rel02-version-single-source`
   all green.
3. **New `ranls-gui-ui-tests` case** (`tests/test_anlz04_wingraph_bridge.cpp`):
   construct a `WinGraphView`, feed a series with an interior NaN run
   (e.g. `[0.5, 0.6, NaN, NaN, 0.55]`) plus a leading and a trailing NaN case,
   render to a recording Cairo surface / drive `onDraw`, and assert the widget
   produces a connected path across the interior gap and no dot at the NaN
   indices. If a pixel/recording-surface assertion is impractical in the harness,
   assert at the seam the code exposes (e.g. factor the "gap segments" list out of
   `onDraw` into a small pure helper `computeGapBridges(const std::vector<double>&)`
   returning `{fromIdx,toIdx}` pairs, and unit-test that helper in the model-layer
   `ranls-gui-tests` target — mirrors how `buildWinGraphSeries` was split out for
   UX-06).
4. **Manual smoke:** load/replay a game with a few unanalysed plies (or the
   original UI-13 repro screenshot flow with Analyze Mode off) → the WinGraph is
   one continuous trace, the gap portions visibly dashed/faint, no dots there.
5. `docs/audit.md` row + `docs/audit/<date>-wingraph-nan-bridge.md` written
   (UI-01 refinement rationale).

Tiers 3–5 are required, not just 1–2.

## Boundaries — do not touch

- `buildWinGraphSeries` (`win_graph_series.h`), the `evalHistory` NaN sentinel,
  eval→win% maths, UI-01 attribution, UI-09 series colour/weight, RT-01 cadence.
- WinGraph axes, Y labels, 50 %-line, current-move vertical highlight, hover box
  layout and text.
- No `ViewConfig` flag / Settings entry / gap cap.
