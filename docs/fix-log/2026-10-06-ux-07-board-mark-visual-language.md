# UX-07: board mark visual language (2026-10-06)

**Status:** Active
**Related:** UX-07 (follows UI-16/17/18; UX-08 depends on this model)

## Prompt

Implement UX-07 per `docs/todo/UX-07-board-mark-visual-language.md` and
`docs/instruction/UX-07-board-mark-visual-language.md`: redesign the board mark language so engine,
database and variant marks on one cell stay distinguishable, mark the best database entry, show the
variant branch count, and drop the O(n^2) move-number lookup.

## Root cause

Non-bug change addressing review findings 4-9 of `docs/audit/2026-10-06-board-marks-ux-review.md`: the
filled database diamond, the variant dot and the engine tag disc were painted on top of each other
(the later layer hid the earlier); the best database entry had no emphasis; variant dots carried no
branch count; `drawStones` ran a `std::find` over `moveHistory` per stone.

Side finding fixed in passing because the layer reorder exposed it: `save()/restore()` does not cover
the Cairo current path, so a layer ending in `show_text` left a current point and the next layer's
first `arc()` drew a stray connecting line (seen in the screenshots as an orange/red line between
marks; the UI-17 isolation tests failed after the reorder until `begin_new_path()` was added).

## Fix

- `src/model/board_view_model.{h,cpp}`: `moveNumberAt(Coord)` backed by a per-update table;
  `Marker::isBest` (highest `value`, ties flag every tied entry) and `Marker::branchCount`
  (`getBranchCoords` is insertion-ordered, so index 0 = main continuation).
- `src/ui/board_renderer.{h,cpp}`: new layer order; outlined DB diamond, corner badge when
  `hasEngineMark()`, thicker outline for best; variant ring + dot + count badge (> 1 branch); both
  badge kinds hidden when `cellSize_ < 20`; `begin_new_path()` in the layer wrapper and in the
  per-mark loops of the DB / variant / search / forbidden layers; `drawStones` uses `moveNumberAt`.
- Left alone: HSV heat ramp, settings, bound/comment glyphs, UX-08 items, `drawPVHighlight` (same
  latent text-then-arc pattern, only fills; not touched).

## Verification

- New: `tests/test_ux07_board_mark_model.cpp`, `tests/test_ux07_board_marks_render.cpp` (+17 cases).
- Release `RUN_TESTS=1 ./build.sh`: ctest 5/5; `ranls-gui-tests` 246 cases / 2760 assertions;
  `ranls-gui-ui-tests` 80 cases / 542 assertions.
- Offscreen Cairo PNG renders (15x15 and 22x22, light and dark margins) inspected, including the UI-18
  best ring. Not verified in the live GTK window.

## Knowledge

`docs/audit/2026-10-06-board-marks-ux-review.md`, `features/board-marks/planning.md`.
