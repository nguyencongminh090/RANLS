# ARCH-01 — Break the model ↔ engine include cycle

**Status:** ✅ DONE
**Area:** `src/engine/engine_types.h`, `src/model/`, includes in `src/{engine,ui,command}` + `tests/`
**Priority:** P1
**Source:** architecture review, 2026-10-05 — [docs/audit/2026-10-05-architecture-review.md](../audit/2026-10-05-architecture-review.md)
**Design:** none — scoped directly
**Depends on / relates to:** ARCH-03 (do this first)

## Problem

`model/game_state.h` and `model/board_view_model.h` include `engine/engine_types.h`, which includes `model/board_state.h`. The model layer therefore depends on the engine layer, contradicting the dependency rule (`ui → command/engine → model`). The types involved (`PVLine`, `EngineStatus`, `DatabaseEntry`, `AnalysisOverlay*`) are analysis domain data, not protocol types.

## Scope (in order)

1. Decide per type: domain data (→ `model/`) vs protocol-only (stays in `engine/`).
2. Move the domain types to a `model/` header; leave no forwarding include that re-creates the cycle.
3. Update includes in `engine/`, `ui/`, `command/`, `tests/`.

## Reasoning

Move, don't wrap: a forwarding header in `model/` pointing at `engine/` would hide the cycle, not remove it. Rejected: moving everything incl. protocol-only enums (they belong with the protocol); a DIP interface for analysis data (over-engineered for plain value structs).

## Knowledge

- `docs/knowledge/principle-layering-dependency-rule.md` (dependency rule)
- `docs/knowledge/style-layered.md`
- `.claude/skills/software-architecture` (layer rule), `.claude/skills/data-architecture` (PVLine/DatabaseEntry ownership)
- Audit: `docs/audit/2026-10-05-architecture-review.md`

## Acceptance criteria

- `grep -rn 'engine/' src/model` returns nothing.
- Build clean; full test suite unchanged (no behavior change).
- A grep check (script or CI step) keeps `src/model` free of `engine/` includes.

## Scope boundary

- Do not change type shapes or semantics.
- Do not touch `MainWindow` or `CommandContext` (ARCH-02/03).

## Outcome (2026-10-05)

- Classification: `PVLine`, `EngineStatus`, `DatabaseEntry`, `AnalysisOverlayCell`, `AnalysisOverlay` are domain data, moved verbatim to `src/model/analysis_types.h`. `EngineMessageType` is protocol-only and stays in `src/engine/engine_types.h`, which re-includes the model header (engine -> model) so existing includes compile unchanged.
- `model/game_state.h` and `model/board_view_model.h` include `model/analysis_types.h`; `grep -rn "#include.*engine/" src/model` is empty. (A bare `engine/` grep still matches a string literal in `config.h` and a comment in `game_state.h`; those are not includes.)
- Guard: ctest `arch01-model-no-engine-includes` (`tests/arch01_check_model_includes.cmake`), `#include` lines only; verified it fails on a planted include.
- Verification (Release): `RUN_TESTS=1 ./build.sh` clean; ctest 5/5 (was 4/4 plus new guard); `ranls-gui-tests` 228 cases / 2625 assertions, 0 failed, identical to clean main; ui-tests, port02, rel02 pass. The 17 EngineController timing failures did not reproduce on this host (clean main: 0 failed).
- Deviation: the acceptance grep `grep -rn engine/ src/model` is literally non-empty (config.h literal, comment); interpreted as include lines.
