# NAV-01 — `undoAll`/`redoAll` floods the engine and rebuilds the whole UI per move

**Status:** ✅ DONE

`undoMove()`/`redoMove()` were each split into a position-mutation-only `undoMoveSilent()`/
`redoMoveSilent()` (no `clearDatabase()`, no `signal_board_changed`) plus the original public
method, which now calls the silent half and then does the clear/reset/emit exactly as before —
so single-step navigation is unchanged. `undoAll()`/`redoAll()`/`gotoMove()` now loop the silent
halves and do exactly one `clearDatabase()` + `resetAnalysisState()` + `invalidateEvalHistoryCache()`
+ `signal_board_changed.emit()` for the whole bulk operation (skipped entirely if nothing moved).
`gotoPath()` already did a single clear+emit at the end of its loop (it rebuilds the board fresh
from a path rather than looping undo/redo), so it needed no change — verified by inspection and a
new regression test.

`signal_move_selected` is now emitted, once, at the end of `gotoMove()` with the history index
actually landed on (not the requested index, in case it was out of range) — including when the
move didn't change the position, so a UI click on the already-current move still gets acknowledged.
It is intentionally NOT emitted by `undoMove`/`redoMove`/`undoAll`/`redoAll`/`gotoPath` — those are
stepping/path operations, not the index-based "jump to move" pick it exists for.

### Verification

- Clean build: `bash build.sh <dir>` — 0 warnings/errors in the touched files (pre-existing unused-
  function warnings in `gomocup_protocol.cpp` are unrelated).
- `ctest --output-on-failure` — 100% tests passed (1/1 suite; `rapfi-gui-tests` internally runs all
  doctest cases).
- `tests/rapfi-gui-tests -tc="*NAV-01*" -s` — 6/6 new test cases, 496/496 assertions passed:
  - `undoAll` on a 120-move game: `signal_board_changed`/`signal_database_updated` fire exactly once.
  - `redoAll` on a 120-move game: same, exactly once.
  - `undoAll`/`redoAll` on an empty/fully-redone game: zero emissions (no-op guarded).
  - `gotoMove` jumping 110 plies back then 90 plies forward: `signal_board_changed`/
    `signal_database_updated`/`signal_move_selected` each fire exactly once per `gotoMove` call,
    `signal_move_selected` carries the landed-on index.
  - `gotoMove` to the already-current index: `signal_move_selected` fires, `signal_board_changed`
    does not (no position change).
  - `gotoPath` on a 100-move path: `signal_board_changed`/`signal_database_updated` fire exactly
    once.
- Read through `src/main_window.cpp`'s connected slots for `signal_board_changed` (rebuilds move
  log, queries database, refreshes `BoardViewModel`), `signal_database_updated` (refreshes
  `BoardViewModel` + redraw), and `signal_move_selected` (refreshes `BoardViewModel` + redraw) —
  none assume per-ply invocation; all just re-derive full state from the current `GameState`, so
  batching the emission count doesn't change correctness, only how often the (already O(n)/O(n²))
  rebuild runs.
- Not separately verified against a live engine subprocess (no engine binary available in this
  environment) — the Engine Log "single `yxquerydatabaseallt` block" criterion is covered
  indirectly by the `databaseUpdatedCount == 1` assertions, since `controller_.queryDatabase()` is
  invoked once per `signal_board_changed` emission in `main_window.cpp`.

### Note

This fix was recovered from an interrupted prior session (uncommitted diff in a sibling worktree,
`worktree-agent-a5f39658049ed0b01`, never committed to that branch). The diff was reapplied
file-by-file, checked against this document's acceptance criteria, and found complete — no
additional code changes were needed beyond what was recovered.
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
