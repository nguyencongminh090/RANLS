# Instruction — ARCH-01

Detail: [docs/todo/ARCH-01-break-model-engine-include-cycle.md](../todo/ARCH-01-break-model-engine-include-cycle.md)

## Approach

Pure header move. Classify each type first; move; fix includes; build.

## Pitfalls

`engine_types.h` also includes `model/board_state.h` — leaving a forwarding header in `engine/` that re-includes the new `model/` header is fine, the reverse is not.

## Verification before done

`grep -rn 'engine/' src/model` empty; `RUN_TESTS=1 ./build.sh` green.

## Boundaries

Types keep their shape; no logic edits.
