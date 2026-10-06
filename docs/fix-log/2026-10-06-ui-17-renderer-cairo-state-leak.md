# UI-17: BoardRenderer layers leaked Cairo state (font face) (2026-10-06)

**Status:** Active
**Related:** UI-17 (audit: `docs/audit/2026-10-06-board-marks-ux-review.md`, finding 3)

## Prompt

Next board-UI bug after UI-18 / UI-16: label weight depended on whether forbidden points exist.

## Root cause

All layers share one `Cairo::Context` and none saved/restored it. `drawForbiddenPoints` calls
`select_font_face(BOLD)`; `drawDatabaseMarkers`, `drawSearchOverlay` and `drawPVHighlight` never set a
face, so they inherited BOLD whenever any forbidden point existed, and the normal face otherwise
(inherited from the coordinate labels in `drawGrid`). Reproduced first by rendering a database label and
a search tag into a Cairo image surface with and without a forbidden point in another cell: pixels
differed in both cases.

## Fix

`src/ui/board_renderer.cpp` `draw()`: each layer now runs inside `cr->save()/restore()` (small
`layer()` lambda), which covers font face/size, line width and source colour for every layer — the whole
bug class, not only the BOLD "X". The base face (`sans-serif`, normal) is set once before the layers so
the text layers keep exactly the face they used to inherit. Layer bodies unchanged.

## Verification

- Regression: `tests/test_ui17_renderer_state_isolation.cpp` (in `ranls-gui-ui-tests`) — database label
  and search tag are pixel-identical with/without forbidden points elsewhere (failed before the fix).
- Release (`./build.sh` + ctest): 5/5; `ranls-gui-tests` 246/2760 unchanged; `ranls-gui-ui-tests`
  62 cases / 462 assertions (+2 / +2).
- Not verified: on-screen look. No visual change expected when there are no forbidden points
  (same base face); with forbidden points, labels are now normal weight instead of bold.

## Knowledge

`docs/audit/2026-10-06-board-marks-ux-review.md`; skill `gtk-ui-design`.
