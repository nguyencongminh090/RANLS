# Current sprint

## Sprint 2

**Goal:** Clear the remaining P0 backlog surfaced by the 2026-08-21 `src/` review — engine-state
honesty, unbounded log growth, PVView rebuild breaking hover, and the settings dialog silently
dropping config fields.
**Dates:** 2026-08-21 to — (open — no fixed end date set yet)

**Dependency graph:** all four items are independent of each other and of Sprint 1's work — no
ordering constraint found in any item's own detail file. They can be dispatched in parallel.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| STATE-02 | Settings dialog silently resets `multiPV` and wipes `customParams`, then persists it | — | — | ✅ Done |
| ENG-01 | Engine state is dishonest ("● ON" with no process, crash ≡ never-started, no "thinking" state) and stopping blocks the UI ~2.5s | — | — | ✅ Done |
| RT-02 | Engine log grows unbounded and writes per-line; gutter labels desync on wrap | — | — | ✅ Done |
| RT-03 | PVView full rebuild destroys hover, breaking the board PV ghost-stone preview during analysis | — | — | ✅ Done |

All four items landed 2026-08-21, dispatched in parallel via `/implement-task` in isolated
worktrees and reconciled by hand onto `main` (see each item's fix-log entry). Sprint 2 is
functionally complete — see `docs/sprint/burndown.md` and the "Sprint cadence" section of
`/CLAUDE.md` for closing it out (archive + reset `current.md`) when ready to open Sprint 3.

Points not yet estimated. Dispatch each with `/implement-task <CODE>`. Since all four are
independent, they may be run concurrently — if dispatching more than one at once, use isolated git
worktrees per agent (the `Agent` tool's `isolation: "worktree"`), same rationale as Sprint 1's
"Running two at once" note: avoid two agents silently clobbering shared files (e.g.
`tests/CMakeLists.txt`) in one working tree. Merge/reconcile by hand afterward if more than one
touches the same shared file.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
