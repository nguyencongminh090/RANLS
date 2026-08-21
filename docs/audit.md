# Audit log

Index of reviews and non-bug decisions: architecture choices, security reviews, protocol-
compatibility checks against Rapfi/Yixin-protocol changes, build/toolchain decisions. Not a bug
(→ `docs/fix-log.md`) and not a feature (→ `features/<slug>/`).

Append-only, same shape as the fix log: one new `docs/audit/<date>-<slug>.md` detail file, one new
row here. Never edit/reorder/delete an existing row — a wrong past entry gets a new correcting entry.

| Date | Summary | Detail |
|---|---|---|
| 2026-08-21 | Test framework choice for TEST-01: doctest (vendored), no gtkmm-transitive-dependency issue found | [docs/audit/2026-08-21-test-framework-choice.md](audit/2026-08-21-test-framework-choice.md) |
