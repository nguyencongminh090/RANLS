# Sprint 2 (closed 2026-08-21)

**Goal:** Clear the remaining P0 backlog surfaced by the 2026-08-21 `src/` review — engine-state
honesty, unbounded log growth, PVView rebuild breaking hover, and the settings dialog silently
dropping config fields.
**Dates:** 2026-08-21 to 2026-08-21 (same-day close — all four items dispatched in parallel via
isolated worktree agents and reconciled onto `main` in one working session).

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| STATE-02 | Settings dialog silently resets `multiPV` and wipes `customParams`, then persists it | ✅ DONE |
| ENG-01 | Engine state is dishonest ("● ON" with no process, crash ≡ never-started, no "thinking" state) and stopping blocks the UI ~2.5s | ✅ DONE |
| RT-02 | Engine log grows unbounded and writes per-line; gutter labels desync on wrap | ✅ DONE |
| RT-03 | PVView full rebuild destroys hover, breaking the board PV ghost-stone preview during analysis | ✅ DONE |

Points were never estimated this sprint. Final burndown row (before `docs/sprint/burndown.md` was
reset for Sprint 3): `2026-08-21 | 0 / 4 items | — | All four items landed same-day`.

## What shipped

- **STATE-02:** `SettingsDialog::onApply` now copies from the config it was opened with instead of
  default-constructing, so `customParams`/`showDatabase` survive Apply. Added a `multiPV` spin
  button and extended `SettingsStorage::save`/`load` to actually persist `customParams`, which
  previously wasn't written to disk at all. See
  `docs/fix-log/2026-08-21-state-02-settings-dialog-drops-config-fields.md`.
- **ENG-01:** replaced `EngineController`'s `started_`/`analyzing_` bool pair with an explicit
  `EngineState` enum (`NotStarted/Starting/Idle/Analyzing/Stopping/Crashed`), honored
  `EngineProcess::start()`'s previously-discarded return value, and made engine shutdown fully
  non-blocking — removed both the `g_usleep` waits and the re-entrant `g_main_context_iteration`
  pump from the stop path, replacing them with an async grace-period race guarded by weak_ptr
  lifetime markers. Crash is now actively announced via an inline banner. See
  `docs/fix-log/2026-08-21-eng-01-engine-state-honesty-and-blocking-stop.md`.
- **RT-02:** replaced the dual-TextView gutter+content Engine Log with a single tagged-prefix
  `TextView` (removing the wrap-mode desync at its root) backed by a bounded, batched
  `EngineLogModel` (default cap 5000 lines) instead of unbounded per-line inserts. See
  `docs/fix-log/2026-08-21-rt-02-engine-log-unbounded.md`.
- **RT-03:** `PVView::update()` now reuses row widgets in place instead of destroying and
  recreating every row (and its hover motion controller) on each analysis update, fixing the
  PV-hover-to-board-ghost-stone preview flicker during active search. See
  `docs/fix-log/2026-08-21-rt-03-pvview-rebuild-breaks-hover.md`.

## Process note

All four items were dispatched concurrently to isolated agents in git worktrees. None of the
agents could see `tests/` or each other's work during implementation (it was uncommitted on `main`
at dispatch time), so each worked around it independently, and one (STATE-02's agent) even copied
uncommitted state across the worktree boundary via file reads to get a full test run. Reconciling
required manually merging four sets of changes plus the pre-existing uncommitted Sprint 1 work onto
`main`, one fix at a time, with a full build + test run after each merge. Lesson for next time:
commit finished work (including `tests/`) before dispatching parallel worktree agents, so each
starts from a consistent, complete base.

## Rolled over to Backlog

Nothing rolled over — all four committed items finished within the sprint.

## Next sprint

Sprint 3 pulls the P1 backlog items (PROTO-02, STATE-03, RT-04, NAV-01, UI-01, UI-02) into Active —
see `docs/sprint/current.md`.
