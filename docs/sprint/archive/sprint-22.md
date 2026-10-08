# Sprint 22 (closed 2026-10-08)

**Goal:** Board mark visual language: stacked engine/database/variant marks, best ring, hover tooltips
**Dates:** 2026-10-06 to 2026-10-08.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| UX-07 | Redesign board mark language (engine / database / variant / best) | ✅ DONE |
| UX-08 | Board hover crosshair, margin highlight, mark tooltips | ✅ DONE |

Points were not estimated. Also shipped during the sprint window, outside the committed scope: UI-21 (modern chrome, PR #55), UI-19 / UI-20 (PR #51), a steeper heatmap scale (PR #52) and two winrate-scale fixes (PRs #53, #54, no CODE).

## What shipped

- **UX-07** (PR #49 squash `8e33e1f`): `BoardViewModel` gained a Coord→move-number table (replaces the per-stone `std::find`), database `isBest` and variant `branchCount`; renderer layer order grid → stones → last move → variant → database → engine (+best ring) → forbidden → PV ghost → hover, outlined database diamond, variant ring + dot with count badge. 17 new cases.
- **UX-08** (PR #50 squash `c8b59c4`): `BoardViewModel::tooltipFor` + `BoardView` query-tooltip, hover crosshair, bold margin labels; `hasComment` confirmed to be a flag only.
- **UI-19 / UI-20** (PR #51): best ring clears the winrate tag text; stone numbers centred (bearing-aware) and bold; Sabaki-style winrate heatmap. **PR #52:** steeper heatmap scale.
- **Fixes without a CODE** (PRs #53, #54): keep the engine's `INFO WINRATE` instead of re-deriving it from cp; database marker colour follows the engine's label (`docs/fix-log/2026-10-08-*.md`).
- **UI-21** (PR #55 squash `a5529c8`): icon header bar, hamburger menu, CSS tokens.

## Lessons

- Visual checks are still human-owed: marks, tooltips, the heatmap and the new chrome were verified by offscreen render / Xvfb only, not in a live window; light theme and Windows unverified for UI-21.
- Scale assumptions leak: a hardcoded cp→winrate scale (200) disagreed with the engine's (~116) in two places. Prefer the engine's own value/label over re-deriving.
- `isBest` still picks the highest raw `value`; given the engine labels use `-value` this may favour the wrong side (not investigated).
- Work shipped outside the committed list (UI-19/20/21) makes the Active table understate the sprint; file the CODE before the PR next time.

## Rolled over to Backlog

Nothing rolled over — all committed items finished.

## Next sprint

Sprint 23 — command palette (Ctrl+K), opened right after this close. Release `v0.9.0` cut at close (see `CHANGELOG.md`).
