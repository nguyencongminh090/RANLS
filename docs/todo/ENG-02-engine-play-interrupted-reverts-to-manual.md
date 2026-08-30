# ENG-02 — Interrupting engine auto-play reverts "Engine plays" to Off (Manual Analyze)

**Status:** 📋 BACKLOG
**Area:** engine auto-move path (`src/main_window.cpp` auto-move / `src/engine/engine_controller.cpp`),
`MatchConfig` (`src/model/config.h`), "Engine plays" menu (`src/ui/`)
**Priority:** P2
**Source:** UI review request, 2026-08-30. Builds on UI-06 (`MatchConfig::enginePlays`).

## Problem / request

With "Engine plays" set to Black or White (UI-06), the engine auto-moves on its turn. The user
wants: if the user **interrupts** that arrangement — presses Stop, or manually asks the engine to
analyze the opponent's turn instead of letting it play its own — the app should drop back to
`EnginePlaysSide::Off` ("Mode None" / manual analyze). I.e. any manual intervention cancels
auto-play rather than leaving it armed to fire again on the next turn.

## To resolve with the user before implementing

1. **Exact triggers.** Which actions revert to Off? Candidates: toolbar Stop during an engine
   auto-move search; user invokes Analyze on a position where it is the engine's assigned turn;
   user makes a board move on the engine's assigned turn. List the definitive set.
2. **Both sides or one?** If engine plays Black and the user stops once, does White auto-play (if
   also assigned) also cancel? Likely yes — revert the whole `enginePlays` to Off.
3. **Feedback.** Menu radio updates to Off — any transient status message ("Auto-play cancelled")?
4. **Persistence.** The revert should update the in-memory `MatchConfig` and follow the normal
   STATE-04-style persistence path so it survives / is written like any other setting change.

## Acceptance criteria (draft — finalize after the above)

- After the agreed interrupt action, `MatchConfig::enginePlays == Off` and the "Engine plays" menu
  radio reflects Off.
- The engine does not auto-move again until the user re-selects a side.
- One-shot Analyze/Stop still works normally in the reverted state.

## Scope boundary

- Do not change UI-06's auto-move mechanics themselves, only add the revert-on-interrupt transition.

## Related

- UI-06 (`docs/todo/UI-06-analysis-menu-duplicate-repurpose-to-player-assignment.md`)
- UX-06 (WinGraph SingleSide currently reads `enginePlays`; see UI-09 which removes that coupling)
