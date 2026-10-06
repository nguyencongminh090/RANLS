# Instruction — UX-07: board mark visual language (engine / database / variant / best)

Detail: [docs/todo/UX-07-board-mark-visual-language.md](../todo/UX-07-board-mark-visual-language.md)
Design of record: [features/board-marks/planning.md](../../features/board-marks/planning.md) (resolved
2026-10-06). UI-16/17/18 already shipped (PRs #46–#48) — build on them, don't redo them.

## Decisions already made — do not re-open

- Best is a **flag** (`SearchOverlayMark::isBest`, exists since UI-18), drawn as a ring over the tag.
- Board shows **eval text only** for database marks — no bound/comment glyphs (those go to UX-08's tooltip).
- **HSV heat stays**; no new colour ramp. Colour-blind safety = the numeric label (keep its dark shadow).
- "Best move track" = highlight only. No arrows, no persistent PV trail.
- Q2/Q3/Q6–Q10 take the planning defaults (below).

## Approach (suggested split into real CODEs at sprint planning — letter suffixes are not allowed)

1. **Model — stacking + index map.** In `BoardViewModel::update()`:
   - Keep one `SearchOverlayMark` per empty cell; engine channel (tag/lost/examined/examining) + `isBest`.
     Marks never sit on stones, so "stack" only means engine + database + variant on the *same empty cell*.
   - Add a `Coord → move index` map built once per `update()` (replaces the per-stone `std::find` in
     `drawStones`). Measure first (`perf-optimization`) — land it only if it is measurable or trivially safe.
   - Database marker gets `isBest` (highest `value` among entries) for a thicker outline.
   - Variant markers get a branch count (`children.size()`); `getBranchCoords()` returns children in
     order, so index 0 = main continuation — confirm in `VariationTree` before relying on it.
   - Unit-test all of this in the GTK-free path (like `test_proto06_analysis_overlay.cpp`).
2. **Renderer — layer order and glyphs.** Order: grid → stones → last move → variant → database →
   engine → best ring → lost/forbidden → PV ghost → hover. Database = **outlined** diamond; when the cell
   also has an engine mark it shrinks to a corner badge. Variant = ring + dot, count when > 1. Hide
   badges/counts under a cell-size threshold (22×22 boards). Keep the per-layer `save()/restore()`
   (UI-17) — new layers go through the same `layer()` wrapper.
3. **Verify visually.** Pixel/render tests (pattern: `test_ui16_hover_occupied.cpp`,
   `test_ui17_renderer_state_isolation.cpp`) for "both marks visible on one cell"; plus screenshots in the
   PR (Light + Dark theme, 15×15 and 22×22). UI-18's ring geometry was never seen on screen — check it here.

## Pitfalls

- `ui → model` only; `BoardRenderer` reads the ViewModel, never `GameState` (rule 8).
- `showDatabase`, `showSearchOverlay`, `showSearchWinrate`, `showMoveNumbers` must keep working unchanged;
  no new settings in this pass (Q10).
- Overlay redraws at engine-update rate (RT-01 throttled) — no per-frame allocation in the renderer.
- Forbidden points stay clickable (UI-03); do not turn marks into input blockers.
- Do not start UX-08 (tooltips/crosshair) here; it depends on this model but is its own PR.
