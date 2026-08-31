# REL-01 — CHANGELOG.md, release checklist, and backfilled history

**Status:** Backlog
**Area:** release/versioning
**Priority:** P3
**Source:** `docs/notes/2026-08-30-versioning-and-changelog.md` → `features/versioning-and-changelog/`

## Context

YixinBoard is a shipped application with no version history a user can read. `docs/fix-log.md` is
per-fix and internal; `docs/sprint/archive/` is per-sprint and internally framed. There is no
user-facing "what changed per release" record and no git tags.

Decided with the user (2026-08-30): SemVer `0.x` series; root `CHANGELOG.md` in "Keep a Changelog"
format; backfill the history from Sprints 1–6; release cadence = sprint close.

See `features/versioning-and-changelog/user_story.md` (rules, hard constraints) and `planning.md`
(open question Q1 — the exact starting version number — must be resolved at pickup).

## Scope

- Create root `CHANGELOG.md`, "Keep a Changelog" format: `## [Unreleased]` at top, then
  `## [0.x.y] - YYYY-MM-DD` sections newest-first, grouped `Added / Changed / Fixed / Removed`.
- **Backfill**: reconstruct the shipped work (Sprints 1–6) from `docs/sprint/archive/` +
  `docs/fix-log.md` into changelog history, per planning.md Q1 (recommendation: a single
  `## [0.1.0]` covering everything through Sprint 6).
- Add a **"Cutting a release" checklist** — as a new section in the `github` skill: finalize the
  `[Unreleased]` section into a versioned one, commit `Release v0.x.y`, `git tag v0.x.y`,
  `git push --tags`; reset `[Unreleased]`.
- Update `CLAUDE.md` "Sprint cadence" so closing a sprint includes cutting the release + tag.
- Doc/process only — **no code, no CMake change** (that is REL-02).

## Acceptance criteria

- `CHANGELOG.md` exists, valid "Keep a Changelog" structure, backfilled through the current state.
- The current `main` commit is tagged (version per Q1) and the tag is pushed.
- The release checklist is written and linked from `CLAUDE.md` "Sprint cadence".
- No source file changed.

## Related

- REL-02 (version string single-source) — sibling, can proceed independently.
- `github` skill "Future" note (Releases) and `docs/audit/2026-08-30-github-project-management.md`.
