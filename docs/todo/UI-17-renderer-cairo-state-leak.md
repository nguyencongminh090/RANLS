# UI-17 — BoardRenderer layers leak Cairo state (font face)

**Status:** ✅ FIXED (2026-10-06) — every layer wrapped in `save()/restore()` in `BoardRenderer::draw`; real-render regression test `test_ui17_renderer_state_isolation.cpp`; Release ctest 5/5. See [fix-log](../fix-log/2026-10-06-ui-17-renderer-cairo-state-leak.md).
**Area:** src/ui/board_renderer.cpp
**Priority:** P3
**Source:** User board-UI task + [board marks review audit](../audit/2026-10-06-board-marks-ux-review.md) — 2026-10-06
**Design:** none — scoped directly
**Depends on / relates to:** UI-16, UI-18

## Problem

`drawForbiddenPoints` sets a BOLD font face with no `cr->save()/restore()`, so later layers' label weight depends on whether forbidden points exist (audit finding 3). Visual effect unverified.

## Scope (in order)

1. Reproduce: Renju, Black to move with forbidden points + database/tag labels visible.
2. Wrap each layer in `save()/restore()` (or set face explicitly per layer); regression test if a seam exists, else an explicit "no test infrastructure" note.
3. Fix-log entry.

## Scope boundary

- No visual redesign (UX-07).

## Reasoning

Per-layer save/restore removes the whole bug class; setting the face in each layer only fixes this instance.

## Knowledge

Skill `gtk-ui-design`; `.claude/rules/bugfix.md`.

## Acceptance criteria

- Label font face is identical with and without forbidden points.
- Fix-log entry added.
