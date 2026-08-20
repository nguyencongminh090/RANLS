# Current sprint

## Sprint 1

**Goal:** Fix the P0 memory-safety/data-correctness cluster surfaced by the 2026-08-21 `src/` review,
in dependency order, starting with the test harness that later items need for regression coverage.
**Dates:** 2026-08-21 to — (open — no fixed end date set yet)

**Committed items** (in required order — `STATE-01` and `RT-01` are hard-blocked by the item before
them per their `docs/instruction/<CODE>-*.md` entries; `PROTO-01` is priority-ordered second as the
other P0 memory-safety item, not a technical dependency of `TEST-01`):

| Order | CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|---|
| 1 | TEST-01 | Stand up test infrastructure (header-only, model/protocol only, no display server) | — | — | Active |
| 2 | PROTO-01 | Harden Gomocup parser: OOB `currentPVs_[-1]`, unbounded `NUMPV` resize, unvalidated DB coords | — (priority only) | — | Active |
| 3 | STATE-01 | Stale PV/status/board markers survive New Game, makeMove, undo/redo | TEST-01 | — | Active |
| 4 | RT-01 | Throttle the 6 unthrottled engine→UI emit sites | STATE-01 | — | Active |

Points not yet estimated. Work items in this order — don't start `STATE-01` before `TEST-01`'s
harness lands, or `RT-01` before `STATE-01`'s shared reset path exists; see each item's
`docs/instruction/` entry for why. Dispatch each with `/implement-task <CODE>`.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
