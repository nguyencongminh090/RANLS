# UI-20: move numbers on stones were off-centre (2026-10-07)

**Report:** "Consider font for num on stone, centerize number of stone."

**Root cause:** `BoardRenderer::drawStones` positioned the number with `move_to(cx - ext.width/2, cy + ext.height/2)`,
ignoring `ext.x_bearing` / `ext.y_bearing` (offset of the ink box from the text origin), so the glyphs sat
off-centre by a digit-dependent amount.

**Fix:** `move_to(cx - w/2 - x_bearing, cy - h/2 - y_bearing)`; numbers now use the BOLD face for legibility;
`begin_new_path()` after `show_text` (stray-line guard). Single file, `src/ui/board_renderer.cpp`.

**Regression test:** `tests/test_ui20_stone_number_centered.cpp` (real Cairo render; centroid of the number ink
vs the stone centre, tolerance 0.02 cell). Failed before the fix (0.7 px off vs 0.47 allowed), passes after.
Release ctest 5/5.

**Not verified:** in the live window (offscreen render only).
