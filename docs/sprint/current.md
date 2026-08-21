# Current sprint

## Sprint 3

**Goal:** Clear the P1 backlog surfaced by the 2026-08-21 `src/` review — wrong results and wasted
work: hardcoded board size, unbounded PV-slot growth, tree-view rebuild cost, `undoAll`/`redoAll`
flooding, and win-rate/tree-table attribution errors.
**Dates:** 2026-08-21 to — (open — no fixed end date set yet)

**Dependency graph:** all six items' own detail files cross-reference each other only
informationally (shared root causes, related symptoms), not as blocking dependencies — no item's
"Related" section says it depends on another. They can be dispatched in parallel, same as Sprint 2.
Two pairs share a root cause and may be worth eyeballing together even though neither blocks the
other: RT-04 ↔ UI-02 (both about the tree views), and UI-01 ↔ UI-02 (both about eval values shown
in the tree table).

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| PROTO-02 | Hardcoded board size 15 in coordinate parsing, `Best:` readout, and star points — breaks every non-15×15 board | — | — | Active |
| STATE-03 | `currentPVs_` never shrinks and materialises empty PV slots rendered as garbage rows | — | — | Active |
| RT-04 | Both tree views fully rebuild many times per second during analysis; `layoutTree` is O(n²) | — | — | Active |
| NAV-01 | `undoAll`/`redoAll` send one database query and rebuild the whole UI per ply | — | — | Active |
| UI-01 | Win-rate graph attributes evals to the wrong side (off by one ply); evals can go unrecorded | — | — | Active |
| UI-02 | Tree "Table" tab can't click-to-jump and shows no current path; the two tree views disagree | — | — | Active |

Points not yet estimated. Dispatch each with `/implement-task <CODE>`. Since all six are
independent, they may be run concurrently — if dispatching more than one at once, use isolated git
worktrees per agent (the `Agent` tool's `isolation: "worktree"`).

**Lesson carried over from Sprint 2** (see `docs/sprint/archive/sprint-2.md`'s "Process note"):
commit all finished work — including `tests/` and any uncommitted fixes sitting in the working
tree — *before* dispatching parallel worktree agents, so each starts from a consistent, complete
base instead of independently working around a stale/incomplete copy.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
