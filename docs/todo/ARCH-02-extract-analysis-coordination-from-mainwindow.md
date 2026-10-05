# ARCH-02 — Extract analyze/auto-move orchestration out of MainWindow

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/main_window.{h,cpp}`, new GTK-free class(es) in `src/engine/` or a new layer
**Priority:** P2
**Source:** architecture review, 2026-10-05 — [docs/audit/2026-10-05-architecture-review.md](../audit/2026-10-05-architecture-review.md)
**Design:** features/<slug>/ required first — none yet (design gate: choose target layer)
**Depends on / relates to:** ARCH-01

## Problem

`MainWindow` (1329-line cpp, ~38 methods) mixes widget construction with application logic: analyze-mode restart debounce, auto-move, engine-plays state, file load/save orchestration, graceful close. Logic inside a GTK class is hard to unit-test and violates SRP.

## Scope (in order)

1. Write `features/<slug>/` (user story, diagram, planning) and resolve open questions with the user.
2. Add characterization tests around current analyze/auto-move behavior.
3. Extract one responsibility at a time into GTK-free classes (e.g. `AnalysisCoordinator`, `GameFileService`); `MainWindow` keeps widgets + signal wiring.

## Reasoning

Orchestration rules (when to analyze/auto-move) are application logic and should be testable without GTK; the timer stays in the UI because tests have no main loop (RT-01). Rejected: leaving it and adding more tests through `MainWindow` (needs a display); a rewrite (too risky without characterization tests).

## Knowledge

- `docs/knowledge/principle-solid-overview.md` (SRP), `docs/knowledge/style-ui-mvc-family.md`, `docs/knowledge/pattern-facade.md`
- `.claude/skills/solid-single-responsibility`, `.claude/skills/software-architecture`
- Audit: `docs/audit/2026-10-05-architecture-review.md`

## Acceptance criteria

- Behavior unchanged; `test_anlz*`, `test_eng*`, `test_rt01*` pass.
- Extracted logic is unit-tested without GTK.
- `main_window.cpp` is materially smaller.

## Scope boundary

- Not a UI redesign; no menu/shortcut changes.
- No code before the design gate is cleared.
