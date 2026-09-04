---
description: Sprint-lifecycle trigger for YixinBoard's tracking files — open a sprint, close a sprint (with release cut), or scaffold+file a new task. Doc-only bookkeeping that lands straight on main; mirrors the tracking convention in CLAUDE.md + the github skill so the ceremony actually happens.
argument-hint: open <N> "<goal>" <CODE...> | close [version] | add-task <CODE> "<summary>" [--active]
allowed-tools: Read, Grep, Glob, Bash, Edit, Write
---

## Inputs

Parse `$ARGUMENTS` as `<mode> …`. `mode` is one of `open`, `close`, `add-task`.
Project dir is always `.` (YixinBoard keeps `TODO.md` / `instruction.md` / `docs/` at the repo root
— see `/CLAUDE.md`).

- `open <N> "<goal>" <CODE...>` — open Sprint `N` with `<goal>` and pull the listed `CODE`s from
  `TODO.md` **Backlog** into **Active**.
- `close [version]` — close the current sprint: archive it, roll over unfinished items, cut the
  release (`version` = explicit `0.N.P`; omitted → derive per rule below), reset for the next sprint.
- `add-task <CODE> "<summary>" [--active]` — scaffold `docs/todo/` + (optional) `docs/instruction/`
  + the `TODO.md` line for a new `CODE`. Lands in **Backlog** unless `--active` (which also performs
  the Backlog→Active sprint-planning move into the current sprint).

## Why this command exists

Every sprint transition in this repo is several coordinated edits across `docs/sprint/current.md`,
`docs/sprint/archive/`, `docs/sprint/burndown.md`, `TODO.md`, `CHANGELOG.md`, `CMakeLists.txt`, a
git tag, and GitHub (milestone / labels / board). `/CLAUDE.md` "Sprint cadence" and the `github`
skill describe all of it, but done by hand a step gets dropped (Sprint 7 shipped with no milestone
or labels at all — see the `github` skill retro). This command runs the same steps every time.

**It is doc/process only** — every edit here is a tracking-file or release-metadata change, which
`/CLAUDE.md` and the `github` skill both permit straight on `main` with no branch or PR. It never
touches `src/`. Implementing a `CODE` is `/implement-task`, not this.

## Shared preflight (all modes)

1. `git -C . fetch origin && git -C . switch main && git -C . pull --ff-only`.
2. `git -C . status --porcelain` — only doc-only tracking edits may be pending; never inherit a
   dirty `src/` tree. Stop and report if it is dirty with code.
3. Read `docs/sprint/current.md` (the active sprint + its Active table), the `TODO.md`
   **Backlog** / **Active** sections, and `docs/sprint/burndown.md` (last row).
4. Note the current sprint number `N` from `docs/sprint/current.md`.

---

## Mode: `add-task`

### 1. Validate

- `CODE` = `<2–5 uppercase letters>-<number>` (e.g. `ANLZ-04`, `UI-14`). Reject anything else.
- The prefix must already appear in `TODO.md` (existing area) **or** you explicitly tell the user
  you are introducing a new area prefix — if new, also add the `area:<prefix>` label line to the
  `github` skill's label list and create the label (`gh label create "area:<prefix>" --force`).
- `CODE` must not already exist in `TODO.md` or as a `docs/todo/<CODE>-*.md` file. If it does,
  stop — this is a re-file; the user wants a new number or an edit to the existing detail file.
- Derive `<slug>` from `"<summary>"`: lowercase, kebab-case, ≤ 6 words.
- **Design gate.** If a `features/<slug-ish>/` folder exists for this work and its `planning.md`
  still has unresolved open questions, **stop and report** — the task is not ready to file
  (`/CLAUDE.md`: resolve `planning.md` first). `add-task --active` on such a `CODE` is always
  refused.

### 2. Scaffold the detail file

Write `docs/todo/<CODE>-<slug>.md`:

```markdown
# <CODE> — <summary>

**Status:** 🔲 OPEN (<Backlog | Active — Sprint N>)
**Area:** <files/dirs this will touch — best guess, one line>
**Priority:** <P1 | P2 | P3>
**Source:** <who asked / which note or feature folder — today's date 2026-…>
**Design:** <features/<slug>/ if one exists, else "none — scoped directly">
**Depends on / relates to:** <other CODEs, or "—">

## Problem

<1–3 short paragraphs: what is wrong / missing and why it matters. From the user's words + the
source note — do not invent scope.>

## Scope (in order)

1. …

## Acceptance criteria

- …

## Scope boundary

- Do not …
```

Fill `Problem` / `Scope` / `Acceptance` from the source note or the user's request only — if there
is not enough to write real acceptance criteria, stop and ask rather than guessing.

### 3. Optional instruction file

If the task has non-obvious pitfalls or hard "do not touch" boundaries, also write
`docs/instruction/<CODE>-<slug>.md` (Approach / Pitfalls / Verification before done / Boundaries)
and add its one-line row to `instruction.md` under a matching `## <CODE> — <slug>` heading. Skip
both if the detail file's "Scope boundary" is sufficient (note that choice in your report).

### 4. Index line

Add to `TODO.md`:
- Default → the **Backlog** section, in priority order:
  `- 🔲 **<CODE>.** <summary>. <one clause of context / gate>. [Model: <tier>] — [detail](docs/todo/<CODE>-<slug>.md)[ · [instruction](docs/instruction/<CODE>-<slug>.md)]`
- `--active` → skip Backlog; go straight to step 4 of **Mode: `open`** for this single `CODE`
  (append it under the current sprint's Active heading, bump the burndown item count, assign the
  milestone + `sprint:N` label, note it was pulled mid-sprint).

Add a "Filed <date> from <source>" provenance line to the bottom of `TODO.md`'s Backlog notes if a
new source is being cited.

### 5. Land it

- `node scripts/check-tracking-sync.js --full` — must pass.
- `git add -A && git commit -m "<CODE>: file task (<Backlog|Sprint N Active>)"` with the
  `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` trailer. `git push`.
- Report: files created, where the line landed, and whether an instruction file was written.

---

## Mode: `open`

### 1. Preconditions

- The **previous** sprint must already be closed — `docs/sprint/current.md` must not still list an
  open sprint with unfinished Active items. If it does, tell the user to run `/sprint close` first
  and stop.
- Every `CODE` in the argument list must be present in `TODO.md` **Backlog** with a
  `docs/todo/<CODE>-*.md` detail file whose `Status:` is `🔲 OPEN`. Any miss → stop and report
  (offer `add-task` for a genuinely new one).
- `N` = the new sprint number (previous + 1). Confirm it matches what the user passed.

### 2. Reset `docs/sprint/current.md`

Rewrite it to:

```markdown
# Current sprint

## Sprint <N>

**Goal:** <goal>

**Dates:** <today ISO> to — (open — no fixed end date set yet)

**Dependency graph:** <one bullet per CODE: layer, files, what it must not touch, whether it needs
`systematic-debugging` first, and any "pick X with the user" design call. Pull this from each
`docs/todo/<CODE>-*.md` + `docs/instruction/<CODE>-*.md`.>

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| <CODE> | <summary> | <dep or —> | — | 🔲 Not started |

Points not yet estimated (consistent with Sprints 3–…).

**Lesson carried in from Sprint <N-1>:** <the "Lessons" bullet(s) from
`docs/sprint/archive/sprint-<N-1>.md` most relevant to these CODEs — keep the strongest 1–3.>

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
```

### 3. Burndown row

Append to `docs/sprint/burndown.md`:
`| <today ISO> | <k> / <k> items | — | Sprint <N> opened (goal "<goal>"): <CODE, CODE…> pulled from Backlog into Active — see docs/sprint/current.md. Backlog now <empty | has M items> |`

### 4. Move the index lines in `TODO.md`

- Add a new heading in the **Active** section (matching the existing style):
  `Sprint <N> (opened <today ISO>, goal "<goal>") — pulled from Backlog:` followed by the moved
  `CODE` lines, each turned from `🔲` to `🔲` (unchanged marker until work starts — status lives in
  `current.md` while active) and kept linking their detail (+ instruction) files.
- Remove those same lines from **Backlog**.
- Add a provenance line: `Filed <date>… — **pulled into Sprint <N> Active <today ISO>** (see docs/sprint/current.md).`

### 5. GitHub (best-effort, never blocks the doc edit)

- `gh label create "sprint:<N>" --force`; `gh label create "area:<prefix>" --force` for each
  distinct prefix among the CODEs.
- Create the milestone if absent:
  `gh api repos/:owner/:repo/milestones -f title="Sprint <N>" -f state=open -f description="<goal>"`.
- Board: move each `CODE` card Backlog→Active if the `gh` token has `project` scope; otherwise note
  "board sync deferred to sprint close" in the report.

### 6. Land it

- `node scripts/check-tracking-sync.js --full` — must pass.
- `git add -A && git commit -m "Sprint <N>: open (<CODE, CODE…>)"` + `Co-Authored-By` trailer.
  `git push`.
- Report: sprint number, goal, committed CODEs, milestone/label state, and that implementation is
  next via `/implement-task <CODE>` (this command does not implement).

---

## Mode: `close`

### 1. Preconditions — every Active item is done

For the current sprint `N`, for each `CODE` in `docs/sprint/current.md`'s Active table:
- Its `current.md` status is `✅ Done` **and** the `TODO.md` Active line is `✅` **and**
  `docs/todo/<CODE>-*.md` `Status:` is `✅ DONE|FIXED|CLOSED|VERIFIED`. If any of the three
  disagree → stop and report the drift (`.claude/rules/tracking-files.md`: bring the index in line
  with the detail file's evidence — do not "close" over drift).
- `gh pr list --state merged --search "<CODE> in:title"` returns the squash-merge PR. Note its
  number + SHA. A `CODE` with no merged PR → stop (it was hand-landed — that is the Sprint 7
  anti-pattern; log it in `docs/audit.md` before proceeding, per the `github` skill retro).

Any Active item **not** ✅ → it rolls over (step 3), it does not block the close, but call it out.

### 2. Snapshot to the archive

Write `docs/sprint/archive/sprint-<N>.md`:

```markdown
# Sprint <N> (closed <today ISO>)

**Goal:** <goal from current.md>
**Dates:** <start> to <today ISO>.

## Final state — <all items shipped | M of K items shipped>

| CODE | Summary | Status |
|---|---|---|
| <CODE> | <summary> | ✅ <DONE|FIXED|…> |

<one paragraph: any mid-sprint pulls, points-not-estimated note.>

## What shipped

- **<CODE>** (PR #<n> squash `<sha>`): <2–4 sentences from the fix-log / todo detail — mechanism,
  regression test, anything deferred.>

## Lessons

- <carried-forward lessons still true + any new one this sprint. These feed the next
  `current.md`'s "Lesson carried in from Sprint <N>".>

## Rolled over to Backlog

<"Nothing rolled over — all committed items finished." | one bullet per unfinished CODE, keeping
its original CODE, with what's left.>

## Next sprint

<"Sprint <N+1> — run `/sprint open <N+1> …` to commit its Backlog items." | the goal if the user
has already named it. Note the release cut below.>
```

Pull "What shipped" text from `docs/fix-log/*.md` + `docs/todo/<CODE>-*.md` for each CODE — do not
re-summarize from memory.

### 3. Roll over unfinished items

For each non-✅ Active `CODE`: move its `TODO.md` line back to **Backlog** (keep the same `CODE`),
set its `docs/todo/<CODE>-*.md` `Status:` back to `🔲 OPEN (Backlog)` with a one-line "rolled from
Sprint <N>: <what's left>" note, and record it in the archive's "Rolled over" section.

### 4. Cut the release (`github` skill "Cutting a release")

Run those steps verbatim:
1. **Version.** `version` arg if given. Else `0.N.P`: bump **MINOR** if the sprint shipped a new
   user-visible feature, **PATCH** if it was fix/polish only. If it is genuinely ambiguous, ask the
   user which — one question, then proceed.
2. `CHANGELOG.md`: `## [Unreleased]` → `## [0.N.0] - <today ISO>`, keep only non-empty categories,
   user-impact phrasing (trailing `(CODE)` ok). Cross-check against the archive's "What shipped" +
   new `docs/fix-log.md` rows since the last tag.
3. Add a fresh `## [Unreleased]` + `_Nothing yet._` at the top.
4. Fix the link-reference definitions: `[Unreleased]` → `compare/v0.N.0...HEAD`, add `[0.N.0]` →
   `releases/tag/v0.N.0`.
5. `CMakeLists.txt`: bump `project(... VERSION 0.N.P ...)` to the new version (REL-02 single source
   — `rel02-version-single-source` ctest pins CLI == CMake, so this must move with the tag).
6. `git add -A && git commit -m "Release v0.N.0"` + `Co-Authored-By` trailer.
7. `git tag v0.N.0 && git push && git push --tags`.
8. Verify: `git tag -l | grep v0.N.0`; `git ls-remote --tags origin | grep v0.N.0`.

### 5. Reset `docs/sprint/current.md`

To a holding state until the next `open`:

```markdown
# Current sprint

## Sprint <N+1> — not yet opened

Sprint <N> closed <today ISO> (archived: `docs/sprint/archive/sprint-<N>.md`, release `v0.N.0`).

Run `/sprint open <N+1> "<goal>" <CODE...>` to commit Backlog items and start it.
```

### 6. Burndown closing rows

Append to `docs/sprint/burndown.md`:
```
| <today ISO> | 0 / <K> items | — | Sprint <N> closed — <CODE list> landed on `main`; archived to `docs/sprint/archive/sprint-<N>.md`, <rollover note>. Release `v0.N.0` cut (<one clause>). Table reset below for Sprint <N+1> |
```

### 7. GitHub close-out

- `gh api repos/:owner/:repo/milestones --jq '.[] | select(.title=="Sprint <N>") | .number'` → close
  it: `gh api -X PATCH repos/:owner/:repo/milestones/<num> -f state=closed`.
- Board: move each shipped `CODE` card Active→Done (if `project` scope); else note it.
- `github` skill "Reconciling": confirm each shipped `CODE` has merged PR + complete milestone +
  Done card. Fix GitHub to match the archive, never the reverse.

### 8. Land + report

- `node scripts/check-tracking-sync.js --full` — must pass.
- The tracking edits + the `Release v0.N.0` commit are already pushed (step 4.7 pushed the tag
  commit; push any remaining doc edits: `git add -A && git commit -m "Sprint <N>: close + archive" ;
  git push`).
- Report: archive file path, release version + tag (pushed y/n), rolled-over CODEs, milestone
  closed y/n, board state, and the next action (`/sprint open <N+1> …`).

---

## Notes

- **Never implements a `CODE`.** `open` commits scope; `/implement-task <CODE>` builds it;
  `close` archives and releases. Keep them separate.
- **All three modes are doc/release-metadata only** and land straight on `main` — no feature branch,
  no PR. This is the one place `/CLAUDE.md` and the `github` skill both allow direct `main` commits.
- A `CODE` still gated on `features/<slug>/planning.md` open questions cannot be `add-task --active`
  or listed in `open` — resolve planning with the user first (`/CLAUDE.md` "Process model").
- If `scripts/check-tracking-sync.js` fails at any step, fix the drift before committing — do not
  commit through a red check.
- Big multi-`CODE` feature: `open` still just commits the sub-task `CODE`s; the
  `feat/<feature-slug>` integration branch is set up separately per the `github` skill "Branch
  model".
