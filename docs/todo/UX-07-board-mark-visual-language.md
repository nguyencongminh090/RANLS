# UX-07 — Redesign board mark language (engine / database / variant / best)

**Status:** 🔲 OPEN (Active — Sprint 22)
**Area:** src/model/board_view_model.{h,cpp}, src/ui/board_renderer.{h,cpp}, tests/
**Priority:** P2
**Source:** User board-UI task + [board marks review audit](../audit/2026-10-06-board-marks-ux-review.md) — 2026-10-06
**Design:** [features/board-marks/](../../features/board-marks/planning.md) — created 2026-10-06, planning.md RESOLVED 2026-10-06 (design gate passed) · [instruction](../instruction/UX-07-board-mark-visual-language.md)
**Depends on / relates to:** UI-18, UX-08

## Problem

Engine tags, database markers and variant dots collide or hide each other; heat colour is red→green only; DB markers lack confidence/best emphasis; variant dots are unlabeled; move-number lookup is O(n²) (audit findings 4–9).

## Scope (in order)

Already shipped (do not redo): best flag + ring over the tag (UI-18), occupied-cell guards (UI-16), per-layer `save()/restore()` (UI-17). Design of record: `features/board-marks/planning.md`.

1. **Model** (`BoardViewModel::update()`): `Coord → move index` map built once (replace the per-stone `std::find` in `drawStones`); database marker `isBest` (highest `value`); variant markers carry a branch count (`children.size()`; index 0 = main continuation — verify in `VariationTree`). Unit tests, GTK-free.
2. **Renderer**: layer order grid → stones → last move → variant → database → engine → best ring → lost/forbidden → PV ghost → hover; database = outlined diamond, shrinking to a corner badge when the cell also has an engine mark, thicker outline for the best DB entry; variant = ring + dot, count when > 1; hide badges/counts below a cell-size threshold. HSV heat unchanged; database keeps eval text only.
3. **Verify**: render regression tests (pattern: `test_ui16_hover_occupied.cpp`) for "engine + database + variant all visible on one cell"; screenshots in the PR (Light + Dark, 15×15 and 22×22), including UI-18's ring.

## Scope boundary

- No keyboard-nav work (separate accepted limitation). No engine/protocol changes.

## Reasoning

See audit. Layering unchanged (ui → model).

## Knowledge

`docs/audit/2026-10-06-board-marks-ux-review.md`; `docs/knowledge/principle-layering-dependency-rule.md`; skills `ui-ux-review`, `gtk-ui-design`.

## Acceptance criteria

- Engine, database and variant marks on the same empty cell are all distinguishable (render test).
- The best DB entry is visually distinct; variant count shows when a node has > 1 branch.
- Move numbers no longer use a per-stone linear search.
- Evaluation stays readable without hue (numeric label kept, HSV unchanged — planning Q5); no new settings, no on-board bound/comment glyphs.
- Existing `showDatabase` / `showSearchOverlay` / `showSearchWinrate` / `showMoveNumbers` toggles behave as before.
- Release ctest green; fix-log/todo markers updated; screenshots attached to the PR (or an explicit "not verified on screen" note).
