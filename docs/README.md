# docs/ map

See `/CLAUDE.md` ("Process model") for the workflow this layout supports. Quick reference:

| Path | Purpose |
|---|---|
| `docs/todo/<CODE>-<slug>.md` | Detail files for `/TODO.md` index lines |
| `docs/instruction/<CODE>-<slug>.md` | Detail files for `/instruction.md` index lines |
| `docs/design/*.md` | Design docs referenced from todo/instruction entries |
| `docs/sprint/current.md` | Active sprint: goal, dates, committed items |
| `docs/sprint/burndown.md` | Daily remaining-points table for the active sprint |
| `docs/sprint/archive/sprint-N.md` | Closed sprints |
| `docs/audit.md` + `docs/audit/<date>-<slug>.md` | Reviews/decisions (architecture, security, protocol compat) |
| `docs/fix-log.md` + `docs/fix-log/<date>-<slug>.md` | Bug-fix log, append-only |
| `docs/notes/<date>-<slug>.md` | Freeform brainstorm/discussion, no fixed structure |

`../features/<slug>/` (repo root, not under `docs/`) holds pre-implementation design discussion —
see `/CLAUDE.md`.

## Scripts

| Command | Purpose |
|---|---|
| `node scripts/check-task-structure.js` | Lints `TODO.md`/`instruction.md`: duplicate codes, malformed index lines, broken/orphaned links to `docs/todo,instruction/` detail files. |
| `node scripts/check-tracking-sync.js --full` | Audits the whole backlog for `✅`-marked `TODO.md` items whose detail file lacks a completion marker (`--hook` mode exists for wiring as a `Stop` hook — not currently wired, see `.claude/rules/tracking-files.md`). |
| `node scripts/assign-task.js <CODE> [--model="<tier>"] [--owner="<name>"]` | Moves a `TODO.md` item from Backlog to Active and stamps it with a model/owner tag; re-running on an Active item updates the tags in place. |
