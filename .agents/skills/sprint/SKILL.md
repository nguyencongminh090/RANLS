---
name: sprint
description: >-
  Sprint-lifecycle trigger for YixinBoard's tracking files — open a sprint, close
  a sprint (with release cut), or scaffold + file a new task. Doc-only bookkeeping
  that lands straight on main; mirrors CLAUDE.md "Sprint cadence" + the github skill
  so the ceremony actually happens. Never implements a CODE (that is /implement-task).
---

# Sprint (`/sprint open|close|add-task …`)

Runs the coordinated tracking-file edits behind every sprint transition so none get dropped
(Sprint 7 shipped with no milestone or labels — see the `github` skill retro). **Doc / process /
release-metadata only** — `docs/sprint/`, `TODO.md`, `instruction.md`, `docs/todo/`,
`docs/instruction/`, `CHANGELOG.md`, `CMakeLists.txt` `project(VERSION)`, a git tag, GitHub
milestone/labels/board. Never touches `src/`. Implementing a `CODE` is `/implement-task`.

Full step-by-step procedure: `.claude/commands/sprint.md` (same repo). This file is the mirror for
the skill loader — the command file is canonical if they ever drift.

## Inputs

`<mode> …`, mode ∈ `open` | `close` | `add-task`. Project dir is always the repo root.

- `open <N> "<goal>" <CODE...>` — open Sprint `N`; pull the CODEs from `TODO.md` Backlog → Active.
- `close [version]` — archive the current sprint, roll over unfinished items, cut the release,
  reset for the next sprint.
- `add-task <CODE> "<summary>" [--active]` — scaffold `docs/todo/<CODE>-<slug>.md` (+ optional
  `docs/instruction/…`) + the `TODO.md` line. Backlog unless `--active`.

## Shared preflight

1. `git fetch origin && git switch main && git pull --ff-only`; working tree free of `src/` changes.
2. Read `docs/sprint/current.md`, `TODO.md` Backlog/Active, last `docs/sprint/burndown.md` row.
3. `N` = current sprint number.

## `add-task`

1. Validate `CODE` = `<2–5 uppercase>-<number>`, prefix known (or explicitly new + label added),
   not already in `TODO.md` / `docs/todo/`. Derive `<slug>` from the summary.
2. **Design gate:** a `features/<slug>/planning.md` with unresolved open questions → stop
   (`--active` on such a CODE is always refused).
3. Write `docs/todo/<CODE>-<slug>.md` — `Status:` `🔲 OPEN (<Backlog|Active — Sprint N>)`, then
   `Area` / `Priority` / `Source` / `Design` / `Depends on`, then `## Problem`, `## Scope (in
   order)`, `## Acceptance criteria`, `## Scope boundary`. Fill only from the source note / user
   request — if there is not enough for real acceptance criteria, ask, don't guess.
4. Optional `docs/instruction/<CODE>-<slug>.md` (+ `instruction.md` row) when there are non-obvious
   pitfalls / hard boundaries; otherwise note the skip.
5. `TODO.md` line: Backlog (priority order) by default; `--active` → run the `open` step-4 move for
   that single CODE (append under the current sprint's Active heading, bump burndown count, assign
   milestone + `sprint:N` label, note the mid-sprint pull).
6. `node scripts/check-tracking-sync.js --full`; commit `"<CODE>: file task (<where>)"` +
   `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`; push. Report.

## `open`

1. **Preconditions:** previous sprint already closed (`current.md` not still listing open
   unfinished items — else tell the user to `/sprint close` first); every listed `CODE` in
   `TODO.md` Backlog with a `🔲 OPEN` detail file. `N` = previous + 1.
2. Rewrite `docs/sprint/current.md`: `## Sprint <N>`, goal, `Dates: <today> to — (open …)`,
   a **Dependency graph** (one bullet per CODE: layer / files / must-not-touch / needs
   `systematic-debugging`? / "pick X with the user" design calls — from each todo + instruction
   file), the Active table (Status `🔲 Not started`, Points `—`), and the
   **Lesson carried in from Sprint <N-1>** (strongest 1–3 from the prior archive's Lessons).
3. Burndown row: `| <today> | <k> / <k> items | — | Sprint <N> opened (goal "<goal>"): <CODEs>
   pulled from Backlog into Active … |`.
4. `TODO.md`: add `Sprint <N> (opened <today>, goal "<goal>") — pulled from Backlog:` heading in
   **Active** with the moved CODE lines (keep detail/instruction links); delete them from
   **Backlog**; add a provenance line.
5. GitHub (best-effort, non-blocking): `gh label create "sprint:<N>" --force` + each
   `area:<prefix>`; create the `Sprint <N>` milestone if absent
   (`gh api repos/:owner/:repo/milestones -f title="Sprint <N>" -f state=open -f description="<goal>"`);
   board Backlog→Active if `project` scope, else note deferred.
6. `check-tracking-sync.js --full`; commit `"Sprint <N>: open (<CODEs>)"`; push. Report that
   `/implement-task <CODE>` is next.

## `close`

1. **Preconditions per Active `CODE`:** `current.md` status `✅ Done` **and** `TODO.md` line `✅`
   **and** `docs/todo/<CODE>-*.md` `Status:` `✅ DONE|FIXED|CLOSED|VERIFIED` — any disagreement →
   stop, fix the index to the detail file's evidence (`.claude/rules/tracking-files.md`). Each
   shipped `CODE` must have a merged squash PR (`gh pr list --state merged --search "<CODE> in:title"`);
   none → log the hand-land in `docs/audit.md` first (Sprint 7 anti-pattern). Non-✅ items roll
   over (step 3), they don't block the close.
2. Write `docs/sprint/archive/sprint-<N>.md`: Goal, Dates `<start> to <today>`, **Final state**
   table, **What shipped** (2–4 sentences per CODE pulled from `docs/fix-log/*` + todo detail —
   not from memory — with `PR #n squash <sha>`), **Lessons** (carried + new), **Rolled over to
   Backlog**, **Next sprint**.
3. Roll over each non-✅ `CODE`: `TODO.md` line back to Backlog (same CODE), detail `Status:` →
   `🔲 OPEN (Backlog)` + "rolled from Sprint <N>: <what's left>", record in the archive.
4. **Cut the release** — `github` skill "Cutting a release" verbatim: pick `0.N.P` (`version` arg,
   else MINOR for a feature sprint / PATCH for fix-only — ask if ambiguous); `CHANGELOG.md`
   `[Unreleased]` → `[0.N.0] - <today>` (non-empty categories only, user-impact phrasing); fresh
   `[Unreleased]` + `_Nothing yet._`; fix link refs (`compare/v0.N.0...HEAD`, add `[0.N.0]`);
   bump `CMakeLists.txt` `project(VERSION)` (REL-02 single source — `rel02-version-single-source`
   ctest pins it); `git commit -m "Release v0.N.0"`; `git tag v0.N.0 && git push && git push --tags`;
   verify tag local + remote.
5. Reset `docs/sprint/current.md` to `## Sprint <N+1> — not yet opened` + a pointer to
   `/sprint open <N+1> …`.
6. Burndown closing row: `| <today> | 0 / <K> items | — | Sprint <N> closed — … archived …; release
   v0.N.0 cut …; table reset below for Sprint <N+1> |`.
7. GitHub: close the `Sprint <N>` milestone
   (`gh api -X PATCH repos/:owner/:repo/milestones/<num> -f state=closed`); board Active→Done;
   reconcile each shipped CODE (merged PR + complete milestone + Done card) — fix GitHub to the
   archive, never the reverse.
8. `check-tracking-sync.js --full`; push the remaining doc edits
   (`git commit -m "Sprint <N>: close + archive"`); report archive path, version + tag,
   rolled-over CODEs, milestone/board state, next action.

## Notes

- `open` commits scope, `/implement-task <CODE>` builds it, `close` archives + releases — separate.
- All modes land on `main` directly, no branch/PR — the one place `/CLAUDE.md` + the `github` skill
  both allow it.
- A `CODE` gated on `features/<slug>/planning.md` open questions cannot be `add-task --active` or
  in `open` — resolve planning with the user first.
- Never commit through a red `scripts/check-tracking-sync.js`.
