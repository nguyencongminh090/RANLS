---
paths:
  - "docs/sprint/**"
  - "CHANGELOG.md"
  - "TODO.md"
---

# Sprint cadence

- `docs/sprint/current.md` — the active sprint: goal, start/end date, committed `CODE`s pulled from
  `TODO.md`'s Backlog into its Active section, story points per item.
- `docs/sprint/burndown.md` — one row per day: date, points remaining, ideal-line reference. Update
  it when an Active item's status changes, not just at sprint end. Ask the user before rendering it
  as a chart artifact (cheap to do on request — don't do it unprompted every update).
- `docs/sprint/archive/<sprint-N>.md` — closed sprints: final burndown, what shipped, what rolled
  over to the next sprint's Backlog (rolled-over items keep their original `CODE`).
- Starting a new sprint = one edit: snapshot `current.md` into `archive/sprint-N.md`, reset
  `current.md` for the next sprint, pull newly-committed items from `TODO.md` Backlog into Active.
- Closing a sprint also cuts a release: finalize `CHANGELOG.md`'s `[Unreleased]` into a `v0.N.0`
  section and tag it — see the `github` skill "Cutting a release" checklist.
