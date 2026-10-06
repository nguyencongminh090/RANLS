# Board marks — planning

See [user_story.md](user_story.md) · [diagram/flow.md](diagram/flow.md).

**Status: RESOLVED 2026-10-06** — see "Resolution" (two sections). Q12 is resolved as an investigation step inside UX-08. Not authorization to implement. Resolve with the user, then
write `docs/instruction/UX-07-*.md` and refine `docs/todo/UX-07-board-mark-visual-language.md` /
`UX-08-*`. UI-16/17/18 are independent and may ship before this.


## Resolution — 2026-10-06 (user, partial)

- **Q1 → Best = highlight.** Marks only ever sit on *empty* cells (stones are never stacked on); the
  point of Q1/Q2 is only that Best and the winrate tag of the *same candidate cell* no longer hide each
  other. "Kind" = the `SearchOverlayMark::Kind` enum (Tag/Lost/Best/Examined/Examining), i.e. the one
  type a cell currently resolves to. Default stands: `isBest` flag drawn as a ring over the tag.
  User note: PV ghosts normally don't overlap stones, so UI-16's ghost half is low-risk; its hover-on-
  occupied half still applies.
- **Q4 → board shows eval text only** — no `bound` / `hasComment` glyphs. Comment, depth and bound go in
  the **hover tooltip** (UX-08). See Q12 for where the comment text comes from.
- **Q5 → keep HSV** (no new ramp, no helper rewrite). Colour-blind safety relies on the numeric label
  already drawn on every tag/DB mark (kept with its dark shadow). Story 6 is reworded accordingly.
- **Q11 → highlight only** (the Best ring). No arrow, no persistent PV trail.

Consequences: UX-07 shrinks — no new colour ramp, no on-board DB glyphs for bound/comment. Remaining
UX-07 work = stacking model (Q2), DB outline/badge placement (Q3), variants (Q6), index map (Q8).

### New question

| # | Question | Proposed default | Alternatives |
|---|---|---|---|
| Q12 | **Tooltip comment source.** `DatabaseEntry` has only `hasComment` (bool, `analysis_types.h:46`); comment *text* lives on `VariationTree` nodes (`variation_tree.h:35`). Where does a database entry's comment come from? | Check how `hasComment` is populated (loader/engine); if it maps to a tree node, the ViewModel resolves the text for the tooltip; if the text is not available, tooltip shows "has comment" only. Investigate before designing UX-08. | Add a text field to `DatabaseEntry` (model change, needs the source to provide it). |

## Resolution (final) — 2026-10-06 (user: "use the defaults")

Q2, Q3, Q6, Q7, Q8, Q9, Q10 → the **Proposed default** in the table below, unchanged. Q12 → the proposed
default: UX-08 starts by checking how `hasComment` is populated; the tooltip shows the comment text if it
resolves to a `VariationTree` node, else just "has comment" — no `DatabaseEntry` model change without a
new decision. Decisions of record: Q1/Q4/Q5/Q11 per the partial resolution above; the rest per defaults.

**Q12 resolved 2026-10-06 (investigation):** `hasComment` is only a flag parsed from the engine's `DATABASE` row; comment text is never sent and `VariationTree` comments are unrelated user annotations. Tooltip says "has comment" only; no `DatabaseEntry` change.

## Open questions

| # | Question | Proposed default | Alternatives |
|---|---|---|---|
| Q1 | **Best move representation.** | A flag (`isBest`) on the mark, drawn as a ring *over* the tag; `Best` stops being a priority-rank `Kind`. | Keep as `Kind` but raise above tag (hides the best move's winrate — rejected). Star glyph instead of ring. |
| Q2 | **Model shape.** `SearchOverlayMark` is single-winner per cell. | Replace with a per-cell struct holding independent optional parts (engine tag/pos state, best flag, lost flag) so channels stack; ViewModel stays the only place that resolves. | Keep single-winner and only add `isBest` (smaller, solves UI-18 but not the stacking goal). |
| Q3 | **Database mark.** | Outlined (stroke-only) diamond; when it shares a cell with an engine disc it shrinks to a corner badge. Best DB entry (highest `value`) gets a thicker outline. | Filled diamond with different ramp; separate on/off already exists (`showDatabase`). |
| Q4 | **What DB info to surface.** `DatabaseEntry` has `depth`, `bound`, `hasComment` unused on the board. | Board: eval text + outline style for `bound` (solid exact, dashed bound) + tiny dot if `hasComment`. Depth in tooltip only. | Show depth on the board (clutter on small cells). |
| Q5 | **Colour ramp.** | Perceptual, luminance-monotonic ramp (viridis/cividis-like) for both sources + always a numeric label; shared helper replaces `set_source_from_winrate`. | Keep HSV but add pattern/shape per band; user-selectable palette (more settings). |
| Q6 | **Variants.** | Ring+dot; show count when > 1; main-line continuation distinguished from side branches (needs `getBranchCoords` to expose which is main — check `VariationTree`). | Keep plain dot, add only tooltip. |
| Q7 | **Hover (UX-08).** | Hover only on empty cells; faint row/column crosshair; margin coordinate labels highlighted; `query-tooltip` text built by the ViewModel from the same marks. | Tooltip only; no crosshair. Crosshair as a setting. |
| Q8 | **Move-number lookup.** | Build `Coord → index` once in `update()` (O(n)), ViewModel field. | Leave; fix only if profiled (`perf-optimization` says measure first). |
| Q9 | **Testing seam.** There is no renderer/ViewModel test today (`tests/` has none for the board). | Test the ViewModel (mark resolution, tooltip text, index map) in the GTK-free `ranls-gui-tests`; renderer stays visually verified (screenshots in the fix-log/PR). | Add a Cairo image-snapshot test (brittle across fonts/themes). |
| Q10 | **Settings.** Existing toggles: `showDatabase`, `showSearchOverlay`, `showSearchWinrate`, `showMoveNumbers`. | No new toggles in the first pass; add one only if clutter is reported. | Per-channel toggles for variants and DB badges. |
| Q11 | **Scope of "best move track".** The brief's wording is ambiguous. | Interpret as "best move clearly marked on the board" (Q1). | Also a persistent best-move trail/arrow (PV path drawn without hover) — a separate follow-up if wanted. |

## Proposed sequencing (one PR each, `github` skill)

1. **UI-18/16/17** bug fixes (already filed) — independent, first.
2. **UX-07a** model: per-cell stacking struct, index map, shared colour-ramp helper + ViewModel tests.
3. **UX-07b** renderer: new layer order and glyphs; screenshots in the PR.
4. **UX-08** hover crosshair + tooltips (after UX-07).
CODE format forbids letter suffixes — split into real CODEs when sprint-planning (see
`features/extract-mainwindow-orchestration/planning.md` precedent).

## Risks

- Overlay redraw runs at engine-update rate (RT-01 throttled); the stacked model must not add per-frame
  allocation — measure before/after (`perf-optimization`).
- Small boards/cells (22×22): corner badges and counts must degrade (hide below a cell-size threshold).
