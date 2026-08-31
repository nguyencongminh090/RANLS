# Sprint 7 bypassed the per-CODE PR lifecycle

**Date:** 2026-08-31
**Type:** process deviation / correction

## What happened

Sprint 7 (`ENG-02`, `UI-08`, `UI-09`, `REL-02`) was implemented by hand in the main Claude session
and landed as a **single commit `314d434` pushed straight to `main`** — no per-`CODE` branch, no
PR, no `area:`/`sprint:` labels, no `Sprint 7` milestone, no board update. `REL-01` earlier in the
same sprint had the same shape (direct-to-`main` commits `0b7ff49`, `0bde181`). Only Sprint 6 ever
produced a PR (`#1`), and that one bundled six `CODE`s.

This contradicts the `github` skill "PR lifecycle for one CODE" and the `## GitHub project
management` rule in `CLAUDE.md` (one squash-merged PR per `CODE`).

## Why it went unnoticed

`main` has a branch-protection rule requiring a PR, but **`enforce_admins` is not set**, so pushes
by the repo owner bypass it silently. There is no CI and no `pre-push` hook. Nothing mechanical
stopped the manual push — the only guard was process discipline, and the dispatch tooling didn't
carry it: `/implement-task` (as written before today) dispatched a code-only subagent and stopped,
leaving branch/PR/merge/label/milestone entirely to a human step that got skipped.

## Decision

1. **Do not unwind `314d434`.** It is already on `origin/main`, tested, and tracked in
   `docs/fix-log.md`. Rewriting published `main` history for a solo repo to satisfy the process
   retroactively costs more than it's worth.
2. **Backfill the GitHub side** (done 2026-08-31): created the `area:*`, `sprint:1..7`,
   `needs-triage`, `blocked` labels (none had existed); created `Sprint 6` (#1) and `Sprint 7` (#2)
   milestones, both closed; assigned PR #1 to `Sprint 6` + `sprint:6`.
3. **Fix the tooling so it can't recur** (done 2026-08-31): rewrote `.claude/commands/implement-task.md`
   so the command itself drives branch → commit-on-branch → PR (with label + milestone) →
   squash-merge → sprint/board sync, and the dispatched agent explicitly must **not** touch `main`.
   `.claude/skills/github/SKILL.md` gained a "Low ceremony is not no ceremony" section, a retro,
   and preflight steps that bootstrap missing labels/milestones. `AGENTS.md` updated to match.
4. **From Sprint 8 on**, every `CODE` goes through `/implement-task` or the identical by-hand PR
   lifecycle. Considered `enforce_admins: true` on `main` but rejected — the project deliberately
   allows doc-only tracking edits to land directly on `main`, which that flag would also block.

## Follow-up (not yet done)

- Projects board: the current `gh` token lacks `project` scope (`gh auth refresh -s project`) and
  Projects-classic is deprecated; stand up a new-experience board and hand-sync at sprint close.
