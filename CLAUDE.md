# Project Rules

YixinBoard is a GTK4/gtkmm-4.0 desktop GUI (C++) for Gomoku/Renju engines. It drives an external
engine subprocess (Rapfi, Yixin, or any Gomocup/Yixin-protocol-compatible engine) over stdin/stdout
— see `src/engine/`. Sibling reference projects (context only, not dependencies):
- `/run/media/ngmint/Data/Programming/Programming/C++/Project/Lab/Rapfi_V1/rapfi/Rapfi/` — the usual
  target engine; its `docs/protocol.md` is the canonical protocol reference, and its
  `TODO.md`/`CLAUDE.md`/`docs/todo,instruction,design/` use this repo's tracking convention.
- `/run/media/ngmint/Data/Programming/Programming/HTML/gomoku-vn/` — unrelated web Gomoku app; UI/UX
  precedent only (board rendering, move-tree display).

This file is loaded every session, so it holds only **hard rules and pointers**. Activity-specific
workflows live in `.claude/rules/*.md` (path-scoped, loaded when you touch matching files) and in
skills (loaded on demand) — don't duplicate them here.

## Hard rules

1. **Locate code with CodeGraph first** (`codegraph_explore` / `codegraph explore "<symbols>"`), before
   grep/find — see `~/.claude/CLAUDE.md`.
2. **Any bug, test failure, or unexpected behavior → invoke the `systematic-debugging` skill and follow
   `.claude/rules/bugfix.md` before proposing a fix.** No fix without root cause; one change at a time.
3. **Every bug fix gets a regression test** (or an explicit "no test infrastructure" note) **and a
   fix-log entry** (`docs/fix-log.md` + `docs/fix-log/<date>-<slug>.md`), regardless of size.
4. **Don't implement new work you were not asked to do now.** Record it (note / feature folder / todo
   Backlog) — see "New requirements/tasks".
5. **No code before the design gate:** a non-trivial feature needs `features/<slug>/` with `planning.md`
   open questions resolved, then `docs/todo/` + `docs/instruction/`, *before* coding.
6. **Read the matching `instruction.md` entry before implementing a `TODO.md` item**; note any deviation.
7. **Tracking files are append-only** (fix-log, audit): correct with a new entry, never a rewrite.
8. **Code changes go branch → PR → squash-merge** (`<code>/<slug>`); only doc-only tracking edits may
   land straight on `main`. Details: `github` skill.
9. **Dependency rule:** `ui → command/engine → model`; `model/` never includes gtk or `engine/`
   (see `software-architecture` skill, `docs/knowledge/principle-layering-dependency-rule.md`).

## Knowledge base: use it when reasoning

`docs/knowledge/` holds reference notes (architecture styles/layering, quality trade-offs, storage and
schema evolution, UX heuristics/a11y, SOLID, code smells, design patterns). For architecture, UI/UX,
data/storage or pattern decisions, **read the matching note first** (only that file, not the folder) and
cite it when it drives a conclusion. Notes are generic; this file and the project skills win on
conflict (e.g. prefer dependency injection over the singleton note). Before a design/protocol/build
decision also search past decisions: `node scripts/audit-search.js "<need>"`.

## Process model: Agile Scrum + tracking-file discipline

Work flows through four stages, each with its own artifact — don't skip a stage:

1. **Discuss/brainstorm** — `docs/notes/` (freeform).
2. **Design** — non-trivial work gets `features/<slug>/` first (see below).
3. **Backlog → Sprint** — `TODO.md` (index: **Backlog** / **Active**) + `docs/todo/<CODE>-<slug>.md`
   (detail) + `instruction.md` + `docs/instruction/<CODE>-<slug>.md` (how). Backlog → Active is a
   sprint-planning act, not a coding act.
4. **Fix log** — every bug fix: `docs/fix-log.md` + `docs/fix-log/<date>-<slug>.md`.

`docs/audit.md` records reviews and non-bug decisions (architecture, security, protocol-compat,
build/process); `AGENTS.md` records which agent/model fits which task. Index + detail layout, status
markers, sync rule, templates (`docs/templates/`), audit Status/Supersedes: `.claude/rules/tracking-files.md`.
Read the index first; open only the matched detail file.

### `features/<slug>/`: pre-implementation discussion folders

Fixed structure, don't rename: `user_story.md` (actors, stories, rules, constraints); `diagram/`
(Mermaid inside Markdown); `planning.md` (open questions + sequencing). Cross-link relatively. A
feature folder does not authorize implementation — resolve `planning.md` with the user, then formalize
into todo + instruction. Doc-only: may be written straight on `main`.

## Task size: small tasks get a temporary note, not an audit

Classify each task S/M/L first (table + rules: `.claude/rules/tracking-files.md` → "Task size").
**S** (≤ ~2 files, one layer, no design decision, reversible) → short expiring `docs/notes/` note
(`docs/templates/note.md`). **M** → todo (audit only if a real decision was made). **L** (cross-layer /
protocol / format / toolchain / process / hard to reverse) → feature folder → todo → audit. Bug fixes
always get a fix-log. When unsure, pick the larger size.

## New requirements/tasks: stack, don't perform directly

When the user raises a new requirement mid-conversation (not an explicit "do this now"): record it — a
`docs/notes/` entry if half-formed, `features/<slug>/` if it needs design, or directly a
`docs/todo/` + `TODO.md` Backlog line if well-scoped. Perform it only on "do this now" / "implement
this" / "fix it". This triages *new* work; it doesn't re-litigate tasks assigned this turn.

## Sprint cadence

Moved to `.claude/rules/sprint-cadence.md` (loads on `docs/sprint/**`, `TODO.md`, `CHANGELOG.md`);
commands: `/sprint open|close|add-task`. Closing a sprint also cuts a release (`github` skill
"Cutting a release").

## Bug-fix workflow: diagnosis pipeline, scope discipline, unit tests

Moved to `.claude/rules/bugfix.md` (loads on `src/**`, `tests/**`, fix-log). Hard rules 2–3 above are
the summary: root cause first, fix at the source, regression test, fix-log entry.

## Audit

`docs/audit.md` + `docs/audit/…` — append-only reviews/decisions (correct with `**Supersedes:**`).
Search: `node scripts/audit-search.js "<need>"`. Short entries go in `docs/audit/<YYYY-MM>-log.md`.

## GitHub project management

Personal Scrumban; **local tracking files are the single source of truth**, GitHub only adds review,
CI history and a board. One branch + PR per `CODE` (`<code>/<slug>`, title `<CODE>: <summary>`,
squash-merge); single trunk; thin Issues; labels `area:<prefix>` + `sprint:<N>`; milestone = sprint.
Full model and commands: `github` skill ("Development model and GitHub's role").

## Agent management

`AGENTS.md`: which subagent type / model tier per task shape, and how `/implement-task` dispatches
bounded `TODO.md` items to an isolated subagent.

## Notes

`docs/notes/` is the loosest tier — brainstorming, half-formed ideas, discussion transcripts; one
dated file per topic (`YYYY-MM-DD-slug.md`). Promote to `features/<slug>/` once it needs a user story.
