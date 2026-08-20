# TEST-01 — No test infrastructure exists; several P0 fixes need it

**Status:** open
**Area:** build / test harness
**Priority:** P1 (blocker for STATE-01 and PROTO-01)
**Source:** codebase review, 2026-08-21

## Problem

The repo has no tests and no test target. `CMakeLists.txt` builds only the application; there is no
`tests/` directory, no test framework dependency, and CodeGraph reports "no covering tests found"
for every symbol queried during the review.

`/CLAUDE.md` ("Bug-fix workflow") requires a regression test for any fix where the affected code has
or can reasonably get coverage, and requires saying so explicitly when it can't. Two P0 items are
currently blocked on this:

- **PROTO-01** — hostile-input parser hardening. `GomocupProtocol` is pure string-in / signal-out
  with no GTK widget dependency; feeding it malformed lines and asserting no crash is the single
  cheapest, highest-value test in this codebase.
- **STATE-01** — analysis-data lifetime. `GameState`, `MoveHistory`, `BoardState`, and
  `VariationTree` are all `src/model/` classes with no widget dependency; asserting
  "makeMove → undo → `pvLines()` is empty" is a few lines.

## What is testable today without touching GTK

| Target | Why it's clean to test |
|---|---|
| `src/engine/gomocup_protocol.cpp` | string in, `sigc` signals out; no widgets |
| `src/model/board_state.cpp` | pure grid logic including `checkWin` |
| `src/model/move_history.cpp` | pure cursor logic — note `moveCount()` vs `totalMoves()` vs `currentIndex()` is exactly the kind of off-by-one worth pinning down |
| `src/model/variation_tree.cpp` | pure tree ops |
| `src/model/game_state.cpp` | needs `sigc` only |
| `src/model/settings_storage.cpp` | round-trip save/load — directly relevant to STATE-02 |

`sigc++` is already a dependency, so signal-emission assertions need nothing new.

## Acceptance criteria

- A `tests/` target wired into CMake, runnable via `ctest` (or a documented single command), that
  does **not** require a display server.
- A test framework chosen and recorded (Catch2 and doctest are both single-header and add no real
  build burden; GoogleTest is heavier — pick one and note why in `docs/audit.md`).
- At least one meaningful test per target listed above, so the harness is proven rather than empty.
- `build.sh` / `build_msys2.sh` updated, or a documented separate test invocation.
- README or `docs/README.md` says how to run the tests.

## Scope boundary

- Do **not** attempt GTK widget tests — the value here is the model and protocol layers, which are
  already free of widget dependencies. Widget testing is a much larger question and not needed for
  any currently-filed item.
- This item builds the harness; the actual regression tests for specific bugs belong to those bugs'
  items.

## Related

- Blocks: STATE-01, PROTO-01. Also benefits STATE-02, STATE-03, UI-01, PROTO-02.
