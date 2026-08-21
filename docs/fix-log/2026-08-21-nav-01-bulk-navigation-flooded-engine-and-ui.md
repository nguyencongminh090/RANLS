# 2026-08-21 — Bulk navigation flooded the engine and rebuilt the whole UI per ply (NAV-01)

## Summary

`GameState::undoAll()`/`redoAll()`/`gotoMove()` were loops over the single-step `undoMove()`/
`redoMove()`, and each single step independently called `clearDatabase()` (→
`signal_database_updated`) and emitted `signal_board_changed`. One click on the "jump to start"
control in a 100+ move game therefore drove, per undone ply: an engine `yxquerydatabaseallt`
query, a full move-log rebuild, a full `BoardViewModel::update()` scan, full tree-view rebuilds,
and a win-graph recompute — all ×N for a single user-visible bulk operation.

Separately, `signal_move_selected` was declared on `GameState` and connected in
`src/main_window.cpp`, but nothing ever emitted it — a dead signal.

## Fix

`src/model/game_state.h`/`.cpp`: split `undoMove()`/`redoMove()`'s position-mutation logic out
into new private `undoMoveSilent()`/`redoMoveSilent()` methods that walk history/board/tree by one
ply but do *not* call `clearDatabase()` or emit `signal_board_changed`. The public `undoMove()`/
`redoMove()` now call the silent half and then do the clear/reset/emit exactly as before (no
behavior change for single-step navigation). `undoAll()`, `redoAll()`, and `gotoMove()` now loop
the silent halves and perform exactly one `clearDatabase()` + `resetAnalysisState()` +
`invalidateEvalHistoryCache()` + `signal_board_changed.emit()` for the whole bulk call — skipped
entirely if the position didn't actually move (e.g. `undoAll()` on a fresh game). `gotoPath()`
already rebuilt the board from scratch in a single pass with one clear+emit at the end, so it
needed no change.

`gotoMove()` now also emits `signal_move_selected` once, at the end, with the history index it
actually landed on (which may differ from the requested index if it was out of range) — including
when the position didn't change, so a UI click on the already-current move is still acknowledged.
It remains unemitted by the stepping/path operations (`undoMove`/`redoMove`/`undoAll`/`redoAll`/
`gotoPath`), which aren't the index-based "jump to move" pick this signal represents.

## Tests

`tests/test_game_state.cpp` — 6 new `TEST_CASE`s built around a `makeGameWithMoves(count)` helper
(120-move and 100-move games): `undoAll`/`redoAll` each fire `signal_board_changed` and
`signal_database_updated` exactly once regardless of move count; both are no-ops (zero emissions)
on an empty/fully-redone game; `gotoMove` jumping 110 plies back then 90 forward fires
`signal_board_changed`/`signal_database_updated`/`signal_move_selected` exactly once each, with
`signal_move_selected` carrying the landed-on index; `gotoMove` to the already-current index fires
only `signal_move_selected`; `gotoPath` on a 100-move path fires `signal_board_changed`/
`signal_database_updated` exactly once.

## Verification

- `bash build.sh <scratch-dir>` — clean build, no new warnings.
- `ctest --output-on-failure` — 100% passed (1/1 suite).
- `tests/rapfi-gui-tests -tc="*NAV-01*" -s` — 6/6 cases, 496/496 assertions passed.
- Read `src/main_window.cpp`'s slots on `signal_board_changed`/`signal_database_updated`/
  `signal_move_selected` — all re-derive full UI state from current `GameState` rather than
  assuming per-ply invocation, so reducing emission count is safe.
- Not verified against a live engine subprocess in this environment; the "single
  `yxquerydatabaseallt` block" criterion is covered indirectly via `databaseUpdatedCount == 1`,
  since `main_window.cpp` issues one `controller_.queryDatabase()` per `signal_board_changed`.

## Note

Recovered from an interrupted prior agent session: the code+test changes existed as an uncommitted
diff in a sibling worktree (`worktree-agent-a5f39658049ed0b01`) that was never committed. Reapplied
file-by-file in this worktree, checked against `docs/todo/NAV-01-undoall-floods-engine-and-ui.md`'s
acceptance criteria, and found complete as recovered — no further code changes were required.
