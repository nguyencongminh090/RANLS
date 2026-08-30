# UI-08 — Remove empty-state placeholder text; keep panels visually clean

**Status:** 📋 BACKLOG
**Area:** analysis panel / win graph / PV view / tree views / bottom panel (`src/ui/`)
**Priority:** P3
**Source:** UI review request, 2026-08-30

## Problem / request

UX-01 (shipped 2026-08-21) added placeholder strings to every data-driven panel when it has no
data — "No analysis yet — press Analyze (F5)", "No moves yet", etc. The user now wants these
removed: an empty panel should just be **empty (clean)**, no instructional text.

This is a deliberate reversal of the UX-01 acceptance criterion "Each data-driven panel shows a
brief, specific placeholder when empty". UX-01's contrast work and its STATE-01 dependency stay
relevant history; only the visible copy changes.

## Acceptance criteria

- No "No … yet" / "No data" placeholder text renders in any panel in the idle / no-data state:
  PV view, move log, engine log, both tree views, analysis panel.
- Panels in the empty state render as clean empty regions consistent with the app's panel styling
  (no stray half-drawn scaffold, no leftover label widget taking vertical space).
- Real data still appears normally when analysis runs, and the panels return to clean-empty after
  New Game (still depends on STATE-01 clearing + notifying).

## Open question for the WinGraph

UX-01 also made `WinGraphView` draw its 0/50/100% axis scaffold when empty so it "reads as an empty
chart rather than a void". Decide with the user whether the axis scaffold counts as "placeholder"
to remove, or stays (it is structural, not instructional). Default assumption: **keep the axis
scaffold, remove only text**. Confirm before implementing. Coordinate with UI-09 (same widget).

## Scope boundary

- Text/visibility removal only — do not restyle panel backgrounds, borders, or layout.
- Do not touch the STATE-01 clear/notify path.

## Related

- Reverses part of UX-01 (`docs/todo/UX-01-empty-states.md` / `docs/fix-log/2026-08-21-ux-01-empty-states.md`)
- UI-09 (win-graph changes — same widget, land together or sequence deliberately)
