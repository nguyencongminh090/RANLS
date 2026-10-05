# Project Rules

YixinBoard is a GTK4/gtkmm-4.0 desktop GUI (C++) for Gomoku/Renju engines. It drives an external
engine subprocess (Rapfi, Yixin, or any Gomocup/Yixin-protocol-compatible engine) over stdin/stdout
— see `src/engine/`. Naming: the product/docs say **YixinBoard**; the GitHub repo is `RANLS` and the
binary/targets are `ranls-gui`, `ranls-gui-tests`, `ranls-gui-ui-tests` (older docs may say `rapfi-gui*`).
Sibling reference projects (protocol docs, UI precedent): `docs/reference-projects.md`.

This file is loaded every session, so it holds only **hard rules and pointers**. Activity-specific
workflows live in `.claude/rules/*.md` (path-scoped, loaded when you touch matching files) and in
skills (loaded on demand) — don't duplicate them here.

## Build and test

- `./build.sh` — configure + build (`build_msys2.sh` on Windows/MSYS2). `RUN_TESTS=1 ./build.sh`
  also runs `ctest` (`ranls-gui-tests` unit tests; `ranls-gui-ui-tests` real-widget tests, auto-skipped without a display).
- Layout: `src/{model,engine,command,ui}`, `tests/` (doctest, vendored), `tests/mock_engine`.

## Hard rules

1. **Any bug, test failure, or unexpected behavior → invoke the `systematic-debugging` skill and follow
   `.claude/rules/bugfix.md` before proposing a fix.** No fix without root cause; one change at a time.
2. **Every bug fix gets a regression test** (or an explicit "no test infrastructure" note) **and a
   fix-log entry** (`docs/fix-log.md` + `docs/fix-log/<date>-<slug>.md`), regardless of size.
3. **Don't implement new work you were not asked to do now.** Record it (note / feature folder / todo
   Backlog) — see "New requirements/tasks".
4. **Size-L work needs the design gate before code:** `features/<slug>/` with `planning.md` open
   questions resolved, then `docs/todo/` + `docs/instruction/`. (Sizes: "Task size" below.)
5. **Read the matching `instruction.md` entry before implementing a `TODO.md` item**; note any deviation.
6. **Tracking files are append-only** (fix-log, audit): correct with a new entry, never a rewrite.
7. **Branch → PR → squash-merge** (`<code>/<slug>`) for anything outside `docs/`/tracking Markdown —
   i.e. `src/`, `tests/`, `scripts/`, `.claude/`, build files. Only doc-only edits (under `docs/`,
   `features/`, root tracking `.md`) may land straight on `main`. Details: `github` skill.
8. **Dependency rule:** `ui → command/engine → model`; `model/` must not include gtk or `engine/`
   (`software-architecture` skill; `docs/knowledge/principle-layering-dependency-rule.md`).
   **Currently violated** by `model/game_state.h` and `model/board_view_model.h` including
   `engine/engine_types.h` — tracked as ARCH-01; don't add new violations.

## Knowledge base: use it when reasoning

`docs/knowledge/` holds reference notes (architecture styles/layering, quality trade-offs, storage and
schema evolution, UX heuristics/a11y, SOLID, code smells, design patterns). For architecture, UI/UX,
data/storage or pattern decisions, **read the matching note first** (only that file, not the folder) and
cite it when it drives a conclusion. Notes are generic; this file and the project skills win on
conflict (e.g. prefer dependency injection over the singleton note). Before a design/protocol/build
decision also search past decisions: `node scripts/audit-search.js "<need>"`.

## Process model: Agile Scrum + tracking-file discipline

Work flows through four stages, each with its own artifact. **How much of the chain a task needs
depends on its size** (next section) — an S task is just a note; an L task runs the whole chain:

1. **Discuss/brainstorm** — `docs/notes/` (freeform).
2. **Design** — `features/<slug>/`: `user_story.md`, `diagram/` (Mermaid in Markdown), `planning.md`
   (open questions + sequencing). Not authorization to implement — resolve `planning.md` with the user,
   then formalize into todo + instruction. Doc-only.
3. **Backlog → Sprint** — `TODO.md` (**Backlog** / **Active**) + `docs/todo/<CODE>-<slug>.md` (what)
   + `instruction.md` + `docs/instruction/<CODE>-<slug>.md` (how). Backlog → Active is a
   sprint-planning act, not a coding act.
4. **Fix log** — every bug fix: `docs/fix-log.md` + `docs/fix-log/<date>-<slug>.md`.

`docs/audit.md` records reviews and non-bug decisions; `AGENTS.md` records which agent/model fits which
task. Read an index first; open only the matched detail file.

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

## Where the detail lives

| Topic | Location |
|---|---|
| Bug-fix pipeline, scope discipline, regression tests | `.claude/rules/bugfix.md` |
| Sprint cadence, burndown, archive, release cut | `.claude/rules/sprint-cadence.md`; `/sprint open\|close\|add-task` |
| Index+detail layout, status markers, templates, task size, audit Status/Supersedes/search | `.claude/rules/tracking-files.md`; `docs/templates/` |
| GitHub model (Scrumban; local files are the source of truth), PR lifecycle, labels, milestones | `github` skill |
| Agent/model tier per task, `/implement-task` dispatch | `AGENTS.md` |
| Past decisions | `docs/audit.md`, `node scripts/audit-search.js "<need>"` |
