# Project Rules

YixinBoard is a GTK3/gtkmm desktop GUI (C++) for Gomoku/Renju engines. It drives an external
engine subprocess (Rapfi, Yixin, or any Gomocup/Yixin-protocol-compatible engine) over stdin/stdout
— see `src/engine/`. Sibling reference projects on this machine (not code dependencies, context only):
- `/run/media/ngmint/Data/Programming/Programming/C++/Project/Lab/Rapfi_V1/rapfi/Rapfi/` — the
  engine this GUI most commonly targets; its `docs/protocol.md` is the canonical protocol reference
  and its `TODO.md`/`CLAUDE.md`/`docs/todo,instruction,design/` use the same tracking convention as
  this file describes below.
- `/run/media/ngmint/Data/Programming/Programming/HTML/gomoku-vn/` — an unrelated web Gomoku app,
  useful for UI/UX precedent (board rendering, move-tree display) but shares no engine/protocol code.

Rules below apply to every session. Activity-specific workflows live in skills, loaded on demand —
don't duplicate them here.

## CodeGraph

This repo has a `.codegraph/` index (see `/home/ngmint/.claude/CLAUDE.md` for usage) — prefer
`codegraph_explore` / `codegraph explore` over grep/find for locating or understanding code.

## Process model: Agile Scrum + tracking-file discipline

Work flows through four stages, each with its own artifact. Don't skip a stage or perform
implementation before the earlier stages are formalized:

1. **Discuss/brainstorm** — `docs/notes/` (freeform, see below).
2. **Design** — for anything non-trivial, `features/<slug>/` (see below) before it becomes tracked work.
3. **Backlog → Sprint** — formalized items live in `TODO.md` (index) + `docs/todo/<CODE>-<slug>.md`
   (detail), with execution guidance in `instruction.md` + `docs/instruction/<CODE>-<slug>.md`.
   `TODO.md` has two sections: **Backlog** (prioritized, not yet committed to a sprint) and
   **Active** (committed to the current sprint — see `docs/sprint/current.md`). Moving an item from
   Backlog to Active is a sprint-planning act, not a coding act.
4. **Fix log** — every bug fix gets `docs/fix-log.md` (index) + `docs/fix-log/<date>-<slug>.md`
   (detail), regardless of whether it started life as a `TODO.md` item.

`docs/audit.md` records reviews and non-bug decisions (architecture, security, process) that aren't
a "fix" and aren't a feature — see below. `AGENTS.md` records which agent/model to reach for on
which kind of task.

### Tracking-file layout: index + detail files

`TODO.md`, `instruction.md`, `docs/fix-log.md`, and `docs/audit.md` are lightweight **indexes** —
one line per item, linking to a detail file one level down:
- `docs/todo/<CODE>-<slug>.md`, `docs/instruction/<CODE>-<slug>.md` — `CODE` is a short task code,
  e.g. `WALL-01`, `UI-07`. Pick a 2-5 letter prefix for the feature area + a running number per prefix.
- `docs/fix-log/<YYYY-MM-DD>-<slug>.md`, `docs/audit/<YYYY-MM-DD>-<slug>.md` — one file per row,
  named by date + opening-words slug.

Full sync rule, canonical status-marker format, and the automated enforcement hook are in
`.claude/rules/tracking-files.md` (path-scoped — loads automatically when you touch these files).
Read the index first; only `Read` the matched detail file, not the whole tree.

## `features/<slug>/`: pre-implementation feature discussion folders

Before a new feature idea becomes tracked work, work it through `features/<slug>/`:
- Fixed structure, don't omit/rename: `user_story.md` (actors, user stories, rules, hard
  constraints); `diagram/` (Mermaid-fenced sequence/state/class diagrams inside Markdown, not
  separate image files); `planning.md` (open questions + resolution/implementation sequencing).
- Cross-link liberally between the files (relative Markdown links).
- A `features/<slug>/` folder does not by itself authorize implementation. Once `planning.md`'s
  open questions are resolved with the user, formalize into `docs/todo/<CODE>-<slug>.md` + `TODO.md`
  (Backlog section) and `docs/instruction/<CODE>-<slug>.md` + `instruction.md` *before* writing code.
- Doc-only — can be written/updated straight on `main`, no branch needed.

## New requirements/tasks: stack, don't perform directly

When the user raises a new requirement/feature/task mid-conversation (not an explicit "do this now"):
- **Default to recording it, not implementing it**: a `docs/notes/` entry if still half-formed, a
  `features/<slug>/` folder if it needs design, or directly a `docs/todo/<CODE>-<slug>.md` + `TODO.md`
  Backlog line if it's already well-scoped.
- **Only perform directly if explicitly required now** ("do this now", "implement this", "fix it").
- This is triage of *new* work, not re-litigating tasks already assigned this turn.

## Sprint cadence

- `docs/sprint/current.md` — the active sprint: goal, start/end date, committed `CODE`s pulled from
  `TODO.md`'s Backlog into its Active section, story points per item.
- `docs/sprint/burndown.md` — one row per day: date, points remaining, ideal-line reference. Update
  it when an Active item's status changes, not just at sprint end. Ask the user before rendering it
  as a chart artifact (cheap to do on request — don't do it unprompted every update).
- `docs/sprint/archive/<sprint-N>.md` — closed sprints: final burndown, what shipped, what rolled
  over to the next sprint's Backlog (rolled-over items keep their original `CODE`).
- Starting a new sprint = one edit: snapshot `current.md` into `archive/sprint-N.md`, reset
  `current.md` for the next sprint, pull newly-committed items from `TODO.md` Backlog into Active.

## Bug-fix workflow: scope discipline and unit tests

- **Base the fix strictly on what was provided.** Don't silently extend a fix to cover speculative
  scenarios beyond the reported bug — call those out separately (`TODO.md` Backlog) instead.
- **Write a regression test for the fix whenever the affected code has, or can reasonably get, real
  coverage.** If the area has no test infrastructure, say so explicitly rather than skipping silently.
- **Never discard a test case after writing it.** It's the permanent regression guard, not a
  one-time proof.
- **Before implementing any `TODO.md` task, read the matching `instruction.md` entry.** Missing entry
  is fine — not every task has one. If a fix deviates from `instruction.md`, note why in the summary.
- Every fix gets a `docs/fix-log.md` row + `docs/fix-log/<date>-<slug>.md` detail file, whether or
  not it started as a tracked `TODO.md` item.

## Audit

`docs/audit.md` + `docs/audit/<date>-<slug>.md` record reviews and non-bug decisions: architecture
choices, security reviews, protocol-compatibility checks against Rapfi/Yixin-protocol changes,
build/toolchain decisions. Same index+detail, append-only shape as the fix log — a wrong past entry
gets a new correcting entry, not a rewrite.

## GitHub project management

Development model: **personal Scrumban** — Scrum vocabulary (numbered sprints, Backlog/Active split,
story points, burndown, retro "lessons") over a continuous single-piece flow (WIP ≈ 1: one dispatched
task at a time, items pulled into a sprint mid-flight, sprints often open-ended), with V-Model-grade
traceability: every work item runs `docs/notes/` → `features/<slug>/` → `docs/todo/` + `docs/instruction/`
→ code → `docs/fix-log.md`/`docs/audit.md`, and every fix carries a regression test. See
`docs/audit/2026-08-30-github-project-management.md` for the reasoning.

GitHub's role is deliberately narrow — **the local tracking files stay the single source of truth**;
GitHub adds review, CI history, and a visual board, never a second backlog:
- **Branch + PR per `CODE`**: branch `<code>/<slug>`, PR title `<CODE>: <summary>`, PR body links
  `docs/todo/<CODE>-*.md`, squash-merge to keep `main` one-commit-per-task and linear. `main` requires
  a PR but not a reviewer (solo). Doc-only tracking edits still go straight to `main` (see above).
- **Single trunk** — no permanent `dev` branch. A big/risky feature uses a longer-lived
  `feat/<slug>` integration branch (sub-task PRs merge into it, it rebases on `main`, one PR back to
  `main` at the end); demo/stable builds are tagged off `main`. See the `github` skill "Branch model".
- **Issues are thin**: the `docs/todo/` tree is the backlog. Open an Issue only for an externally
  reported bug (intake → triage into a `CODE`) or an item you want on the board; title `<CODE>: …`.
- **Labels** mirror the `TODO.md` prefixes (`area:UI`, `area:STATE`, …) + `sprint:<N>`.
- **Milestone = sprint** (`Sprint <N>`), closed when the sprint is archived.
- **Projects board** (Backlog / Active / Done) mirrors `TODO.md` + `docs/sprint/current.md`,
  hand-synced only at sprint planning and sprint close — not per commit.

Step-by-step commands (PR lifecycle, label/milestone setup, board sync, reconciliation) live in the
`github` skill — load it when doing any of the above.

## Agent management

See `AGENTS.md` for which subagent type and model tier to reach for per task shape, and how
`/implement-task` dispatches bounded `TODO.md` items to an isolated subagent.

## Notes

`docs/notes/` is the loosest tier — brainstorming, half-formed ideas, discussion transcripts worth
keeping. No fixed structure; one dated file per topic (`YYYY-MM-DD-slug.md`). Promote a note to
`features/<slug>/` once it has a concrete enough shape to need a user story.
