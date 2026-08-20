# UX-04 — Board rendering never verified at the extremes of the supported 5–22 range

**Status:** open (investigation — findings below are **unverified**, not observed defects)
**Area:** board renderer / board view
**Priority:** P3
**Source:** codebase review, 2026-08-21

## Context

`MainWindow::onBoardSize` allows 5–22 (`src/main_window.cpp:492`) and `MAX_BOARD_SIZE` is 22
(`src/model/board_state.h:7`), but the renderer's constants are tuned for the 15×15 default. The
`ui-ux-review` checklist calls for checking both ends of the range.

This item is scoped as **investigation first** — the concerns below are read from the code, not
observed. Confirm each before fixing anything.

## Things to check

**At 22×22 (cells get small):**
- `kCoordMargin = 24.0` is a fixed pixel reserve (`src/ui/board_renderer.cpp:40`,
  duplicated in `src/ui/board_view.cpp:6` — note the constant is defined twice and could drift).
  Coordinate label font is `std::max(9.0, cellSize_ * 0.35)` (`src/ui/board_renderer.cpp:132`), so
  below ~26px cells the label stops scaling and may overflow the margin.
- Move-number text on stones uses `std::max(8.0, cellSize_ * 0.45)`
  (`src/ui/board_renderer.cpp:193`) against a stone radius of `cellSize_ * 0.44`
  (`src/ui/board_renderer.cpp:61`) — at small cells a 3-digit move number likely exceeds the stone.
- Candidate-move labels (`src/ui/board_renderer.cpp:284`) and database labels
  (`src/ui/board_renderer.cpp:252`) have the same issue, and both can occupy the same cell — the
  checklist explicitly asks whether the database overlay collides with the PV/last-move markers.
- Click-target size at 22×22 in a small window.

**At 5×5 (cells get large):**
- No star points are drawn (`bs == 15` only, `src/ui/board_renderer.cpp:117`) — see PROTO-02.
- Whether the board grows unboundedly or is capped sensibly.

**Both:**
- `BoardView::pixelToCoord` (`src/ui/board_view.cpp:60-78`) recomputes the same geometry as
  `BoardRenderer::draw` (`src/ui/board_renderer.cpp:70-76`) in a separate copy. If either changes,
  clicks and rendering silently disagree. Worth unifying regardless of what the size testing finds.
- Window resize behaviour at both ends.

## Acceptance criteria

- Screenshots at 5×5, 15×15, and 22×22 in both light and dark themes, attached to the fix-log entry.
- Any confirmed defect fixed; anything checked and found fine is recorded as verified so it is not
  re-investigated.
- Board geometry computed in exactly one place.

## Scope boundary

- Correctness of coordinate *values* at non-15 sizes is PROTO-02 — fix that first, or this
  investigation will be confounded by wrong labels.

## Related

- PROTO-02 (hardcoded 15), UX-03 (contrast and colour-only meaning)
