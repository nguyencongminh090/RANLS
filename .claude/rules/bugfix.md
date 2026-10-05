---
paths:
  - "src/**"
  - "tests/**"
  - "docs/fix-log.md"
  - "docs/fix-log/**"
---

# Bug-fix workflow: diagnosis pipeline, scope discipline, unit tests

## Diagnosis pipeline (run before proposing any fix)

Invoke the `systematic-debugging` skill for any bug, test failure, or unexpected behavior — its
Iron Law ("no fixes without root-cause investigation first"), its hypothesis loop, and its
supporting technique files (`root-cause-tracing.md`, `defense-in-depth.md`,
`condition-based-waiting.md`) are the backbone. The phases below adapt it to this repo; don't skip
a phase or reorder it so localization comes before reproduction.

0. **Read the evidence.** Exact error text / stack trace / assertion verbatim; engine-protocol log
   lines and GTK warnings; and the regression window — `git log`/bisect for what changed. The exact
   message often contains the answer.
1. **Reproduce.** Deterministic steps; every time? Scope it (which board sizes / engines / move
   sequences trigger it). Not reproducible → add logging and gather data, do not guess.
2. **Localize.** CodeGraph (`codegraph_explore`) for candidate symbols/files. Rank by: touched in
   the regression window > on the failing call path > owns the bad state > matches the error text.
3. **Trace to source** (`systematic-debugging/root-cause-tracing.md`). For each ranked scope trace
   *backward* — symptom → immediate cause → caller that passed the bad value → … → original
   trigger. Crossing a boundary static tracing can't see (GUI ↔ engine stdin/stdout, a GTK signal/
   callback hop): add instrumentation logging data in and out of each component, run once, narrow.
4. **Hypothesis + minimal test.** State "root cause is X because Y"; make the smallest change that
   confirms or kills it; one variable at a time. Failed → new hypothesis, don't stack fixes. Three
   failed fixes → STOP and question the architecture with the user (likely wrong layer, not a
   wrong hypothesis).
5. **Fix at the source.** Failing regression test first (see below). One change, root cause only,
   no "while I'm here". Then `systematic-debugging/defense-in-depth.md`: validate the bad value at
   each layer it passed through so the bug becomes structurally impossible.
6. **Verify + record.** Test passes, nothing else breaks, symptom actually gone → `docs/fix-log.md`
   row + `docs/fix-log/<date>-<slug>.md` detail.

Which project skill pairs with which phase: `perf-optimization` for phases 1–3 of a freeze/stutter/
lag report (always measure first); `gtk-ui-design` when the trace lands in `src/ui/` (draw_func,
ListStore, RefPtr lifetime); `data-architecture` when a model invariant (`BoardState`,
`VariationTree`, `PVLine`) is violated; `software-architecture` at phase 4 when the fix would cross
a layer boundary. `systematic-debugging` references `superpowers:test-driven-development` /
`verification-before-completion`, which are not installed — use the regression-test rule below instead.

## Scope discipline and unit tests

- **Base the fix strictly on what was provided.** Don't silently extend a fix to cover speculative
  scenarios beyond the reported bug — call those out separately (`TODO.md` Backlog) instead.
- **Write a regression test for the fix whenever the affected code has, or can reasonably get, real
  coverage.** If the area has no test infrastructure, say so explicitly rather than skipping silently.
- **Never discard a test case after writing it.** It's the permanent regression guard, not a
  one-time proof.
- **Before implementing any `TODO.md` task, read the matching `instruction.md` entry.** Missing entry
  is fine — not every task has one. If a fix deviates from `instruction.md`, note why in the summary.
- Every fix gets a `docs/fix-log.md` row + `docs/fix-log/<date>-<slug>.md` detail file, whether or
  not it started as a tracked `TODO.md` item.
