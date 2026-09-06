# PORT-03 — Native MSVC build support + portable test harness + Windows engine cleanup

**Status:** 🔲 OPEN (Active — Sprint 15)
**Area:** `CMakeLists.txt`, `tests/CMakeLists.txt`, ~8 `tests/test_*.cpp`, `src/engine/engine_process.cpp`, new `tests/mock_engine/`
**Priority:** P3 — MSYS2 MINGW64 already builds and runs; this adds the native MSVC toolchain + a portable test harness so a Windows CI job is possible
**Source:** Platform-dependency audit 2026-09-06 — see [docs/audit/2026-09-06-platform-dependency-audit.md](../audit/2026-09-06-platform-dependency-audit.md). Split from PORT-01 (Tier 3) per the 2026-09-06 tiering decision.
**Design:** none — gate resolved 2026-09-06: native MSVC + Windows CI **is** a supported goal (user decision, see [docs/audit/2026-09-06-native-msvc-windows-ci-goal.md](../audit/2026-09-06-native-msvc-windows-ci-goal.md)). Scope below stands as written; scoped directly.
**Depends on / relates to:** PORT-01 (Tier-1 guards land first), PORT-02; ENG-03 (the Linux-only PDEATHSIG this extends); audit 2026-09-06.

## Problem

Native MSVC cannot compile the project and the test suite cannot run outside a POSIX / MSYS2
environment:

1. **`CMakeLists.txt`** — `target_compile_options(... -Wall -Wextra -Wpedantic)` are GCC/Clang
   flags; MSVC rejects `-Wextra -Wpedantic`. `add_executable(ranls-gui ...)` has no `WIN32`
   flag, so a native Windows build opens a `cmd.exe` console window alongside the GUI.
   `pkg_check_modules(GTKMM ... gtkmm-4.0)` assumes `pkg-config` on `PATH` (absent in a
   vanilla MSVC / vcpkg setup).
2. **`tests/`** — 7 suites hardcode `/bin/cat` / `/bin/true` as stand-in engines
   (`kFakeEngine = "/bin/cat"` in test_eng03_close_request, test_anlz05_stop_then_move,
   test_anlz05_no_automove_action, test_anlz06_search_intent_gate,
   test_anlz07_analyze_restart_convergence, test_proto04_stop_flush_race; `/bin/true` in
   test_eng01_engine_state). `test_eng03_pdeathsig.cpp` uses `fork`/`pipe`/`kill`/`/proc`
   directly (already `#ifdef __linux__`-guarded at the suite level).
3. **`engine_process.cpp`** — the `#ifdef __linux__` `PR_SET_PDEATHSIG(SIGKILL)` child-death
   guard has no Windows/macOS equivalent, so a crashed / force-killed GUI orphans the engine
   subprocess on those platforms (ENG-03 documented this as out of scope).

## Scope (in order)

1. `CMakeLists.txt` — `if(MSVC) target_compile_options(ranls-gui PRIVATE /W4 /permissive-)
   else() ... -Wall -Wextra -Wpedantic endif()`. Add `WIN32` to `add_executable(ranls-gui ...)`
   (guarded or unconditional — `WIN32` is ignored on non-Windows). Document the vcpkg /
   gtkmm-on-MSVC discovery path (or gate `pkg_check_modules` behind `if(NOT MSVC)` with a
   `find_package` fallback).
2. New `tests/mock_engine/` — a tiny portable C++ executable built as its own CMake target
   (reads stdin, echoes / stays alive, exits on a sentinel line). Replace every
   `kFakeEngine = "/bin/cat"` / `"/bin/true"` with the built target's path (passed in via a
   compile definition or a test fixture). Keep `test_eng03_pdeathsig.cpp` Linux-guarded.
3. `engine_process.cpp` — under `#elif defined(_WIN32)` create a Win32 Job Object with
   `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` and assign the spawned process to it, so the engine
   dies with the GUI. (macOS has no clean equivalent — leave the `Gio::Subprocess` fallback,
   document it.)
4. Optional: a Windows job in GitHub Actions once 1–3 are in place.

## Acceptance criteria

- Native MSVC (or at least `clang-cl`) compiles `ranls-gui`; no console window on a Windows
  GUI launch.
- `ctest` passes on Linux with the new `mock_engine` target replacing `/bin/*` — no
  regression.
- Killing the GUI process on Windows (`taskkill /f`) leaves no orphaned engine process.

## Scope boundary

- Do not regress the Linux build or the MSYS2 MINGW64 path.
- `mock_engine` replaces the `/bin/*` stand-ins only — do not rewrite what the tests assert.
- Not pursuing first-class macOS UI integration (native menu bar, app bundle) here — that is
  a separate product decision.
