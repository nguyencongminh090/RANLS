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
| 2026-08-30 | Development model identified as personal Scrumban + V-Model traceability spine; adopted a GitHub strategy (per-CODE squash-merge PRs, thin Issues, prefix-mirrored labels, milestone=sprint, hand-synced Projects board) with local `docs/` staying canonical — new `## GitHub project management` CLAUDE.md section + `github` skill | [docs/audit/2026-08-30-github-project-management.md](audit/2026-08-30-github-project-management.md) |
