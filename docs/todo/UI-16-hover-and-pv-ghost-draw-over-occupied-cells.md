# UI-16 — Hover and PV ghost stones draw over occupied cells

**Status:** 🔲 OPEN (Backlog)
**Area:** src/ui/board_renderer.cpp (drawHover, drawPVHighlight), tests/
**Priority:** P2
**Source:** User board-UI task + [board marks review audit](../audit/2026-10-06-board-marks-ux-review.md) — 2026-10-06
**Design:** none — scoped directly
**Depends on / relates to:** UI-17, UI-18

## Problem

`drawHover` and `drawPVHighlight` never check that the target cell is empty, so a translucent stone is painted on top of an existing stone (findings 2 in the audit). Code-derived; reproduce on the real board first (`systematic-debugging`, `.claude/rules/bugfix.md`).

## Scope (in order)

1. Reproduce (hover an occupied cell; hover a PV whose path crosses an occupied cell).
2. Root cause, then smallest fix: skip occupied cells (needs occupancy in the ViewModel — the renderer must not touch GameState).
3. Regression test + fix-log entry (`docs/fix-log.md` + `docs/fix-log/`).

## Scope boundary

- Do not redesign hover (crosshair/tooltip belong to UX-08).

## Reasoning

Minimal guard fixes the visible defect; redesign is separate work. Alternative (draw hover only when click would be legal) rejected: forbidden points stay clickable by design (UI-03).

## Knowledge

`.claude/rules/bugfix.md`; skill `gtk-ui-design`.

## Acceptance criteria

- No hover/ghost stone is drawn on an occupied cell.
- Regression test covers the occupancy guard; fix-log entry added.
