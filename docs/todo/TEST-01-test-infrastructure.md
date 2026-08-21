# TEST-01 — No test infrastructure exists; several P0 fixes need it

**Status:** ✅ DONE
**Area:** build / test harness
**Priority:** P1 (blocker for STATE-01 and PROTO-01)
**Source:** codebase review, 2026-08-21

## Summary

Added a `tests/` CMake target (`rapfi-gui-tests`) using vendored doctest v2.4.11
(`tests/vendor/doctest.h`), linking only `src/model/move_history.cpp`, `src/model/board_state.cpp`,
and `src/engine/gomocup_protocol.cpp` plus `sigc++-3.0` — no gtkmm/GTK, no display server required.
Framework choice + a confirmed absence of gtkmm-transitive dependencies in `src/model`/`src/engine`
is recorded in `docs/audit.md` → `docs/audit/2026-08-21-test-framework-choice.md`.

Wrote 8 `MoveHistory` test cases pinning the `moveCount()`/`totalMoves()`/`currentIndex()` invariant
across clear/add/undo/redo/truncate-on-redo, and 15 `GomocupProtocol` hostile-input test cases
(`parseLine`) covering empty lines, out-of-range and negative coordinates, truncated/garbage
`MESSAGE`/`INFO`/`DATABASE` payloads, unterminated parenthesis blocks, a 10k-char garbage line, and
a "still works after hostile input" recovery check.

`build.sh` / `build_msys2.sh` gained an opt-in `RUN_TESTS=1` env var that runs `ctest` after the
build (default behavior unchanged — plain `./build.sh` still only builds the app). `README.md` gained
a "Running the tests" section with both the manual `cmake`/`ctest` invocation and the `RUN_TESTS=1`
shortcut.

## Verification

Ran from a clean build directory (`build_test_verify`, deleted after verification), with
`DISPLAY`/`WAYLAND_DISPLAY` unset to simulate a headless environment:

- `cmake -DCMAKE_BUILD_TYPE=Release ..` — configured successfully (found `gtkmm-4.0` for the main
  app, `sigc++-3.0` for the tests target).
- `cmake --build . --target rapfi-gui-tests` — built successfully; `ldd tests/rapfi-gui-tests | grep
  -i gtk` returned nothing (no GTK linked).
- `env -u DISPLAY -u WAYLAND_DISPLAY ./tests/rapfi-gui-tests` →
  `[doctest] test cases: 23 | 23 passed | 0 failed | 0 skipped` /
  `[doctest] assertions: 69 | 69 passed | 0 failed` / `Status: SUCCESS!`
- `env -u DISPLAY -u WAYLAND_DISPLAY ctest --output-on-failure` → `1/1 Test #1: rapfi-gui-tests
  ... Passed` / `100% tests passed, 0 tests failed out of 1`.
- `cmake --build . --target rapfi-gui` — main application still builds successfully (unrelated
  pre-existing unused-function warnings in `gomocup_protocol.cpp`, not introduced by this change).
- Re-ran the same sequence through `RUN_TESTS=1 bash build.sh build_test_verify2` (also deleted
  after verification) end-to-end: app + tests both built, `ctest` passed.

## Out of scope (left for other items)

- No tests written for `board_state.cpp` (beyond linking it for `Coord`), `variation_tree.cpp`,
  `game_state.cpp`, or `settings_storage.cpp` — these are optional "also testable" targets per this
  item's own scope boundary, not required here. STATE-01 and future items should add coverage there.
- No `doctest_discover_tests`-style per-`TEST_CASE` ctest registration — the whole binary is
  registered as a single `ctest` case, which is sufficient for this harness's current size.
- No gtkmm-transitive-dependency problem was found (see audit entry) — nothing to work around.

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
