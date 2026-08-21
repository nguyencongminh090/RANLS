# Current sprint

## Sprint 4

**Goal:** Clear the remaining P2/P3 backlog from the 2026-08-21 `src/` review — usability and
hygiene: rule not reflected on the board, blank empty states, unvalidated settings, accessibility
and destructive-action gaps, unverified board-size extremes, and leaked dialogs/dead code.
**Dates:** 2026-08-21 to — (open — no fixed end date set yet)

**Dependency graph:** all six items' own detail files don't declare a blocking dependency on each
other. They can be dispatched in parallel, same as Sprint 2 and Sprint 3.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| UI-03 | Selected rule (Renju/Standard) has no effect on what the board shows | — | — | Active |
| UX-01 | Three panels render as blank rectangles instead of empty states | — | — | Active |
| UX-02 | Settings dialog accepts an invalid engine path with no feedback | — | — | Active |
| UX-03 | Unlabelled icon buttons, no focus indication on custom-drawn widgets, no confirmation before destroying a game | — | — | Active |
| UX-04 | Board rendering never verified at the extremes of the supported 5–22 range (investigation) | — | — | Active |
| CLEAN-01 | Leaked dialogs, dead signals, leftover debug output, duplicated constant | — | — | Active |
| UX-05 | `Gtk::Paned` divider position doesn't rescale when the window is resized back up after being shrunk, leaving the board squeezed | — | — | Active |

Points not yet estimated. Dispatch each with `/implement-task <CODE>`. UX-05 was surfaced by UX-04's
own investigation (not the original 2026-08-21 review) and pulled straight into Active rather than
sitting in Backlog, since the other six items in this sprint are already done and it's a small,
independent follow-up in the same area.

**Lesson carried over from Sprint 2** (see `docs/sprint/archive/sprint-2.md`'s "Process note"):
commit all finished work — including `tests/` and any uncommitted fixes sitting in the working
tree — *before* dispatching parallel worktree agents, so each starts from a consistent, complete
base instead of independently working around a stale/incomplete copy.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
