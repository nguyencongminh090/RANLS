# 2026-08-21 — Test framework choice for TEST-01

## Decision

Chose **doctest** (v2.4.11), vendored as a single header at `tests/vendor/doctest.h`, over Catch2
and GoogleTest, for the new `tests/` unit-test harness (`rapfi-gui-tests` CMake target).

## Rationale

- **Header-only, no system package dependency.** Both doctest and Catch2 satisfy the "single-header,
  no system package" constraint from `docs/instruction/TEST-01-test-infrastructure.md`. GoogleTest
  was ruled out per the todo file's own note (heavier, needs building/linking a separate library,
  not a fit for `build.sh` / `build_msys2.sh`'s current no-extra-dependency model).
- **doctest over Catch2:** doctest's single header is ~7.1k lines vs. Catch2 v3's amalgamated header
  being noticeably larger and Catch2 v3 having moved toward requiring a compiled library for
  reasonable build times (the old fully-header-only Catch2 v2 line is in maintenance mode only).
  doctest explicitly targets fast compile times and minimal binary bloat, which matters here since
  the goal is a lightweight harness for a handful of tests, not an elaborate test suite.
- **Vendored, not `FetchContent`.** The instruction file allows either. Vendoring one ~316KB header
  file directly into the repo (`tests/vendor/doctest.h`, MIT license, from
  `https://github.com/doctest/doctest` tag `v2.4.11`) avoids a network dependency at configure time,
  which matters for reproducible/offline builds and CI reliability. `FetchContent` would otherwise
  require network access during every clean `cmake` configure.

## Architecture finding: no gtkmm-transitive-dependency problem

Per the instruction file's pitfall about verifying `src/model/` and `src/engine/` sources don't drag
in gtkmm transitively: they do not. `grep -rl "gtkmm\|gtk/gtk" src/model src/engine` returns no
matches. `GomocupProtocol` (`src/engine/gomocup_protocol.h`) pulls in `model/game_state.h` (for the
`GameRule` enum) via `i_engine_protocol.h`, and `game_state.h` pulls in `sigc++/sigc++.h`, but neither
that chain nor `move_history.h`/`board_state.h` reach any GTK header. `sigc++-3.0` is resolvable
standalone via `pkg-config` independent of `gtkmm-4.0`. The `rapfi-gui-tests` target links only
`PkgConfig::SIGCXX` (not `PkgConfig::GTKMM`) and builds/runs cleanly with no display server
(`DISPLAY`/`WAYLAND_DISPLAY` unset) — confirmed by `ldd tests/rapfi-gui-tests | grep -i gtk` finding
nothing. No architecture problem to record beyond this positive confirmation — the model/engine
layers are already cleanly separated from the UI layer as `CLAUDE.md`'s module-layering rules
describe.

## Scope

This entry covers the framework/toolchain decision only. The `tests/` target itself, its test cases,
and verification are tracked under `docs/todo/TEST-01-test-infrastructure.md`.
