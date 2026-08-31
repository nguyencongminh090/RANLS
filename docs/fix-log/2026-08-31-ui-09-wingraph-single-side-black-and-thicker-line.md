# 2026-08-31 — UI-09: WinGraph SingleSide always Black; thicker, higher-contrast win-rate line

## Prompt

Tracked task **UI-09** (`docs/todo/UI-09-wingraph-single-side-black-and-thicker-line.md`), Sprint 7.
Three related changes to the win-rate graph:

1. `WinGraphMode::SingleSide` must render **Black's perspective unconditionally**, for every
   position, regardless of `MatchConfig::enginePlays` — reversing the UX-06 (Sprint 6) decision that
   made SingleSide follow the engine's assigned side.
2. `BothSide` — no behaviour change (its two-line rendering was already perspective-correct per
   side); only the shared line-width / colour change applies.
3. Make the win-rate line visibly thicker and pick series colour(s) that meet WCAG contrast against
   both the light and dark panel backgrounds and are distinguishable from each other and from the
   axis/grid scaffold.

## Action

### Part 1 — SingleSide = always Black

- `src/ui/win_graph_series.h` (`buildWinGraphSeries`): removed the
  `whitePerspective = (SingleSide && enginePlays == White)` branch. SingleSide now always pushes
  `blackWin`. The `EnginePlaysSide enginePlays` parameter is **retained** (call-site stability +
  it lets `test_ux06_wingraph_series.cpp` pin "ignores enginePlays" as a regression guard) but is
  `(void)`-discarded. Header doc comment rewritten.
- `src/ui/analysis_panel.cpp` (`toDisplayWinrate`): unchanged in signature — it still forwards
  `gameState_.matchConfig().enginePlays`, which `buildWinGraphSeries` now ignores for SingleSide.
  The coupling is gone at the point it mattered.
- `src/model/config.h`: `WinGraphMode::SingleSide` doc comment updated to "always Black's
  perspective … regardless of `MatchConfig::enginePlays` (UI-09)".
- `src/ui/settings_dialog.cpp`: combo label `"Single line (engine's perspective)"` →
  `"Single line (Black's perspective)"`; tooltip reworded to drop the "or the engine's side" clause.
- `tests/test_ux06_wingraph_series.cpp`: the case *"UX-06: SingleSide Auto follows enginePlays ==
  White"* is replaced by *"UI-09: SingleSide is always Black's perspective, regardless of
  enginePlays"* — asserts the `Off` / `Black` / `White` series are byte-for-byte identical and that
  the series really is Black's perspective (move 0: White-to-move eval 0.70 → 0.30, not 0.70). The
  other three cases (one-line-vs-two, default-Black, NaN gaps) are unchanged.

#### Rationale for reversing the UX-06 coupling

UX-06's "SingleSide follows `enginePlays`" was meant to show "the number the engine is optimising".
In practice it made the single-line graph **ambiguous without also checking the Settings dialog**:
the same game, same evals, produced a line and its vertical mirror depending on an unrelated
play-mode toggle, and nothing on the graph said which. It also interacted badly with ENG-02 (same
sprint), which now reverts `enginePlays` to `Off` on manual intervention — so the graph's
orientation could flip mid-session as a side effect of pressing Stop. A fixed "Black is up = Black
is winning" convention is what every other Gomoku/chess eval graph uses and is unambiguous on its
own. `BothSide` still covers the "see it from White's side" need explicitly. This is a deliberate,
traceable reversal of the "Resolved decisions (with user, 2026-08-30)" line in
`docs/todo/UX-06-*.md`; UX-06 itself stays ✅ VERIFIED (its dialog/theme/coordinate work is
untouched).

### Part 3 — thicker, higher-contrast line (`src/ui/win_graph_view.cpp`)

Line width: main series `1.5 → 2.8` px, White (BothSide) series `1.2 → 2.6` px, both with round
joins/caps. Widths are user-space units; GTK4 applies the device scale itself and `onDraw` has no
`cr->scale()`, so the line renders at the same logical thickness and stays crisp on HiDPI. The
scaffold is deliberately left subordinate: the 50 % centre line stays `1.0` px at `0.5` alpha, the
axis labels still follow the theme foreground, the current-move highlight stays `1.5` px.

**Colour pass.** The panel background is not set in `style.css` — it follows the GTK theme, forced
by UX-06 to Adwaita / Adwaita-dark:

| surface | hex | approx. |
|---|---|---|
| light panel | Adwaita `window_bg` | `#fafafb` |
| dark panel  | Adwaita-dark `window_bg` | `#242424` |

A single fixed pair has to clear WCAG **1.4.11 non-text contrast (≥ 3:1)** against *both*. Chosen:

| series | old | new | contrast vs `#fafafb` | contrast vs `#242424` |
|---|---|---|---|---|
| Black's-perspective line (also the SingleSide line) | `#3373D9` (0.20,0.45,0.85) | **`#1A73E8`** (0.102,0.451,0.909) | **4.32:1** | **3.45:1** |
| White's-perspective line (BothSide only) | `#D9CC40` (0.85,0.80,0.25) — **1.59:1 on light, failed** | **`#1E8E3E`** (0.118,0.557,0.243) | **4.03:1** | **3.69:1** |

Contrast ratios computed with the WCAG relative-luminance formula. The old yellow White line failed
badly on a light panel (1.59:1); the new green clears 3:1 on both.

`dataviz` skill palette validator (`scripts/validate_palette.js "#1A73E8,#1E8E3E"`) — **all checks
PASS** in both `--mode light --surface #fafafb` and `--mode dark --surface #242424`:

```
[PASS] Lightness band     both inside the mode's band
[PASS] Chroma floor       both >= 0.1
[PASS] CVD separation     worst adjacent #1E8E3E<->#1A73E8  deutan ΔE 26.9 · tritan ΔE 5.8
[PASS] Normal-vision floor  ΔE 28.5
[PASS] Contrast vs surface  both >= 3:1
```

Tritan-type ΔE (5.8) is in the low band, so the two series are **not** distinguished by hue alone:
the White line keeps its dash pattern (widened to `{6,4}` to read at the new weight) as the
secondary (shape) encoding. Blue vs green also stays clear of the red current-move highlight
(`#D93333`) and the neutral-grey grid.

### Verification

Build: `./build.sh` (CMake + Ninja, `build_cmd/`, `BUILD_TYPE=Release`) — clean, no new warnings.

Tests: `ctest --test-dir build_cmd --output-on-failure` — `2/2` suites pass
(`rapfi-gui-tests`, `rapfi-gui-ui-tests`).
- `rapfi-gui-tests`: 132 doctest cases pass (4 in `test_ux06_wingraph_series.cpp`, 27 assertions —
  incl. the new "SingleSide always Black regardless of enginePlays" case).
- `rapfi-gui-ui-tests`: 6 cases / 51 assertions pass (display server available, 0 skipped) —
  `win_graph_view.cpp` + `analysis_panel.cpp` link and construct.

The line-width / colour change itself has no screenshot harness: it is verified by code inspection
(no `cr->scale()`; scaffold weights unchanged) plus the documented contrast-ratio math and the
`dataviz` validator run above.

## Summary

`WinGraphMode::SingleSide` is now unconditionally Black's perspective — the UX-06 "follows
`MatchConfig::enginePlays`" coupling is removed (rationale above; traceable reversal of a
2026-08-30 user decision). `BothSide` logic untouched. The win-rate line is thicker (2.8 / 2.6 px,
was 1.5 / 1.2) and recoloured to a blue `#1A73E8` + green `#1E8E3E` pair that clears WCAG 3:1
non-text contrast against both the light (`#fafafb`) and dark (`#242424`) Adwaita panel
backgrounds, with the White line still dashed for CVD-safe shape redundancy. Settings label +
`config.h` / `win_graph_series.h` doc comments updated. Regression test in
`test_ux06_wingraph_series.cpp` replaced per the todo. Build clean, both ctest suites green.

Out of scope (per the todo's Scope boundary, untouched): win-rate attribution logic (UI-01), the
eval→win-rate conversion maths, graph axis/layout beyond line width + colour, STATE-01 clear/notify
paths, UI-08 empty-state work.
