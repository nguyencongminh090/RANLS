# Sprint 4 (closed 2026-08-30)

**Goal:** Clear the remaining P2/P3 backlog from the 2026-08-21 `src/` review — usability and
hygiene: rule not reflected on the board, blank empty states, unvalidated settings, accessibility
and destructive-action gaps, unverified board-size extremes, and leaked dialogs/dead code.
**Dates:** 2026-08-21 to 2026-08-30 (all seven items landed on `main` 2026-08-21; sprint left open
administratively with no end date until closed today during a leftover-task sweep).

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| UI-03 | Selected rule (Renju/Standard) has no effect on what the board shows | ✅ DONE |
| UX-01 | Three panels render as blank rectangles instead of empty states | ✅ DONE |
| UX-02 | Settings dialog accepts an invalid engine path with no feedback | ✅ DONE |
| UX-03 | Unlabelled icon buttons, no focus indication on custom-drawn widgets, no confirmation before destroying a game | ✅ DONE |
| UX-04 | Board rendering never verified at the extremes of the supported 5–22 range (investigation) | ✅ DONE |
| CLEAN-01 | Leaked dialogs, dead signals, leftover debug output, duplicated constant | ✅ DONE |
| UX-05 | `Gtk::Paned` divider position doesn't rescale when the window is resized back up after being shrunk, leaving the board squeezed | ✅ DONE |

Points were never estimated this sprint, same as Sprint 3. Final burndown row (before
`docs/sprint/burndown.md` was reset for Sprint 5): `2026-08-30 | 0 / 7 items | — | Closing entry;
all seven items had already landed 2026-08-21, burndown just wasn't updated in between`.

## What shipped

- **UI-03:** Added a persistent header-bar rule indicator, a new `RenjuRule` domain module marking
  Black's forbidden points without blocking the click, and made `BoardState::checkWin()` rule-aware.
  See `docs/fix-log/2026-08-21-ui-03-rule-not-visible-on-board.md`.
- **UX-01:** Added a shared `EmptyState`/`EmptyStateOverlay` helper so `WinGraphView`, `PVView`,
  `TreeNodeView`, `TreeExplorer`, and the Move/Engine logs show a specific placeholder instead of a
  blank rectangle. See `docs/fix-log/2026-08-21-ux-01-empty-states.md`.
- **UX-02:** Engine path in Settings is now validated live (exists/regular file/executable) with an
  inline status label, Apply desensitized while invalid. See
  `docs/fix-log/2026-08-21-ux-02-settings-dialog-engine-path-validation.md`.
- **UX-03:** Icon-only nav buttons got tooltip + accessible label; New Game and board-size Apply now
  confirm before discarding a non-empty game; fixed a coordinate-label/board-background contrast
  failure (~1.55:1 → ~8.0:1) and a database-marker-label contrast failure. See
  `docs/fix-log/2026-08-21-ux-03-accessibility-and-destructive-actions.md`.
- **UX-04:** Investigated board rendering at the 5–22 size extremes; fixed coordinate-label clipping
  at 5×5 and 3-digit move-number overflow at 22×22; unified the duplicated board-geometry formula
  into `BoardRenderer::computeGeometry()`. See
  `docs/fix-log/2026-08-21-ux-04-board-geometry-and-overflow.md`.
- **CLEAN-01:** Fixed 3 leaked `new`-allocated dialogs in `main_window.cpp`, removed leftover
  `[DBG]` output and an unused local, unified the duplicated `kCoordMargin` constant. See
  `docs/fix-log/2026-08-21-clean-01-dialog-leaks-and-dead-code.md`.
- **UX-05:** `mainHPaned_`/`mainVPaned_` divider positions are now tracked as a fraction of the
  pane's own extent and reasserted on every window allocation. See
  `docs/fix-log/2026-08-21-paned-resize-does-not-restore.md`.

## Rolled over to Backlog

Nothing rolled over — all seven committed items finished within the sprint.

## Process note

All seven items landed same-day (2026-08-21), but the sprint itself was never formally closed and
`docs/sprint/burndown.md` was never updated in between despite `CLAUDE.md`'s "Sprint cadence"
instruction to update it "when an Active item's status changes, not just at sprint end." Both drifts
were only caught during a 2026-08-30 leftover-task sweep, which also surfaced four new items
(IO-01, DOC-01, TOOL-01, CLEAN-02) now committed to Sprint 5. Lesson: close a sprint (or at least
update its burndown) as soon as its last Active item lands ✅, not only when something else prompts
a review.

## Next sprint

Sprint 5 pulls four newly-surfaced leftover items (IO-01, DOC-01, TOOL-01, CLEAN-02) into Active —
see `docs/sprint/current.md`.
