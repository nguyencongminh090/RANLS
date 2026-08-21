# PROTO-02 — Hardcoded board size 15 breaks every non-15×15 board

**Status:** ✅ DONE — see [fix-log detail](../fix-log/2026-08-21-proto-02-hardcoded-board-size-15.md)
for the full change list and verification notes (clean build; full suite 65/65 test cases, 260/260
assertions pass, including 5 new PROTO-02 regression cases at 5×5/15×15/22×22).
**Area:** protocol parsing + engine status display + board rendering
**Priority:** P1
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

The board supports sizes 5–22 (`src/main_window.cpp:491-492`, `MAX_BOARD_SIZE = 22` in
`src/model/board_state.h:7`), but three places assume 15.

### 1. Coordinate parsing — wrong moves, not just wrong labels

`parseEngineCoord` (`src/engine/gomocup_protocol.cpp:43-53`):

```cpp
// In Yixin flipY_X mode, A1 is bottom-left.
// 15 is the board size.
return Coord{col, 15 - rowNumber};
```

For any engine replying in `A1` notation on a board that is not 15×15, this yields the wrong row —
silently, with no error. The function has no access to `boardSize_` because it is a file-static
helper; the class member is right there at the call site.

This is the serious one: it corrupts move data, not just presentation.

### 2. Engine status "Best:" label

`EngineStatusView::update` (`src/ui/engine_status.cpp:122`):

```cpp
valueBest_.set_text(coordStr(bestMove, 15));
```

`coordStr` computes the row label as `boardSize - c.y` (`src/ui/engine_status.cpp:29`), so on a
non-15 board the displayed best move is labelled wrongly. `PVView` and `TreeExplorer` have the same
helper but are correctly passed the real board size (`src/ui/pv_view.cpp:75`,
`src/ui/tree_explorer.cpp:91`) — `EngineStatusView` is simply never given it.

### 3. Star points only drawn at 15×15

`src/ui/board_renderer.cpp:117` — `if (bs == 15)`. Cosmetic, but it means every other board size
loses its positional reference points entirely.

## Related risk to check while here

`GomocupProtocol::boardSize_` is only updated by `generateStart`
(`src/engine/gomocup_protocol.cpp:200-203`), which is reached from `EngineController::sendConfig`
(`src/engine/engine_controller.cpp:109`) — and `sendConfig` returns early when the engine is not
started (`src/engine/engine_controller.cpp:105`). So changing board size while the engine is stopped
leaves the parser's `boardSize_` stale until the next successful `sendConfig`. Verify and fix as
part of this item.

## Acceptance criteria

- No literal `15` remains as a board-size assumption anywhere in `src/`.
- `parseEngineCoord` takes the board size as a parameter (or becomes a member function).
- `EngineStatusView` receives the real board size — it already gets a `PVLine` vector in `update()`,
  so plumbing the size through is a signature change, not an architectural one.
- Star points are computed from board size rather than hardcoded, or omitted cleanly for sizes with
  no conventional star-point layout.
- `GomocupProtocol::boardSize_` is never stale relative to `GameState::boardSize()`.
- Manually verified at 5×5, 15×15, and 22×22: stone placement, coordinate labels, `Best:` readout,
  PV move labels.

## Scope boundary

- Layout/legibility at extreme board sizes (stone size, label overlap, click-target size) is UX-03 —
  this item is about correctness of the numbers, not the ergonomics.

## Related

- PROTO-01 (parser hardening, same file), UX-03 (board-size ergonomics)
