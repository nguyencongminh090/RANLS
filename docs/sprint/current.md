# Current sprint

## Sprint 20

**Goal:** Fix the model↔engine layering violation and narrow CommandContext (architecture review follow-ups)

**Dates:** 2026-10-05 to — (open — no fixed end date set yet)

**Dependency graph:**
- **ARCH-01** — `src/engine/engine_types.h` → new `model/` header, includes in `engine/ui/command/tests`. Move analysis data types (`PVLine`, `EngineStatus`, `DatabaseEntry`, `AnalysisOverlay*`) into `model/`, no forwarding header; must not change type shapes/semantics nor touch `MainWindow`/`CommandContext`. No debugging needed (refactor). Do first. Design call: per-type domain-vs-protocol split.
- **ARCH-03** — `src/command/command_dispatcher.{h,cpp}`. Drop `EngineProcess&` from `CommandContext` (route via `EngineController`), split `registerBuiltins()` per group; no new commands, `!help` output byte-identical. Depends on ARCH-01.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| ARCH-01 | Break the model ↔ engine include cycle (move analysis data types into `model/`) | — | 2 | ✅ Done (PR #41, squash `2874af3`) |
| ARCH-03 | Narrow `CommandContext` + split `registerBuiltins()` | ARCH-01 | 3 | ✅ Done (PR #42, squash `7dabb0e`) |

Points taken from the Backlog estimates (ARCH-01 2, ARCH-03 3).

**Lesson carried in from Sprint 19:** The build host has no engine binary and no display, so live-engine
acceptance is a human step; for pure refactors like these, "no behavior change" must be pinned by the existing
suites (`test_cons*`, `test_proto07_yxanalz_console`, `!help` byte-identical), not assumed.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `.claude/rules/sprint-cadence.md`.
