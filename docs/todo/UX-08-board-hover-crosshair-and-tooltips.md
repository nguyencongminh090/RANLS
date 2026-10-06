# UX-08 — Board hover UX: crosshair, margin highlight, mark tooltips

**Status:** 🔲 OPEN (Backlog)
**Area:** src/ui/board_view.cpp, src/ui/board_renderer.cpp, src/model/board_view_model.*
**Priority:** P3
**Source:** User board-UI task + [board marks review audit](../audit/2026-10-06-board-marks-ux-review.md) — 2026-10-06
**Design:** features/board-marks/ (shared with UX-07)
**Depends on / relates to:** UX-07, UI-16

## Problem

No tooltip on marks (winrate/depth/DB value/source) and no hover crosshair or coordinate highlight in the margins, which makes cells hard to locate on large boards (audit finding 10).

## Scope (in order)

1. Settled in `features/board-marks/planning.md` (resolved 2026-10-06). First step: investigate where `DatabaseEntry::hasComment` comes from (Q12) — tooltip shows comment text if it maps to a `VariationTree` node, else "has comment".
2. `query-tooltip` on BoardView reading model data; hover crosshair + margin label highlight.

## Scope boundary

- Keyboard navigation excluded.

## Reasoning

Tooltips avoid cluttering the board with extra text; depends on UX-07 so mark data model is settled once.

## Knowledge

`docs/audit/2026-10-06-board-marks-ux-review.md`; skill `ui-ux-review`.

## Acceptance criteria

- Hovering a mark shows its source and values; hovering a cell highlights row/column labels.
