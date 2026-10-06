# UX-08 — Board hover UX: crosshair, margin highlight, mark tooltips

**Status:** ✅ DONE (2026-10-06) — `BoardViewModel::tooltipFor` (engine tag/best/lost + depth, database value/depth/bound/best/has-comment, variant count, Renju forbidden "still playable"; empty for occupied/off-board; examined/examining dots add no text), `BoardView` query-tooltip, renderer crosshair layer below stones (empty hovered cells only; none on occupied) + bold accent margin labels. +26 cases (`test_ux08_*`); Release ctest 5/5, `ranls-gui-tests` 246/2760, `ranls-gui-ui-tests` 104/634. Offscreen PNGs viewed; not verified in the live GTK window.
**Area:** src/ui/board_view.cpp, src/ui/board_renderer.cpp, src/model/board_view_model.*
**Priority:** P3
**Source:** User board-UI task + [board marks review audit](../audit/2026-10-06-board-marks-ux-review.md) — 2026-10-06
**Design:** [features/board-marks/](../../features/board-marks/planning.md) (shared with UX-07) · [instruction](../instruction/UX-08-board-hover-crosshair-and-tooltips.md)
**Depends on / relates to:** UX-07, UI-16

## Problem

No tooltip on marks (winrate/depth/DB value/source) and no hover crosshair or coordinate highlight in the margins, which makes cells hard to locate on large boards (audit finding 10).

## Scope (in order)

1. Q12 investigated 2026-10-06: `hasComment` is only a 0/1 flag from the engine's `DATABASE` row, no comment text exists → tooltip says "has comment", no text, no model change.
2. `BoardViewModel::tooltipFor(Coord)` (GTK-free): engine mark, database entry (value/depth/bound/best/has comment), variant count, forbidden point; empty for occupied/off-board cells. Unit-tested.
3. `BoardView`: `query-tooltip` reading `tooltipFor`.
4. Renderer: faint row/column crosshair below the stones on the hovered empty cell + emphasised margin column/row labels. Render-tested.

## Scope boundary

- Keyboard navigation excluded.

## Reasoning

Tooltips avoid cluttering the board with extra text; depends on UX-07 so mark data model is settled once.

## Knowledge

`docs/audit/2026-10-06-board-marks-ux-review.md`; skill `ui-ux-review`.

## Acceptance criteria

- Hovering a mark/forbidden point/variant cell yields the tooltip text from the ViewModel; occupied cells yield none.
- Database tooltip shows value, depth, bound, best flag and "has comment" (no comment text).
- Hovering an empty cell draws a row/column crosshair and highlights its margin labels; marks stay visible above it.
- No new settings; clicks and forbidden-point behaviour unchanged; Release ctest green; tooltip wiring coverage stated honestly.
