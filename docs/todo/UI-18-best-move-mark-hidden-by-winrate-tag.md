# UI-18 — Best-move mark is hidden whenever a winrate tag is shown

**Status:** 🔲 OPEN (Backlog)
**Area:** src/model/board_view_model.{h,cpp}, src/ui/board_renderer.cpp, tests/
**Priority:** P2
**Source:** User board-UI task + [board marks review audit](../audit/2026-10-06-board-marks-ux-review.md) — 2026-10-06
**Design:** none — scoped directly (full redesign is UX-07)
**Depends on / relates to:** UX-07 (subsumes the broader fix)

## Problem

`BoardViewModel::update()` resolves one mark per cell with `tag > lost > best`; the best root move nearly always has a tag, so the `Best` mark is effectively never visible with tags on (audit finding 1). The best move is the most important thing the overlay should show.

## Scope (in order)

1. Reproduce with a live search (or unit-level on `update()` with an `AnalysisOverlay` whose bestMove has a tag).
2. Minimal fix: carry `isBest` as a flag on the mark; renderer draws a ring over the tag.
3. Regression test on the ViewModel + fix-log entry.

## Scope boundary

- Only the best-flag; palette, DB marks, variants → UX-07.

## Reasoning

Flag instead of rank keeps the single-winner model for the other kinds. Alternative: raise Best above tag — rejected, it would hide the winrate of the best move.

## Knowledge

Prior: PROTO-06 overlay design (`docs/todo/PROTO-06*`); skill `ui-ux-review`.

## Acceptance criteria

- Best move shows both its tag and a best ring when tags are on.
- Regression test + fix-log entry.
