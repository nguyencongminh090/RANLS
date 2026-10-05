# DOC-02 — Standardize CLAUDE.md (hard rules + pointers; move workflows out)

**Status:** ✅ DONE (2026-10-05; no active sprint — filed and done on branch `doc-02/claude-md-standardize`)
**Area:** `CLAUDE.md`, `.claude/rules/`, `.agents/rules/`, `.claude/skills/github/SKILL.md`
**Priority:** P2
**Source:** user request after the 2026-10-05 architecture/audit work — [docs/audit/2026-10-log.md](../audit/2026-10-log.md)
**Design:** none — scoped directly (size L: process change to a file loaded every session)
**Depends on / relates to:** task-size/template work (branch `docs/audit-process-templates`)

## Problem

`CLAUDE.md` (201 lines) is loaded every session but contradicted its own preamble ("activity-specific
workflows live in skills — don't duplicate them here") by embedding ~90 lines of bug-fix, sprint and
GitHub workflow. It also said "GTK3/gtkmm" (project is GTK4/gtkmm-4.0), cited uninstalled
`superpowers:*` skills without saying so up front, and mixed hard rules with process description.

## Scope (in order)

1. Fix the GTK3 → GTK4 statement.
2. Add a numbered **Hard rules** section (9 rules) at the top.
3. Move the bug-fix pipeline to `.claude/rules/bugfix.md` and sprint cadence to
   `.claude/rules/sprint-cadence.md` (path-scoped, verbatim); mirror via `.agents/rules/` symlinks.
4. Move the GitHub model/role section verbatim into the `github` skill.
5. Keep section headings that other live files cite ("Sprint cadence", "Bug-fix workflow", "Audit",
   "GitHub project management") as short pointer sections.

## Scope boundary

- No wording changes to the moved pipeline/cadence/GitHub text beyond the `superpowers` clarification.
- Historical docs (fix-log, audit, archived sprints) that cite old section names are not edited.
- Do not edit `~/.claude/CLAUDE.md`.

## Reasoning

A file loaded every session should carry rules that must always hold plus pointers; procedures belong
where they load only when relevant. Path-scoped rules were chosen over skills for bug-fix/sprint
because they auto-load when touching `src/**`/`docs/sprint/**`; hard rule 2 names `bugfix.md`
explicitly because a bug report may arrive before any `src/` file is opened. Rejected: deleting the
old headings (would break citations in `sprint.md`, `assign-task.js`, `perf-optimization`); a numeric
line target as a hard goal (clarity over count — landed at 113 lines, not ≤ 80).

## Knowledge

- `docs/knowledge/principle-solid-overview.md` (SRP applied to documents), `quality-tradeoff-analysis.md`
- Audit: `docs/audit/2026-09-03-antigravity-workspace-sync.md` (GEMINI.md is a symlink to CLAUDE.md →
  new rules mirrored under `.agents/rules/`), `docs/audit/2026-08-31-systematic-debugging-skill-and-pipeline.md`

## Acceptance criteria

- `CLAUDE.md` says GTK4; has a Hard rules section; ≤ ~115 lines.
- `diff` of moved sections vs. the saved original shows only the intended edits.
- Every old heading still present; `node scripts/check-task-structure.js` OK.
- `.agents/rules/` symlinks resolve.
