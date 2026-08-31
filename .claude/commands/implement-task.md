---
description: Dispatch a subagent to implement one tracked TODO.md CODE end-to-end on its own branch, then drive it through the full github-skill PR lifecycle (branch → PR → squash-merge → tracking + board sync). Never lands work as a manual commit on main.
argument-hint: <CODE> [project-dir]
allowed-tools: Read, Grep, Glob, Bash, Agent
---

## Inputs

Parse `$ARGUMENTS` as `<CODE> [project-dir]`:
- `CODE` — the task code, e.g. `WALL-01`. Required.
- `project-dir` — repo-relative directory holding `TODO.md`/`instruction.md`/`docs/`. Default:
  `.` (this repo's root — YixinBoard keeps its tracking files there, see `/CLAUDE.md`) if omitted.

## Why this command exists

Every tracked `CODE` must reach `main` as **one squash-merged PR of its own** — branch
`<code>/<slug>`, PR title `<CODE>: <summary>`, GitHub label + milestone + board card synced. This
command automates that lifecycle so it actually happens.

**Anti-pattern this command prevents (Sprint 7):** ENG-02, UI-08, UI-09, and REL-02 were
implemented by hand in the main session and pushed as a single commit (`314d434`) straight onto
`main` — no per-`CODE` branch, no PR, no labels/milestone/board. Branch protection didn't stop it
(it doesn't enforce on the repo owner), which is exactly why the discipline has to live here. Do
not do this. If a `CODE` is scoped enough to implement, it goes through this command (or, done by
hand, through the identical `github` skill "PR lifecycle for one CODE" — same steps, nothing
skipped).

## What to do

### 1. Locate the task

In `<project-dir>/TODO.md`, find the line referencing `CODE`. Read
`<project-dir>/docs/todo/<CODE>-*.md` and `<project-dir>/docs/instruction/<CODE>-*.md`. If either
is missing, or the code isn't in `TODO.md`'s **Active** section, **stop and report** — do not guess
which task the user means or invent scope that isn't written down. Also read any linked design
doc(s) (`docs/design/*.md`) so the dispatch prompt can cite concrete `file:line` anchors.

Re-running on a `CODE` already ✅ in `TODO.md` → ask the user for confirmation first (likely a
regression or a follow-up deserving its own new `CODE`).

### 2. Preflight git + GitHub

Run and confirm before dispatching:
- `git -C <project-dir> status --porcelain` is empty (or the only changes are unrelated doc-only
  tracking edits — never let the agent inherit a dirty tree).
- `git -C <project-dir> fetch origin && git -C <project-dir> switch main && git -C <project-dir> pull --ff-only`.
- The `area:<prefix>` and `sprint:<N>` labels for this `CODE` exist (`gh label list`). Create any
  missing one: `gh label create "area:<prefix>" --force` / `gh label create "sprint:<N>" --force`.
- The `Sprint <N>` milestone exists (`gh api repos/:owner/:repo/milestones --jq '.[].title'`).
  Create it if not: `gh api repos/:owner/:repo/milestones -f title="Sprint <N>" -f state=open
  -f description="<sprint goal from docs/sprint/current.md>"`.

Derive `<slug>` from the `docs/todo/<CODE>-<slug>.md` filename. Branch name is `<code>/<slug>`
lowercased (e.g. `ENG-02` → `eng-02/engine-play-interrupted-reverts-to-manual`).

### 3. Dispatch one Agent

`subagent_type: general-purpose` unless the todo file names a more specific one; model per the
`[Model: <tier>]` tag or `AGENTS.md` default (Sonnet 5). Use `isolation: "worktree"` so it works on
an isolated branch. The self-contained prompt must include:

- Absolute paths of the todo/instruction/design files to read first, and an instruction to treat
  the instruction file's **"Boundaries — do not touch"** and **"Pitfalls"** sections as hard
  constraints, not suggestions.
- The concrete scope steps from the todo file, in order.
- The exact **"Verification before marking this task done"** criteria from the instruction file —
  the agent runs/confirms *all* of them itself (build, tests, self-play/regression as specified)
  before reporting success. Passing unit tests alone is not enough if the instruction file lists
  more tiers — say so explicitly.
- **Git discipline for the agent:**
  - Do all work on branch `<code>/<slug>` (create it off `main` if the worktree didn't).
  - Commit in logical chunks; every message `<CODE>: <imperative summary>`, keeping the
    `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` trailer. Intermediate messages are
    unconstrained but must still start `<CODE>:`.
  - Write the regression test (`CLAUDE.md` bug-fix rule) — never discard it.
  - Update the tracking files **in the same branch**, per `.claude/rules/tracking-files.md`: flip
    `docs/todo/<CODE>-*.md` `**Status:**` to `✅ <verb>` with a summary + verification notes, flip
    the `TODO.md` Active line to `✅` (leave it in Active — the orchestrator moves it after merge),
    add the `docs/fix-log/<date>-<slug>.md` detail file + one `docs/fix-log.md` row.
  - **Do NOT push, open a PR, merge, or touch `main`.** Stop after the last commit on the branch.
  - A partially-done task: leave `Status:` `Active` with a note on what's left, still commit what
    exists on the branch, and say so in the report.
- Report back: files touched, what verification was actually run and its result, branch name +
  final commit SHA, anything deliberately left out of scope per the boundaries.

Do not implement it yourself in this session. Launch the agent (background by default) and wait.

### 4. Verify the agent's work yourself

When the result lands, do not relay "done" on the agent's word. On its branch:
`git -C <worktree> log --oneline main..HEAD`, re-run the instruction file's verification tiers
(build + both ctest suites at minimum), read the diff. If verification fails or scope is
incomplete → report that to the user and stop; do not open a PR.

### 5. PR lifecycle (only if verification passed)

Follow the `github` skill "PR lifecycle for one CODE" steps 3–7 verbatim:
- `git push -u origin <code>/<slug>`.
- `gh pr create --title "<CODE>: <summary>" --milestone "Sprint <N>" --label "area:<prefix>,sprint:<N>"`
  with a body that **links `docs/todo/<CODE>-<slug>.md`** (not "Closes #" unless a real Issue
  exists), states what changed, and gives the test evidence (e.g. `132/132 + 6 UI tests pass`).
  Add `Refs #<n>` if a GitHub Issue tracks this `CODE`.
- Self-review the diff in the PR. (No CI yet — see the skill's "Future".)
- `gh pr merge --squash --delete-branch`. One linear commit per `CODE` on `main`.

### 6. Post-merge sync (same turn)

- `git -C <project-dir> switch main && git -C <project-dir> pull --ff-only`.
- Move the `TODO.md` line out of **Active** (it's already ✅); update `docs/sprint/current.md`
  (status → Done) and add a `docs/sprint/burndown.md` row for the day. These are doc-only and land
  straight on `main`.
- Move the board card to **Done** (`gh project item-edit …` if the token has `project` scope;
  otherwise note it for the sprint-close batch).
- If this was the sprint's last Active item → tell the user it's time for the `github` skill
  "Cutting a release" checklist (do not run it unprompted).
- The `Stop` hook (`check-tracking-sync.js`) will flag any index/detail drift — fix it before ending.

### 7. Relay to the user

Report: the merged PR URL + squash SHA on `main`, what verification ran and its result, the
tracking/board updates made, and anything left out of scope.

## Notes

- Assumes the `CODE` already went through discussion→design→todo formalization (`/CLAUDE.md`). A
  `CODE` still sitting in `features/<slug>/planning.md` with open questions → resolve planning
  first; this command is the wrong tool.
- Big/multi-`CODE` features: don't run this per sub-task against `main`. Set up the
  `feat/<feature-slug>` integration branch first (`github` skill "Branch model"), then the
  sub-task PRs target that branch instead of `main`.
- A subagent's summary describes intent, not outcome — step 4 is not optional.
