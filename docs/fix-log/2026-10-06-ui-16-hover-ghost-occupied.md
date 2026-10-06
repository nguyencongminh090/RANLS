# UI-16: hover and PV ghost stones drew over occupied cells (2026-10-06)

**Status:** Active
**Related:** UI-16 (audit: `docs/audit/2026-10-06-board-marks-ux-review.md`, finding 2)

## Prompt

Continuing the board-UI bugs after UI-18. Symptom (from the review): hovering a stone, or a PV preview
path crossing a stone, paints a translucent stone on top of it.

## Root cause

`BoardRenderer::drawHover` and `drawPVHighlight` only validated the coordinate against the board size;
neither knew whether the cell was occupied, and `BoardViewModel` exposed no occupancy query (only the
flat `stones` list). Reproduced first by rendering into a Cairo image surface: the pixel at the stone
centre changed once `hoverMove` / `pvPreview` pointed at it (2 of 3 new cases failed; the empty-cell
control passed).

## Fix

- `src/model/board_view_model.{h,cpp}`: `BoardViewModel::isOccupied(Coord)` over the render-state stones.
- `src/ui/board_renderer.cpp`: `drawHover` returns early on an occupied cell; `drawPVHighlight` skips
  occupied cells (ghost colour alternation still keys on the original path index).
- Left alone: hover redesign / crosshair / tooltips (UX-08), Cairo state (UI-17), `BoardView` still
  records `hoverMove` on occupied cells (renderer is the single guard; clicks unchanged).

## Verification

- Regression: `tests/test_ui16_hover_occupied.cpp` (registered in `ranls-gui-ui-tests`) — real render
  to `Cairo::ImageSurface`; occupied cell unchanged by hover / PV ghost, empty cell still shows both.
- Release (`./build.sh` + ctest): 5/5. `ranls-gui-tests` 246/2760 unchanged; `ranls-gui-ui-tests`
  63 cases / 464 assertions (+3 / +4).
- Not verified: look on a live board (pixel test only). Practical note: engine PVs for the current
  position normally don't cross stones, so the visible case is mostly hover.

## Knowledge

`docs/audit/2026-10-06-board-marks-ux-review.md`; skill `gtk-ui-design`.
