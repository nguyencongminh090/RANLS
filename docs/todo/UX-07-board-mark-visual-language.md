# UX-07 — Redesign board mark language (engine / database / variant / best)

**Status:** 🔲 OPEN (Backlog)
**Area:** src/model/board_view_model.{h,cpp}, src/ui/board_renderer.{h,cpp}, tests/
**Priority:** P2
**Source:** User board-UI task + [board marks review audit](../audit/2026-10-06-board-marks-ux-review.md) — 2026-10-06
**Design:** [features/board-marks/](../../features/board-marks/planning.md) — created 2026-10-06, planning.md RESOLVED 2026-10-06 (design gate passed; instruction entry still to write)
**Depends on / relates to:** UI-18, UX-08

## Problem

Engine tags, database markers and variant dots collide or hide each other; heat colour is red→green only; DB markers lack confidence/best emphasis; variant dots are unlabeled; move-number lookup is O(n²) (audit findings 4–9).

## Scope (in order)

1. ~~Create `features/board-marks/`~~ done 2026-10-06; planning.md questions resolved 2026-10-06 (palette, DB badge vs outline, variant count, toggles).
2. Then `docs/instruction/UX-07-*.md` + this todo's scope refined.
3. Implement: engine disc + best ring over tag; outlined DB diamond / corner badge; ring+dot variants with count; (HSV kept — Q5; no on-board bound/comment glyphs — Q4); index map for move numbers; per-layer `save()/restore()`.

## Scope boundary

- No keyboard-nav work (separate accepted limitation). No engine/protocol changes.

## Reasoning

See audit. Layering unchanged (ui → model).

## Knowledge

`docs/audit/2026-10-06-board-marks-ux-review.md`; `docs/knowledge/principle-layering-dependency-rule.md`; skills `ui-ux-review`, `gtk-ui-design`.

## Acceptance criteria

- Engine, DB and variant marks on the same cell are all distinguishable.
- Colour-blind-safe (value readable without hue); design folder resolved before code.
