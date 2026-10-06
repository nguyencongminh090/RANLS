# UX-08: board hover crosshair, margin highlight, mark tooltips (2026-10-06)

**Status:** Active
**Related:** UX-08 (non-bug change; audit finding 10 of the board-marks review)

## Prompt

Tracked task UX-08: marks have no tooltip (winrate/depth/DB value/source) and hovering an empty cell gives no
crosshair or coordinate highlight, so cells are hard to locate on large boards.

## Root cause

[Board marks review audit](../audit/2026-10-06-board-marks-ux-review.md) finding 10: no tooltip and no
crosshair/margin highlight existed. Not a defect in existing code.

## Fix

- `BoardViewModel::tooltipFor(Coord)` (src/model/board_view_model.cpp, GTK-free): engine mark (tag text, tag
  depth, "Best move", "Losing move"), database entry (label/boardText, value, depth, bound Exact/Alpha/Beta,
  "Best database move", "Has comment" -- flag only, never text, Q12), "N variation(s)", Renju
  "Forbidden for Black (still playable)". Empty for occupied / off-board cells. Honours `showSearchOverlay`
  (engine) and `showDatabase` (database) independently. Examined/Examining dots deliberately add no text.
- `BoardView`: `set_has_tooltip(true)` + `signal_query_tooltip` -> `pixelToCoord` -> `tooltipFor`; false when empty.
- `BoardRenderer`: new `drawCrosshair` layer (through `layer()`) between grid and stones: a translucent
  row + column line through the hovered cell. Margin labels in `drawGrid` for the hovered column/row are bold +
  accent colour. **Occupied hovered cell: no crosshair and no label emphasis** (follows the UI-16 "no hover
  feedback on a real stone" rule; tested).
- Left alone: DatabaseEntry/protocol, settings, click handling, forbidden-point playability, hoverMove semantics,
  UI-18 ring / UX-07 badges.

## Verification

- Tests: `tests/test_ux08_tooltip_model.cpp` (16 cases, every tooltip branch and toggle),
  `tests/test_ux08_hover_crosshair.cpp` (6 render cases: crosshair on 15x15 and 22x22, stone/mark above it,
  occupied hover identical frame, margin label emphasis light/dark, occupied leaves margin unchanged),
  `tests/test_ux08_board_view_tooltip.cpp` (2 cases: real `BoardView` in a window, GTK `query-tooltip` emitted at
  pixels; true only over a database cell, false over empty/occupied/margin; self-skips without a display).
- `RUN_TESTS=1 ./build.sh <dir>` (Release): ctest 5/5; `ranls-gui-tests` 246/2760; `ranls-gui-ui-tests` 104/634.
- Offscreen PNGs (15x15 and 22x22, light and dark margin) viewed. NOT verified in the live GTK window: the actual
  tooltip popup and its text were not seen on screen, only the signal return value is tested (the text set on the
  `Gtk::Tooltip` is not read back).

## Knowledge

`features/board-marks/planning.md` (Q7, Q9, Q12), `docs/instruction/UX-08-board-hover-crosshair-and-tooltips.md`.
