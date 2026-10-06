# Instruction — UX-08: board hover crosshair, margin highlight, mark tooltips

Detail: [docs/todo/UX-08-board-hover-crosshair-and-tooltips.md](../todo/UX-08-board-hover-crosshair-and-tooltips.md)
Design of record: [features/board-marks/planning.md](../../features/board-marks/planning.md) (Q7, Q9, Q12).
UX-07 and UI-16/17/18 are on `main` — build on them (`BoardViewModel::isOccupied`, `moveNumberAt`,
`Marker::isBest/branchCount`, the `layer()` wrapper with `begin_new_path()`).

## Decisions already made — do not re-open

- **Q12 investigated 2026-10-06:** `DatabaseEntry::hasComment` is only a 0/1 flag parsed from field 7 of the
  engine's `DATABASE` row (`gomocup_protocol.cpp` ~L1061–1091). The comment *text* is never sent, and
  `VariationTree` node comments are the user's own annotations on played moves, a different thing. So the
  tooltip says "has comment" and **never** shows text. No `DatabaseEntry` change, no tree lookup.
- Tooltip text is built by the **ViewModel** (GTK-free, unit-testable), shown via GTK `query-tooltip`.
- Hover crosshair only; no warning cursor, no keyboard navigation.
- No new settings in this pass.

## Approach

1. **Model — `BoardViewModel::tooltipFor(Coord) const -> std::string`** (empty = no tooltip). Compose lines
   from what is at the cell, using the render state plus `state_` where depth/bound/value are needed:
   - Engine mark (live search only, matches `searchOverlay`): tag text + winrate, "best move" if `isBest`,
     "losing move" for Lost, tag depth if available (`AnalysisOverlayCell::tagDepth`); Examined/Examining
     marks → a short status line or nothing (don't clutter).
   - Database entry (`state_.database()`): label/boardText, value, depth, bound as text (Exact/Alpha/Beta,
     `bound` 0/1/2 — see `analysis_types.h`), "best database move" if flagged, and "has comment" when
     `hasComment` (no text).
   - Variant marker: "N variations" using `branchCount`.
   - Forbidden point (Renju, `forbiddenPoints`): "Forbidden for Black (still playable)" — wording must keep
     the UI-03 "indication only" meaning.
   - Occupied cell or off-board → empty string.
2. **View — `BoardView`**: `set_has_tooltip(true)` + `signal_query_tooltip` → `pixelToCoord` → `vm_.tooltipFor`;
   return false (no tooltip) when empty. Keep tooltips cheap: no per-motion allocation beyond the string.
3. **Renderer — crosshair + margin highlight**: for the hovered cell (valid, empty — occupied cells follow
   the UI-16 rule and get no hover stone; decide and test whether the crosshair is still drawn there; default:
   draw it on empty cells only) draw a faint full-row/column line through it as a new layer **below** stones,
   and emphasise the hovered column letter and row number in the margin (the labels are drawn in `drawGrid`;
   add the highlight there or in a small separate layer). Must go through the `layer()` wrapper; reset paths
   per mark if a loop mixes text and arcs (UX-07 finding).
4. **Tests** (pattern: `test_ux07_*`, `test_ui16_hover_occupied.cpp`): GTK-free model tests for every tooltip
   branch (engine tag/best/lost, DB with each `bound`, hasComment, variants, forbidden, occupied → empty); render
   tests that the crosshair changes pixels along the hovered row/column and that the hovered margin label differs
   from the unhovered one. The `query-tooltip` wiring itself can only be checked with a real widget — if a
   ui-test can construct `BoardView` headlessly do so, otherwise state "tooltip wiring not covered by an automated
   test" explicitly.

## Pitfalls

- `ui → model` only; the renderer/view never touch `GameState`. `tooltipFor` lives in the model.
- Tooltips must not change click behaviour; forbidden points stay playable (UI-03).
- Don't mutate `hoverMove` semantics: `BoardView` still records it on occupied cells (UI-16 made the renderer the
  guard). Don't re-open that.
- Don't make the tooltip depend on `showSearchOverlay` being on for database/variant info — respect the existing
  toggles only for what each toggle controls (`showDatabase` for DB, `showSearchOverlay` for engine marks).
- No bound/comment glyphs on the board (Q4) — that info is tooltip-only.
- Redraw cost: crosshair redraw already happens on `hoverMove` change (queue_draw); don't add extra redraws.
- Keep the UI-18 ring, UX-07 badges and labels unchanged; check the crosshair does not hide marks (draw below).
