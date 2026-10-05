# ARCH-01: break the model <-> engine include cycle (2026-10-05)

**Status:** Active
**Related:** ARCH-01

## Prompt

Implement ARCH-01 (architecture review 2026-10-05): `model/game_state.h` and `model/board_view_model.h` include `engine/engine_types.h`, which includes `model/board_state.h`.

## Root cause

Analysis domain structs (`PVLine`, `EngineStatus`, `DatabaseEntry`, `AnalysisOverlay*`) were defined in the engine layer next to the protocol-only `EngineMessageType`, so the model layer had to include engine/.

## Fix

New `src/model/analysis_types.h` holds the five domain types verbatim. `src/engine/engine_types.h` keeps `EngineMessageType` and re-includes the model header (engine -> model). The two model headers now include `model/analysis_types.h`. No logic or shape changes. CLAUDE.md rule 8 no longer lists the violation.

## Verification

Regression guard: ctest `arch01-model-no-engine-includes` (`tests/arch01_check_model_includes.cmake`, `#include` lines only; negative case checked by planting an include). Release `RUN_TESTS=1 ./build.sh`: 5/5 ctest; `ranls-gui-tests` 228 cases / 2625 assertions pass, same as clean main; ui-tests, port02, rel02 pass.

## Knowledge

`docs/knowledge/principle-layering-dependency-rule.md`, `docs/audit/2026-10-05-architecture-review.md`.
