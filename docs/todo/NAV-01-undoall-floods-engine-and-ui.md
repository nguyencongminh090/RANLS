# NAV-01 — `undoAll`/`redoAll` floods the engine and rebuilds the whole UI per move

**Status:** open
**Area:** game navigation / signal fan-out
**Priority:** P1
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

`undoAll` and `redoAll` are implemented as loops over the single-step operations
(`src/model/game_state.cpp:122-130`):

```cpp
void GameState::undoAll()  { while (undoMove()) {} }
void GameState::redoAll()  { while (redoMove()) {} }
```

Each `undoMove` emits `signal_board_changed` (`src/model/game_state.cpp:94`) and calls
`clearDatabase`, which emits `signal_database_updated` (`src/model/game_state.cpp:243-246`). So one
click on **⏮** in a 100-move game produces, per move:

| Work | Site |
|---|---|
| A `yxquerydatabaseallt` command sent to the engine | `src/main_window.cpp:287-289` |
| `bottomPanel_.clear()` + full move-log string rebuilt from scratch | `src/main_window.cpp:293-304` |
| `BoardViewModel::update()` — full `O(n²)` board scan | `src/main_window.cpp:283` |
| Both tree views fully rebuilt | `src/ui/analysis_panel.cpp:88-95` |
| Win graph `setData` + full `evalHistory()` tree walk | `src/ui/analysis_panel.cpp:89-91` |
| An extra redraw from the separate `signal_database_updated` emit | `src/main_window.cpp:344-347` |

×100. The database queries in particular are sent to the engine as fast as the loop runs, and 99 of
the 100 results are for positions the user is not stopping at.

`gotoMove` (`src/model/game_state.cpp:132-146`) has the same shape — it calls `clearDatabase` and
then loops `undoMove`/`redoMove`, each of which calls `clearDatabase` again.

## Related defect: a dead signal

`signal_move_selected` is declared (`src/model/game_state.h:85`) and connected
(`src/main_window.cpp:308-311`) but **never emitted anywhere**. `gotoMove` — the one operation it
obviously exists for — does not emit it. Either wire it up as part of the batching design or delete
it; leaving a connected-but-never-fired signal invites the assumption that jump-to-move is already
notified.

## Acceptance criteria

- Bulk navigation (`undoAll`, `redoAll`, `gotoMove`, `gotoPath`) emits **one** board-changed
  notification for the whole operation, not one per ply.
- Exactly one database query is issued for the final position.
- Move log is not rebuilt from scratch per ply.
- `signal_move_selected` is either emitted meaningfully or removed.
- Verified on a ≥100-move game: **⏮** feels instant and the Engine Log shows a single
  `yxquerydatabaseallt` block.

## Scope boundary

- Per-update cost of the individual widgets is RT-01/RT-04; this item is about **how many times**
  they are asked to update during navigation.
- Do not change undo/redo *semantics* (what the cursor does) — only the notification batching.

## Related

- RT-01, RT-04 (per-update cost), RT-02 (log append cost)
