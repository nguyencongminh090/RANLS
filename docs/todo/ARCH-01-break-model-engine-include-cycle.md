# ARCH-01 — Break the model ↔ engine include cycle

**Status:** 🔲 OPEN (Backlog)
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
