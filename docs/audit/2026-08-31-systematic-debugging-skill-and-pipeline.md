# 2026-08-31 — Adopt systematic-debugging skill + merged diagnosis pipeline

## Decision

Install the `obra/superpowers@systematic-debugging` skill and fold it, the user's own localization
pipeline, and this repo's existing skills into a single ordered diagnosis pipeline in `CLAUDE.md`.

- **Skill installed:** `obra/superpowers@systematic-debugging` (242K installs) →
  `.agents/skills/systematic-debugging/`, symlinked into `.claude/skills/`. Security scan at install:
  Gen *Safe*, Socket *0 alerts*, Snyk *Low Risk*. Bundles technique files `root-cause-tracing.md`,
  `defense-in-depth.md`, `condition-based-waiting.md`, and `find-polluter.sh` (test-pollution bisect).
- **`CLAUDE.md` "Bug-fix workflow"** renamed to "diagnosis pipeline, scope discipline, unit tests"
  and split into two subsections:
  - **Diagnosis pipeline** — 7 phases (0 read evidence + regression window → 1 reproduce →
    2 localize via CodeGraph → 3 trace-to-source backward → 4 single hypothesis + minimal test,
    3-failure architecture-stop rule → 5 fix at source + defense-in-depth → 6 verify + fix-log).
    The user's original `localize → scope → trace core reason` becomes phases 2–3, now gated by
    reproduction/evidence in front and a hypothesis loop after. Explicit "don't reorder so
    localization comes before reproduction."
  - **Scope discipline and unit tests** — the pre-existing bullets, unchanged.
- **Phase→skill map** added: `perf-optimization` (phases 1–3 of freeze/lag reports),
  `gtk-ui-design` (trace lands in `src/ui/`), `data-architecture` (model invariant violated),
  `software-architecture` (phase-4 fix crosses a layer boundary).

## Rationale

The user's pipeline was strong on static localization (candidate scope → ranking) but treated
reproduction as a fallback and had no explicit evidence-reading, hypothesis-testing, or stop rule —
the parts industrial systematic-debugging practice front-loads. Rather than replace the user's
approach, the merge keeps their localization step as a first-class phase and wraps the missing
scientific-method scaffolding around it. Keeping this in `CLAUDE.md` (not a new project skill)
matches how the repo already documents the bug-fix workflow.

## Boundaries honored

Doc-only: `CLAUDE.md` + this audit entry + the vendored skill files. No source, no `CMakeLists.txt`,
no changes to `docs/fix-log.md` / `docs/todo/` conventions.

## Follow-ups

- `systematic-debugging/SKILL.md` references `superpowers:test-driven-development` and
  `superpowers:verification-before-completion`, which are **not** installed. `CLAUDE.md` notes that
  the repo's own regression-test rule stands in. Install those two later only if the TDD framing is
  wanted wholesale.
