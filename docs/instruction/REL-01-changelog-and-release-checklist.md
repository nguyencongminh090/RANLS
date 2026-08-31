# REL-01 — changelog-and-release-checklist

## Approach

Doc + process only. Resolve planning.md **Q1 (starting version number)** with the user before
writing `CHANGELOG.md` — the recommendation is one `## [0.1.0]` covering everything shipped through
Sprint 6, then tag current `main` as `v0.1.0`.

Backfill source order: read each `docs/sprint/archive/sprint-N.md` for the "what shipped" framing,
cross-check against `docs/fix-log.md` rows, then compress to **user-impact** lines — not `CODE`
lists, not file names. A `CODE` in trailing parens is allowed for traceability.

## Pitfalls

- "Keep a Changelog" format is specific: ISO dates, `## [x.y.z] - YYYY-MM-DD`, the `[Unreleased]`
  section stays at the top after every release, link-reference definitions at the bottom optional.
- Don't restructure `docs/fix-log.md` or `docs/sprint/` — `CHANGELOG.md` is additive and derived.
- The release checklist belongs in the `github` skill (next to "Branch model" / PR lifecycle), not
  duplicated into `CLAUDE.md`; `CLAUDE.md` "Sprint cadence" gets only a one-line pointer.

## Boundaries — do not touch

- No source files. No `CMakeLists.txt`. No git tag pushed until the user confirms the version.
