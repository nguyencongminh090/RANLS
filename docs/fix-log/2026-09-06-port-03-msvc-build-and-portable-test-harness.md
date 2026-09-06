# 2026-09-06 — PORT-03: native MSVC build support + portable test harness + Windows engine cleanup

## Prompt

Implement tracked task PORT-03 (`docs/todo/PORT-03-msvc-build-and-portable-test-harness.md`) end to
end on branch `port-03/msvc-build-and-portable-test-harness`: (1) MSVC compiler-flag branch + `WIN32`
subsystem + gtkmm-on-MSVC discovery in `CMakeLists.txt`; (2) a portable `tests/mock_engine/` target
replacing the hardcoded `/bin/cat` / `/bin/true` stand-ins across 7 suites without touching what the
tests assert; (3) a `#elif defined(_WIN32)` Win32 Job Object in `engine_process.cpp` extending
ENG-03's Linux-only `PR_SET_PDEATHSIG` block; (4) optional Windows CI job. Must not regress the Linux
build or the MSYS2 MINGW64 path.

## Action

### 1. `CMakeLists.txt` — MSVC toolchain branch

- gtkmm-4.0 / gio-2.0 discovery split: `if(MSVC)` → `find_package(PkgConfig)` (non-REQUIRED) and, if
  a pkg-config shim is present (vcpkg ships one), the same `pkg_check_modules(... IMPORTED_TARGET)`
  calls; otherwise a `FATAL_ERROR` pointing at the README. `else()` = the existing
  `find_package(PkgConfig REQUIRED)` + `pkg_check_modules` path, byte-for-byte unchanged for
  Linux / MSYS2. `glib-compile-resources` discovery unchanged.
- `add_executable(ranls-gui WIN32 …)` — `WIN32` is ignored on non-Windows, selects the GUI
  subsystem on Windows (no `cmd.exe` console window).
- `target_compile_options`: `if(MSVC) /W4 /permissive- else() -Wall -Wextra -Wpedantic endif()`.
- README gains a "Building on Windows" section (MSYS2 + native-MSVC/vcpkg routes, the Job Object
  note); `build.sh` / `build_msys2.sh` gain one-line pointers to it (Sprint 14 lesson: a new
  toolchain branch updates the build scripts + docs in the same change).

### 2. `tests/mock_engine/` — portable stand-in engines

- One source `mock_engine.cpp`, two CMake targets in `tests/mock_engine/CMakeLists.txt`:
  - `mock_engine` — echoes each stdin line back to stdout, stays alive while stdin is open, exits 0
    on EOF or the sentinel line `END` (a faithful `/bin/cat`; the controller already sends `END` on
    graceful stop, so it also dies promptly there).
  - `mock_engine_quit` — same source built with `-DMOCK_ENGINE_QUIT`, exits 0 immediately (`/bin/true`).
- `tests/CMakeLists.txt`: `add_subdirectory(mock_engine)` and, on **both** test binaries,
  `target_compile_definitions(... MOCK_ENGINE_PATH="$<TARGET_FILE:mock_engine>"
  MOCK_ENGINE_QUIT_PATH="$<TARGET_FILE:mock_engine_quit>")`.
- Migrated (string swap only, no assertion change): `test_eng03_close_request`,
  `test_anlz05_stop_then_move`, `test_anlz05_no_automove_action`, `test_anlz06_search_intent_gate`,
  `test_anlz07_analyze_restart_convergence`, `test_proto04_stop_flush_race` →
  `kFakeEngine = MOCK_ENGINE_PATH`; `test_eng01_engine_state` → `MOCK_ENGINE_QUIT_PATH` (both the
  `EngineConfig` path and the direct `proc.start(...)`). `test_eng03_pdeathsig.cpp` left
  Linux-guarded and untouched.

### 3. `src/engine/engine_process.cpp` — Win32 Job Object

- New `#elif defined(_WIN32)` anonymous-namespace helper: a lazily-created process-wide Job Object
  with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`; `assignToEngineJob()` `OpenProcess`es the spawned
  child by the PID `Gio::Subprocess::get_identifier()` returns and `AssignProcessToJobObject`s it
  right after `Gio::Subprocess::create(...)`. The job handle is intentionally never closed — when
  the GUI process exits for any reason (crash, `taskkill /f`) the OS closes it and terminates every
  assigned engine.
- The `#ifdef __linux__` `PR_SET_PDEATHSIG` launcher path is unchanged. The final `#else` (macOS /
  other) keeps the plain `Gio::Subprocess::create` fallback with a comment that there is no clean
  equivalent.

### 4. `src/ui/bottom_panel.cpp` — pre-existing UAF fix (surfaced by item 2)

Switching ENG-03's stand-in from `/bin/cat` to the build-tree `mock_engine` absolute path made an
engine-log line long enough to change the Engine Log's scroll geometry, which turned a latent bug
**deterministic**: `BottomPanel::scrollEngineLogToBottom()` / `scrollMoveLogToEnd()` /
`flushPending()` scheduled `Glib::signal_idle().connect_once([this]{…})` slots capturing a bare
`this`; if the `BottomPanel` was destroyed before the idle ran (window torn down mid-stream — every
`MainWindow` test does this), the idle later dereferenced the freed widget inside
`Gtk::TextView::scroll_to` → SIGSEGV. This is the same defect behind the "ui12 scroll" intermittent
crash. Fix: wrap each of the four idle slots in `sigc::track_obj(…, *this)` so they auto-disconnect
with the widget (`BottomPanel` is a `sigc::trackable` via `Glib::ObjectBase`). No behaviour change
when the panel outlives the idle (the normal case).

### Regression tests

- The mock_engine migration itself is the harness regression guard (the ENG-03 → UI-10 ordering
  reproduced the crash deterministically pre-fix; green post-fix).
- New `TEST_CASE("PORT-03: a pending scroll idle does not outlive its BottomPanel")` in
  `tests/test_ui10_engine_log_scroll_target.cpp`: queues scroll idles on both logs, destroys the
  owning window, pumps the main loop — must not crash.

## Verification (Linux tier — this host has no Windows toolchain and no CI display)

- Clean `cmake -S . -B build && cmake --build build -j` from scratch: `ranls-gui`, `mock_engine`,
  `mock_engine_quit` all build and link; no new warnings (only the 3 pre-existing
  `-Wunused-function` in `gomocup_protocol.cpp`).
- `ctest`: `port02-style-css-bundled` PASS, `ranls-gui-tests` PASS (209/209 cases, 2466
  assertions — includes ENG-01 / ANLZ-05 / ANLZ-06 / PROTO-04 on the mock engine),
  `rel02-version-single-source` PASS, `ranls-gui-ui-tests` 29/30 cases — the single failure
  `test_anlz05_no_automove_action` (ANLZ-05 wire race) reproduces byte-identically on the
  pre-PORT-03 baseline (`670d0dc`); 0 skipped, no crash (deterministic SIGSEGV before the
  `bottom_panel.cpp` fix).
- `grep -rn "/bin/cat\|/bin/true" tests/*.cpp` — comments only, no code.

## Pending — human Windows/MSVC smoke test (cannot run here)

- Native MSVC (or clang-cl) compiles `ranls-gui`; no `cmd.exe` console window on GUI launch.
- `taskkill /f` on the GUI process leaves no orphaned engine subprocess (Job Object).
- MSYS2 MINGW64 rebuild still green (unchanged CMake `else()` path — expected clean).

## Out of scope / deferred

- Windows GitHub Actions job (scope item 4, explicitly optional) — deferred as follow-up.
- First-class macOS UI integration — per the todo's scope boundary.
