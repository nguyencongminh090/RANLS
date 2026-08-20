# TEST-01 — execution guidance

## Approach

Keep it small enough that it lands in one sitting and is proven by real tests, not scaffolding.

1. Pick a header-only framework — doctest or Catch2. Vendor it or fetch it via CMake
   `FetchContent`; do not add a system package dependency that complicates `build.sh` /
   `build_msys2.sh`.
2. Add a `tests/` target linking only what each test needs. Critically: **the model and protocol
   sources must link without GTK widgets.** Verify this early — if `src/model/` or
   `src/engine/` turns out to drag in `gtkmm` headers transitively, that is itself an architecture
   finding worth recording in `docs/audit.md` before working around it.
3. Write the first tests against `MoveHistory` and `GomocupProtocol` — the two smallest, purest
   targets. Prove the harness, then stop. Broad coverage is not this item's job.

## Why these targets first

`MoveHistory` (`src/model/move_history.cpp`, 46 lines) has three overlapping accessors —
`moveCount()` returns `cursor_`, `totalMoves()` returns `moves_.size()`, `currentIndex()` returns
`cursor_ - 1` (`src/model/move_history.h:33-39`). Callers mix them: `GameState::lastMove` uses
`moveCount()` as a guard but `currentIndex()` as the subscript
(`src/model/game_state.cpp:181-187`); `currentPath` uses `moveCount()`
(`:189-196`); `BoardViewModel` uses `moveCount()` (`src/model/board_view_model.cpp:33`).

Review did **not** find an actual off-by-one here — the current combinations are correct. That is
exactly why it's worth pinning with tests: the invariant is load-bearing, non-obvious, and one edit
away from breaking silently.

`GomocupProtocol` is the trust boundary and needs hostile-input tests for PROTO-01 regardless.

## Pitfalls

- Tests must not need a display server — no `Gtk::Application`, no widget construction. If a test
  needs `Glib::MainLoop` for signal timing, that is fine and does not require a display.
- Do not test through the UI. Every currently-filed bug is reachable from the model/protocol layer.
- Do not let this item grow into "write tests for everything." It builds the harness and proves it.
  Bug-specific regression tests belong to their own items (STATE-01, PROTO-01, …).
- Record the framework choice and rationale in `docs/audit.md` per `/CLAUDE.md` — it is a
  build/toolchain decision, which that file exists for.

## Do not touch

- Application source behaviour. If a class turns out to be untestable without a change, note it and
  raise it — do not refactor production code opportunistically under a test-infrastructure item.
