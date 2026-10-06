# Sprint 20 (closed 2026-10-06)

**Goal:** Fix the model↔engine layering violation and narrow CommandContext (architecture review follow-ups)
**Dates:** 2026-10-05 to 2026-10-06.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| ARCH-01 | Break the model ↔ engine include cycle (move analysis data types into `model/`) | ✅ DONE |
| ARCH-03 | Narrow `CommandContext` + split `registerBuiltins()` | ✅ DONE |

Both items were pulled from the Backlog at sprint open (estimates 2 + 3 pts, taken from the Backlog). ARCH-02 (needs design) stayed in the Backlog. One out-of-sprint quick fix also landed on `main` (PR #40, no CODE — see below).

## What shipped

- **ARCH-01** (PR #41 squash `2874af3`): the analysis domain structs (`PVLine`, `EngineStatus`, `DatabaseEntry`, `AnalysisOverlayCell`, `AnalysisOverlay`) moved verbatim to `src/model/analysis_types.h`; `engine/engine_types.h` keeps only `EngineMessageType` and re-includes the model header (engine → model). No `src/model` header includes `engine/` any more. New ctest `arch01-model-no-engine-includes` guards it (matches `#include` lines only). `CLAUDE.md` rule 8's "currently violated" note removed. Detail: `docs/fix-log/2026-10-05-arch-01-break-model-engine-cycle.md`.
- **ARCH-03** (PR #42 squash `7dabb0e`): `registerBuiltins()` split into seven per-help-group private registrars; `EngineProcess&` removed from `CommandContext` (handlers use the new `EngineController::isRunning()` forwarder — the only new controller API). New `test_arch03_help_pinned` pins the full `!help` output and the sorted names + usage strings, with the golden captured from pre-refactor code. Internal registration order changed but is unobservable (outputs are sorted). Detail: `docs/fix-log/2026-10-06-arch-03-narrow-command-context.md`.
- **Out of sprint** (PR #40 squash `9f896ff`, no CODE): `INFO …` visual-search lines are no longer echoed into the Engine Log. Detail: `docs/fix-log/2026-09-10-info-lines-flood-engine-log.md`.

## Lessons

- **Capture the golden before refactoring.** ARCH-03 pinned `!help` and the names/usage list from the pre-change code first, so "byte-identical" was a test result rather than a claim. Reuse for ARCH-02.
- **A grep acceptance criterion must say what it matches.** ARCH-01's literal `grep 'engine/' src/model` also hits a string literal and a comment; the real rule is about `#include` lines. Write acceptance greps as the exact pattern.
- **Create the sprint's GitHub labels at open and check they exist.** `sprint:20` / `area:ARCH` were not on the repo when the first task started; `gh label create --force` is idempotent, so run it at `open` and re-run in preflight.
- **The build host has no engine binary and no display.** Carried from Sprint 19: live-engine checks (e.g. the Engine Log staying quiet during a real search after PR #40) remain a human step.

## Rolled over to Backlog

Nothing rolled over — all committed items finished.

## Next sprint

Sprint 21 — not yet opened. The only open Backlog item is ARCH-02 (8 pts, needs design: resolve its `features/` planning questions first). Release `v0.8.1` cut at close (see `CHANGELOG.md`).
