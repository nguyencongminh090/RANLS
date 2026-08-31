---
name: github
description: GitHub workflow for YixinBoard — branch naming, the PR lifecycle for one TODO.md CODE, commit-message format, squash-merge and main protection, the label/milestone scheme, and how the GitHub Projects board maps to TODO.md. Use when opening a PR, creating a branch, filing or triaging an Issue, updating the Projects board or a sprint milestone, or reconciling GitHub state with the local tracking files. Not for the local tracking convention itself (see CLAUDE.md "Process model" + .claude/rules/tracking-files.md).
---

# GitHub workflow — YixinBoard

Repo: `github.com/nguyencongminh090/RANLS`. Solo developer, AI-agent-assisted. **The local
tracking files (`TODO.md`, `docs/todo/`, `docs/instruction/`, `docs/sprint/`, `docs/fix-log.md`,
`docs/audit.md`) are the single source of truth.** GitHub adds review surface, CI history, and a
visual board — it never becomes a second backlog. Every GitHub object (branch, PR, Issue,
milestone) references the local task `CODE` (e.g. `UI-07`, `STATE-04`); when GitHub and the local
docs disagree, the local docs win and you fix GitHub.

Keep ceremony low: no required reviewers (would block a solo dev), no per-commit board sync. The
board and milestones are reconciled only at sprint planning and sprint close.

**Low ceremony is not "no ceremony".** These are mandatory, not aspirational:

- **One squash-merged PR per `CODE`** — never one bundled PR per sprint, never a hand commit on
  `main` for code. `main` has a PR rule but it does *not* enforce on the repo owner, so a bad
  manual push will silently succeed — the discipline is yours to keep, not the server's.
- The `area:<prefix>` + `sprint:<N>` label, the `Sprint <N>` milestone, and the board card get
  created/assigned **as part of shipping the `CODE`** (see the PR lifecycle) — not left for a
  cleanup pass that never happens.
- `/implement-task <CODE>` automates this whole lifecycle. Doing a `CODE` by hand means doing the
  identical seven "PR lifecycle" steps below by hand — same steps, nothing dropped.

### Lessons (retro)

- **Sprint 6** shipped as one PR covering six `CODE`s (`fix/ui-07-pv-cross-position`, #1). Too
  coarse — a revert or review comment can't isolate one task. One PR per `CODE` from here.
- **Sprint 7** (ENG-02, UI-08, UI-09, REL-02) was implemented by hand in the main session and
  pushed as a single commit (`314d434`) straight onto `main` — no branches, no PRs, no
  labels/milestone/board. It went through because branch protection here does **not** enforce on
  the repo owner (no `enforce_admins`), so nothing mechanically stops a bad manual push; the guard
  is process discipline, i.e. this skill. Since `314d434` is already on `origin/main`, don't try to
  unwind it — instead backfill: create the `Sprint 7` milestone + labels, and log the deviation in
  `docs/audit.md`. From Sprint 8 on, every `CODE` goes through the PR lifecycle / `/implement-task`.

## Branching

- Work happens on a short-lived branch off `main`, one branch per `CODE`.
- Name: `<code-lowercased>/<short-slug>` — e.g. `ui-07/pv-cross-position`, `state-04/persist-rule`.
  The `CODE` prefix already encodes the area, so no `fix/` prefix. (The one longer-lived exception
  is a `feat/<feature-slug>` integration branch — see "Branch model".)
- `/implement-task` dispatches into an isolated git worktree; those `worktree-agent-<hash>` branches
  are harness-generated and fine to squash-merge as-is — rename to the convention only if you push
  one for review.
- Delete the branch after merge (`gh pr merge --delete-branch`).
- Never commit directly to `main` **except** doc-only tracking-file edits, which `CLAUDE.md` already
  permits (`features/<slug>/`, `docs/notes/`, backlog/sprint bookkeeping, this skill, audit rows).

## Branch model — single-trunk now, feature branch when needed

**Default (fixes and small tasks): single trunk.** `main` is the trunk *and* the closest thing to
"production". No permanent `develop`/`dev` branch — with WIP ≈ 1 and no Releases yet, a second
long-lived branch would just be `main` with a delay. Each task branches off `main` and squash-merges
straight back (see "PR lifecycle"). This covers the current mode of work (fix patches).

**When a feature is too big or too risky for one PR** — multi-task, spans layers, or you want to
keep shipping fixes while it stabilises — use a **feature integration branch**, the code counterpart
of the `features/<slug>/` design folder:

1. `git switch -c feat/<feature-slug> main` — one longer-lived branch for the whole feature.
2. Break the feature into `CODE`s (`docs/todo/`). Each sub-task gets its own short branch **off
   `feat/<feature-slug>`**, PR'd **into** `feat/<feature-slug>` (not `main`), squash-merged there.
3. Rebase the feature branch on `main` regularly (`git rebase main` or a merge) so it keeps
   absorbing the fixes landing on the trunk. Resolve conflicts here, not at the end.
4. When the feature is complete and its tests pass, open **one** PR from `feat/<feature-slug>` into
   `main`. Merge (not squash — keep the per-`CODE` commits) so each sub-task stays a distinct commit.
5. `main` stays releasable throughout: fix PRs merge to `main` normally in parallel.

**Need a stable build to demo/ship mid-feature:** cut it from `main`, never from the feature branch.
Simplest is a tag (`git tag v0.3.0-demo && git push --tags`); if the demo needs its own patches, a
short-lived `release/v0.3` branch off `main`. Delete/forget it once the feature lands.

**Escalate to a permanent `develop` only if** a second regular contributor joins and you have
overlapping features in flight at once — not before. Until then it is pure overhead.

## Commit messages

Match existing history: `<CODE>: <imperative summary>` for the squashed commit (e.g.
`Fix UI-02: tree Table tab can't click-to-jump`, `STATE-04: persist rule and board size`).
Intermediate commits on the branch are unconstrained — the squash message is what lands on `main`.
Keep the `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` trailer.

## PR lifecycle for one CODE

This is what `/implement-task` runs. By hand, run the same steps — do not shortcut.

0. Preflight: `git fetch origin && git switch main && git pull --ff-only`, working tree clean.
   Ensure the `area:<prefix>` + `sprint:<N>` labels and the `Sprint <N>` milestone exist (create
   the missing ones now — see "Labels" and "Milestones = sprints" below).
1. Branch: `git switch -c <code>/<slug>` off an up-to-date `main` (`<slug>` from the
   `docs/todo/<CODE>-<slug>.md` filename).
2. Implement per `docs/instruction/<CODE>-*.md`; write the regression test (`CLAUDE.md` bug-fix
   rule). Commit in chunks, every message `<CODE>: <imperative summary>` with the `Co-Authored-By`
   trailer. Update the tracking files **on this branch** (`docs/todo/` ✅ + `TODO.md` ✅ +
   `docs/fix-log.md` row + detail file).
3. Push and open the PR:
   ```
   gh pr create --title "<CODE>: <summary>" \
     --body "Closes the local task in docs/todo/<CODE>-<slug>.md

   <what changed, test evidence: e.g. 129/129 + UI tests pass>

   Refs #<issue-number if one exists>" \
     --milestone "Sprint <N>" --label "area:<prefix>,sprint:<N>"
   ```
   The PR body **links the local detail file** (not "Closes #" unless a GitHub Issue exists — a
   local `docs/todo/` file is not an Issue).
4. Let CI run when it exists (none yet — see Future). Self-review the diff.
5. Squash-merge: `gh pr merge --squash --delete-branch`. Linear history, one commit per task.
6. Same turn, back on `main` (`git switch main && git pull --ff-only`): move the `TODO.md` line
   out of **Active** (already ✅ from step 2), update `docs/sprint/current.md` (→ Done) and add a
   `docs/sprint/burndown.md` row. These are doc-only, straight to `main`. The `Stop` hook
   (`check-tracking-sync.js`) enforces index/detail sync — fix any drift before ending the turn.
7. Move the board card to Done (or defer to the sprint-close batch). If this was the sprint's last
   Active item, the "Cutting a release" checklist is next (on request).

## Issues — thin and optional

The local `docs/todo/` tree is the backlog; do **not** mirror every `CODE` into an Issue. Open a
GitHub Issue only for:
- an externally reported bug (contributor / user), which you then triage into a `docs/todo/<CODE>`
  file — the Issue is the intake, the `CODE` file is the tracked work;
- an item you specifically want visible on the Projects board with a live checklist.

Issue title: `<CODE>: <summary>` once triaged (before a CODE is assigned: plain description +
`needs-triage` label). Always cross-link Issue ↔ `docs/todo/<CODE>-*.md` in the Issue body.

## Labels

Mirror the `TODO.md` prefixes exactly as `area:` labels:
`area:RT` `area:STATE` `area:PROTO` `area:ENG` `area:NAV` `area:UI` `area:UX` `area:TEST`
`area:CLEAN` `area:IO` `area:DOC` `area:TOOL` `area:REL` (add a label when you add a prefix to `TODO.md`).

Plus: `sprint:<N>` (current-sprint items), `needs-triage` (Issue not yet a CODE),
`blocked` (mirrors a "Depends on" in `docs/sprint/current.md`).

These are **not yet created on the repo** (only GitHub's default label set exists). Bootstrap:
```
for p in RT STATE PROTO ENG NAV UI UX TEST CLEAN IO DOC TOOL REL; do
  gh label create "area:$p" --force; done
gh label create needs-triage --force; gh label create blocked --force
```
Thereafter each PR lifecycle (step 0) creates the specific `area:<prefix>` / `sprint:<N>` label it
needs if still missing — `gh label create "<name>" --force` is idempotent.

## Milestones = sprints

- One milestone per sprint, titled `Sprint <N>`, description = the sprint goal from
  `docs/sprint/current.md`. Due date only if the sprint has a fixed end (several have been
  open-ended — leave it unset then). **None exist yet** — the first PR of a sprint creates it:
  `gh api repos/:owner/:repo/milestones -f title="Sprint <N>" -f state=open -f description="<goal>"`.
- Assign a PR/Issue to `Sprint <N>` when its `CODE` is pulled into `docs/sprint/current.md` Active.
- Close the milestone when the sprint is archived to `docs/sprint/archive/sprint-<N>.md`.

## Projects board

A board named "YixinBoard" with three columns mirroring `TODO.md`:

| Column  | Mirrors                                   |
|---------|-------------------------------------------|
| Backlog | `TODO.md` **Backlog** section             |
| Active  | `docs/sprint/current.md` committed items  |
| Done    | ✅ items in the current + last sprint      |

The board is a **view of the local files, hand-synced at two moments only**: sprint planning (pull
Backlog→Active) and sprint close (Active→Done, archive). Do not chase per-commit accuracy. If the
board drifts from the local docs between those moments, that is expected and harmless — the docs
are canonical.

`gh project` needs the `read:project` / `project` token scope, which the current `gh` token lacks
— run `gh auth refresh -s project` once before any board command. If the board doesn't exist yet,
create it with `gh project create --owner nguyencongminh090 --title YixinBoard`. Until the scope is
added, the board steps are skipped and reconciled by hand at sprint close.

## Reconciling GitHub with local docs

- Local `docs/` wins every disagreement. `check-task-structure.js` / `check-tracking-sync.js` guard
  the local side; there is no automated GitHub sync — that is deliberate.
- At sprint close: verify each shipped `CODE` has a merged PR, its milestone is complete, its card
  is in Done. Fix GitHub to match the archive file, never the reverse.

## Releases and versioning

Versioning is **SemVer `0.x`**; the user-facing history is a root `CHANGELOG.md` ("Keep a
Changelog" format). A release is cut **at sprint close** (`v0.1.0` was cut 2026-08-31 covering Sprints 1–6; the next
sprint close becomes `v0.2.0`).

### Cutting a release

Run at sprint close, after the sprint's last Active item is ✅ and its work is on `main`. Doc-only,
so it lands straight on `main` — no branch/PR.

1. **Pick the version.** `0.MINOR.PATCH`: bump MINOR for a sprint that shipped a new user-visible
   feature, PATCH for a fix/polish-only sprint. An out-of-band hotfix is a PATCH bump on its own.
2. **Finalize the changelog.** In `CHANGELOG.md`, rename `## [Unreleased]` to
   `## [0.N.0] - YYYY-MM-DD` (today's ISO date), keeping only the categories that have entries.
   Confirm every line is user-impact phrasing, not a `CODE` list or file names (a trailing
   `(CODE)` for traceability is fine). Cross-check against the sprint archive's "What shipped" and
   the new `docs/fix-log.md` rows since the last tag.
3. **Add a fresh empty `[Unreleased]`** at the top: `## [Unreleased]` + `_Nothing yet._`.
4. **Update the link-reference definitions** at the bottom: point `[Unreleased]` at
   `compare/v0.N.0...HEAD` and add `[0.N.0]` → `releases/tag/v0.N.0`.
5. **Commit:** `git add CHANGELOG.md && git commit -m "Release v0.N.0"` (keep the
   `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` trailer).
6. **Tag and push:** `git tag v0.N.0 && git push && git push --tags`.
7. **Verify:** `git tag -l` shows `v0.N.0`; `git ls-remote --tags origin` shows it pushed.
8. Close the `Sprint <N>` milestone as part of the normal sprint-close batch.

GitHub Releases (a Release object on the tag with a built artifact) stay deferred until CI exists to
produce the artifact — see Future.

See `features/versioning-and-changelog/` and `REL-01` for the origin of this process.

## Future (not in scope now — file as backlog when wanted)

- **CI**: a GitHub Actions workflow building the GTK4 app and running the `doctest` +
  `rapfi-gui-ui-tests` suites on every PR; then make it a required check on `main`.
- **GitHub Releases**: mirror each `CHANGELOG.md` section into a Release on its `v<x.y.z>` tag with
  a distributable Linux build attached (needs CI to produce the artifact first).
