# Agent management

How to pick a subagent/model for a task, and how tracked work gets dispatched to one.

## Model tiers by task shape

Tag `TODO.md`/`docs/todo/<CODE>-*.md` items with `[Model: <tier>]` when filing them, as a
suggestion re-read at pickup time (not at filing time — scope may have narrowed since):

- **Haiku 4.5** — pure measurement/verification with no judgment call: confirm a build flag,
  reproduce a reported crash, check a protocol response against `docs/protocol.md`.
- **Sonnet 5** — implementation with clearly scoped boundaries per this repo's conventions: a
  `TODO.md` item whose `instruction.md` entry already states the approach.
  This is the tier `/implement-task` dispatches to by default.
- **Opus 5** — architecture/protocol-compatibility decisions with real tradeoffs, multi-round
  root-cause diagnosis (engine subprocess hangs, GTK threading races, protocol desync) where an
  early fix is likely to patch the visible symptom rather than the real layer.

## Subagent types (this harness)

- **Explore** — locating code, "where is X", quick-to-medium codebase surveys. Read-only.
- **general-purpose** — the default for dispatched `TODO.md` implementation work (build, edit,
  test, report back). What `/implement-task` uses unless a todo file names something more specific.
- **Plan** — architecture/implementation-strategy design before formalizing a `features/<slug>/`
  entry into a `TODO.md` item.

## Assigning a Backlog item to the current sprint

`node scripts/assign-task.js <CODE> --model="<tier>" --owner="<name>"` moves the item from
`TODO.md`'s Backlog to Active and stamps it — this is the "commit to sprint" step, done before
dispatching. Re-running it on an already-Active item updates the tags in place (reassign). After
assigning, also add the item to `docs/sprint/current.md`'s committed-items table (the script prints
a reminder). Run `node scripts/check-task-structure.js` after manually editing `TODO.md`/
`instruction.md` by hand — it catches duplicate codes, malformed index lines, and broken/orphaned
links to detail files that `check-tracking-sync.js` doesn't check.

## Dispatch flow: `/implement-task <CODE> [project-dir]`

Reads `TODO.md`'s Active section + `docs/todo/<CODE>-*.md` + `docs/instruction/<CODE>-*.md` (and
any linked `docs/design/*.md`), then dispatches one bounded subagent — in an isolated worktree
branch — with the scope, boundaries, and verification criteria spelled out. The agent implements +
commits + updates tracking files **on the branch** and stops; the orchestrator then verifies and
drives the `github` skill PR lifecycle (push → PR with label + milestone → squash-merge → sprint/
board sync). Work never lands as a hand commit on `main` (the Sprint 7 anti-pattern). See
`.claude/commands/implement-task.md` for the exact contract. `project-dir` defaults to `.` (this
repo root) since YixinBoard keeps its own `TODO.md`/`docs/` here rather than under a subdirectory.

Do not implement a Backlog item directly in the main session unless the user explicitly says to do
it now — dispatch it instead, so it runs isolated from this session's context (per `CLAUDE.md`
"New requirements/tasks: stack, don't perform directly").

## Reviewing dispatched work

A subagent's own summary describes intent, not necessarily outcome — check the actual diff/build/
test result before relaying "done" to the user, same as any other delegated work in this session.
