# Board marks: a clear visual language for engine, database, variant and best-move info (UX-07, UX-08)

## Background

The 2026-10-06 review (`docs/audit/2026-10-06-board-marks-ux-review.md`) found that the board draws
four kinds of per-cell information — live engine search (tag / best / lost / examined / examining),
database entries, variation-tree branches, and Renju forbidden points — but they compete for the same
cell and the same colour scale. Concretely (all `src/ui/board_renderer.cpp`, `src/model/board_view_model.cpp`):

- The best root move is resolved *below* its own winrate tag, so it is rarely visible (→ UI-18).
- A database diamond and an engine tag on the same cell: the tag is drawn later and hides it.
- `DatabaseEntry` carries `depth`, `bound`, `hasComment` (`analysis_types.h:40`) but the marker uses
  only `value` and the label text — confidence and "has comment" never reach the board.
- Both engine and database use the same red→green HSV heat; not colour-blind safe.
- Variation branches are an unlabeled dot; nothing says "main line" vs "side branch" or how many.
- No tooltip, no hover crosshair; move numbers are looked up in O(n²) each frame.

## Actors

- **Analyst** — studies a position with the engine running, and wants to see at a glance *which move
  is best, how good each candidate is, and what the opening database says*, without one source hiding
  another.
- **Player vs. engine** — mostly wants last-move and forbidden-point clarity; must not be distracted
  by analysis clutter (existing toggles stay).
- **Colour-blind user** — must be able to read evaluation without relying on red/green hue.

## Stories

1. As an analyst, when the engine is searching I see the best move marked distinctly *and* still see its
   winrate tag.
2. As an analyst, a cell that has both an engine candidate and a database entry shows both, readable.
3. As an analyst, I see which database entry is the database's best on the board; depth, bound and any
   comment appear in the hover tooltip, not on the board (Q4).
4. As an analyst, I see on the board which cells continue the game tree (variants) and how many.
5. As any user, hovering a mark explains it (source, value, depth); hovering a cell highlights its
   row/column labels; hovering an occupied cell does not paint a stone on it.
6. As a colour-blind user, evaluation is readable from the numeric label, not hue alone (HSV kept — Q5).

## Non-goals

- Keyboard navigation / accessible roles (accepted limitation, `docs/audit/2026-08-21-custom-drawn-widgets-no-keyboard-focus.md`).
- Engine protocol or analysis-data changes (UX-07 consumes `AnalysisOverlay` / `DatabaseEntry` as is).
- Stone texture/shadow polish (cosmetic; separate if wanted).
- The three P0 bugs UI-16/17/18 ship first as stand-alone fixes; this folder is the redesign beyond them.
