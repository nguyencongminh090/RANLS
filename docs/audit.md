# Audit log

Index of reviews and non-bug decisions: architecture choices, security reviews, protocol-
compatibility checks against Rapfi/Yixin-protocol changes, build/toolchain decisions. Not a bug
(→ `docs/fix-log.md`) and not a feature (→ `features/<slug>/`).

Append-only, same shape as the fix log: one new `docs/audit/<date>-<slug>.md` detail file, one new
row here. Never edit/reorder/delete an existing row — a wrong past entry gets a new correcting entry.

| Date | Summary | Detail |
|---|---|---|
| 2026-08-21 | Test framework choice for TEST-01: doctest (vendored), no gtkmm-transitive-dependency issue found | [docs/audit/2026-08-21-test-framework-choice.md](audit/2026-08-21-test-framework-choice.md) |
| 2026-08-21 | UX-03: BoardView/TreeNodeView/WinGraphView have no keyboard focus mechanism to draw an indicator for, and no accessible role — accepted as a limitation pending a separate keyboard-navigation feature, not fixed in this pass | [docs/audit/2026-08-21-custom-drawn-widgets-no-keyboard-focus.md](audit/2026-08-21-custom-drawn-widgets-no-keyboard-focus.md) |
| 2026-08-31 | REL-01: stood up user-facing versioning — root `CHANGELOG.md` ("Keep a Changelog", SemVer 0.x) backfilled as a single `## [0.1.0]` covering Sprints 1–6, a "Cutting a release" checklist in the `github` skill + one-line `CLAUDE.md` pointer, and tag `v0.1.0` pushed. Doc/process only (version-string single-sourcing stays REL-02) | [docs/audit/2026-08-31-changelog-and-release-process.md](audit/2026-08-31-changelog-and-release-process.md) |
| 2026-08-30 | Development model identified as personal Scrumban + V-Model traceability spine; adopted a GitHub strategy (per-CODE squash-merge PRs, thin Issues, prefix-mirrored labels, milestone=sprint, hand-synced Projects board) with local `docs/` staying canonical — new `## GitHub project management` CLAUDE.md section + `github` skill | [docs/audit/2026-08-30-github-project-management.md](audit/2026-08-30-github-project-management.md) |
| 2026-08-31 | Installed `obra/superpowers@systematic-debugging` skill and merged it with the user's localization pipeline + existing project skills into a 7-phase "diagnosis pipeline" in `CLAUDE.md` (evidence → reproduce → localize → trace-to-source → hypothesis/minimal-test → fix+defense-in-depth → verify), with a phase→skill map. Doc-only. | [docs/audit/2026-08-31-systematic-debugging-skill-and-pipeline.md](audit/2026-08-31-systematic-debugging-skill-and-pipeline.md) |
| 2026-08-31 | Sprint 7 (`ENG-02`/`UI-08`/`UI-09`/`REL-02`, + `REL-01`) bypassed the per-CODE PR lifecycle — landed as hand commits pushed straight to `main` (branch protection has no `enforce_admins`; `/implement-task` was code-only). Decision: don't unwind `314d434`; backfilled labels + `Sprint 6`/`Sprint 7` milestones; rewrote `/implement-task` + `github` skill to drive branch→PR→merge→sync so it can't recur | [docs/audit/2026-08-31-sprint-7-bypassed-pr-lifecycle.md](audit/2026-08-31-sprint-7-bypassed-pr-lifecycle.md) |
