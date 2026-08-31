# 2026-08-31 — CHANGELOG.md, release checklist, and backfilled history (REL-01)

## Decision

Stand up user-facing versioning for YixinBoard as doc/process only (no code — that is REL-02):

- **Root `CHANGELOG.md`** in "Keep a Changelog" 1.1.0 format, SemVer `0.x`. `[Unreleased]` stays at
  the top; released sections are `## [x.y.z] - YYYY-MM-DD`, newest first, grouped
  Added / Changed / Fixed / Removed. Link-reference definitions at the bottom.
- **Backfill = a single `## [0.1.0] - 2026-08-31`** covering everything shipped through Sprint 6
  (planning.md Q1, resolved with the user at pickup: "least fiction" — do not invent six releases
  that were never cut). Sprint 7's close becomes `0.2.0`.
- Entries were reconstructed from `docs/sprint/archive/sprint-1..6.md` ("What shipped") cross-checked
  against `docs/fix-log.md`, then compressed to user-impact lines. `CODE`s appear only in trailing
  parens for traceability. Internal-only items (TEST-01, DOC-01, CLEAN-02, TOOL-01) were omitted
  from the changelog.
- **"Cutting a release" checklist** added to the `github` skill (next to the PR lifecycle), not
  duplicated into `CLAUDE.md`. `CLAUDE.md` "Sprint cadence" gets a one-line pointer to it.
- **Tag:** `v0.1.0` created on the current `main` commit and pushed, after `CHANGELOG.md` was
  committed.

## Rationale

`docs/fix-log.md` is per-fix and internally framed; `docs/sprint/archive/` is per-sprint and
internally framed. Neither is a "what changed for the user, per release" record, and there were no
git tags. `CHANGELOG.md` is a new *derived* artifact — the fix-log / sprint convention is unchanged.

## Boundaries honored

No source files, no `CMakeLists.txt`. `docs/fix-log.md` and `docs/sprint/` not restructured.
The version-string single-sourcing (CMake `VERSION` → `configure_file` → About dialog + `--version`
flag) remains REL-02.
