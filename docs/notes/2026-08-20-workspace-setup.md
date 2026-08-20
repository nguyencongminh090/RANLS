# 2026-08-20 — Workspace setup: Scrum + tracking-file scaffold

## Context

Set up `CLAUDE.md`, `TODO.md`/`instruction.md` + `docs/todo,instruction,design/`,
`docs/sprint/{current,burndown,archive}`, `docs/audit.md`, `docs/fix-log.md`, `docs/notes/`,
`features/`, and `AGENTS.md` for YixinBoard, per user request for an Agile Scrum workspace
(Audit, Agent Manage, TODO with Backlog/Sprint/Burndown, Fix log, Notes).

## Why this exact layout

Surveyed two sibling projects first (both on this machine, both maintained with Claude Code):
- `Rapfi` (`…/Rapfi_V1/rapfi/Rapfi/`) — the engine YixinBoard most commonly drives. Already uses
  `TODO.md` + `docs/todo,instruction,design/` at its root.
- `gomoku-vn` (`…/HTML/gomoku-vn/`) — unrelated web Gomoku app, but already uses the same
  `TODO.md`/`instruction.md`/`docs/fix-log.md`/`features/<slug>/` convention plus a `.claude/rules/
  tracking-files.md` path-scoped rule and a `Stop` hook (`scripts/check-tracking-sync.js`) enforcing
  index/detail sync.

Since the same convention is already independently established in two other projects this user
works in, YixinBoard's `.claude/commands/implement-task.md` (already present here, unused until now
— its default `project-dir` argument was still `Rapfi`, a stale leftover) was clearly written
against this convention. Reused it rather than inventing a parallel structure, and layered the
Scrum-specific asks (Sprint/Burndown/Audit/Agent-management) on top rather than replacing anything.

## What's new relative to the two precedents

- `docs/sprint/{current,burndown,archive}` — neither precedent project tracks sprints; this is
  new, addressing the "Sprint/Burndown graph" part of the request.
- `docs/audit.md` — gomoku-vn has no separate audit log (security reviews went straight into
  `TODO.md`); split out here as its own append-only index per the request's "Audit" item.
- `AGENTS.md` — neither precedent has one; model-tier tagging existed inline in gomoku-vn's
  `TODO.md` (`[Model: Sonnet 5]` etc.) but wasn't its own file. Wrote it as first-class here.

## Follow-ups not done yet (left for explicit ask, not invented as scope)

- `scripts/check-tracking-sync.js` was ported from gomoku-vn (English status verbs
  `DONE`/`FIXED`/`CLOSED`/`VERIFIED` instead of its Vietnamese ones) and works standalone via
  `node scripts/check-tracking-sync.js --full`. Wiring it as a `Stop` hook in
  `.claude/settings.local.json` (auto-run after every turn) was blocked by the auto-mode permission
  classifier — editing hook config that runs commands automatically needs explicit user action, not
  agent-driven edits. User chose to skip it (2026-08-20) rather than add it manually right now; the
  JSON snippet is documented in `.claude/rules/tracking-files.md` if they want it later.
- No first sprint has been planned — `docs/sprint/current.md` is a template until the user files
  Backlog items and asks to start sprint 1.
