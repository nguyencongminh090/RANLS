# 2026-10-06 — Board marks / visualization / UX review (read-only)

**Status:** Active

## Prompt

User UI task (scope: Board): "Improve mark, visualize on board; best move track / database mark;
improve UX on board." Asked for read-the-codebase-first, then reasoning about the UI. Follow-up:
"write it to audit and todo".

## Scope

Read `src/ui/board_renderer.{h,cpp}`, `src/ui/board_view.cpp`, `src/model/board_view_model.{h,cpp}`,
`src/model/analysis_types.h` (via CodeGraph + file reads). Static review only: **nothing was run or
screenshotted**, so visual claims below are code-derived, not observed. Not examined: PVView /
analysis panel, keyboard navigation (already an accepted limitation — see
`docs/audit/2026-08-21-custom-drawn-widgets-no-keyboard-focus.md`).

Layer order today: grid → stones → lastMove → forbidden → database → variant → searchOverlay →
pvGhost → hover (`board_renderer.cpp:95-104`).

## Findings

**Defects (code-confirmed):**
1. **Best mark hidden by tag.** `board_view_model.cpp` resolves one mark per cell with priority
   `tag > lost > best > examined > examining`. The best root move almost always carries a winrate
   tag, so the cyan `Best` disc is almost never drawn while tags are on.
2. **Hover/ghost not guarded for occupied cells.** `drawHover` (`board_renderer.cpp` ~L560) and
   `drawPVHighlight` never check the cell is empty → translucent stone drawn over a real stone.
3. **Cairo state leak.** `drawForbiddenPoints` calls `select_font_face(..., BOLD)` with no
   `save()/restore()`; later layers (database/tag labels) inherit bold only when forbidden points
   exist, so label weight depends on position. (Visual effect unverified.)

**Design weaknesses:**
4. Database diamond and engine tag share the HSV heat scale; on the same cell the tag (drawn later)
   hides the diamond → DB info lost. Only shape distinguishes the sources.
5. Red→green heat is not colour-blind safe (text labels partly compensate).
6. Variant marker is an unlabeled dot; no main-line vs side-branch distinction or branch count; drawn
   under the search overlay.
7. Database `eval < 0` branch (green fallback) is dead code: `eval` is a sigmoid output in [0,1].
8. DB marker carries only eval + text: no confidence (depth/visits), no "best DB move" emphasis.
9. Move numbers: `std::find` over `moveHistory` per stone per frame = O(n²).
10. No tooltips, no crosshair / margin-coordinate highlight on hover (hard to locate cells on 22×22).

## Reasoning

Principle: one visual language per information channel, allowed to overlap without hiding each
other (engine = filled disc at centre; database = outlined diamond / corner badge; variant =
ring+dot with count; best = ring drawn *over* the tag, as a flag not a priority rank). Perceptual
(viridis/cividis-like) ramp plus text label for colour-blind safety. Alternatives rejected: keep
priority-resolution and just reorder (still one mark per cell → still hides info); separate
overlay toggle per source (more settings, doesn't fix the collision).
All changes stay in `ui → model` (ViewModel gains fields, Renderer reads) — no layering change.

## Knowledge

`docs/knowledge/principle-layering-dependency-rule.md`; skills `ui-ux-review`, `gtk-ui-design`;
prior audit `2026-08-21-custom-drawn-widgets-no-keyboard-focus`.

## Decision

Filed to Backlog (no code changed): **UI-16**, **UI-17**, **UI-18** (bugs, each needs a regression
test + fix-log when fixed); **UX-07** (size L — redesign of mark language, needs
`features/board-marks/` design gate first); **UX-08** (hover/tooltip UX, depends on UX-07).
