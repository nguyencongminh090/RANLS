# Native MSVC build + Windows CI is a supported goal (PORT-03 un-gated)

**Date:** 2026-09-06
**Type:** Decision (build/toolchain, product scope)
**Relates to:** `docs/audit/2026-09-06-platform-dependency-audit.md`, PORT-01 (shipped Sprint 14),
PORT-02, PORT-03, ENG-03 (the Linux-only `PR_SET_PDEATHSIG` PORT-03 extends).

## Context

The 2026-09-06 platform-dependency audit tiered the portability follow-ups into PORT-01 (Tier-1
guarded fixes, shipped Sprint 14), PORT-02 (GResource asset bundling, Backlog) and PORT-03
(native MSVC compiler-flag branch + `WIN32` subsystem + a portable `mock_engine` test target
replacing the hardcoded `/bin/cat` / `/bin/true` stand-ins + a Win32 Job Object for
engine-subprocess cleanup). PORT-03 was filed to Backlog **explicitly gated**: its detail file's
`Design:` line read "needs a decision on whether native MSVC / Windows CI is actually wanted
before this leaves Backlog", because MSYS2/MinGW already builds and runs and native Visual Studio
support is only worth the CMake and test-harness churn if it is a real target.

## Decision

Native MSVC (or at minimum `clang-cl`) build support **and** a Windows GitHub Actions job are a
supported project goal. PORT-03 is un-gated and eligible to be pulled into a sprint.

Scope stays as written in `docs/todo/PORT-03-msvc-build-and-portable-test-harness.md`:

1. `if(MSVC)` compiler-flag branch (`/W4 /permissive-`) + `WIN32` on `add_executable(ranls-gui …)`
   + a documented vcpkg / gtkmm-on-MSVC discovery path (or `pkg_check_modules` behind `if(NOT MSVC)`).
2. A portable `tests/mock_engine/` CMake target replacing every `/bin/cat` / `/bin/true` stand-in
   across the 7 affected suites; `test_eng03_pdeathsig.cpp` stays Linux-guarded.
3. A `#elif defined(_WIN32)` Win32 Job Object (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`) in
   `engine_process.cpp` so the engine dies with the GUI on Windows.
4. A Windows CI job once 1–3 are in place.

Unchanged by this decision: no first-class macOS UI integration (native menu bar, app bundle) —
that stays a separate future product call; the portable-app settings model stays (2026-09-06).

## Why

- PORT-01's guarded `_WIN32` / `__APPLE__` branches are already in the tree but **cannot be
  compiled or exercised on the Linux build host** — every Sprint 14 close note flags the MSYS2
  smoke as an unverifiable-here human step. A Windows CI job is the structural fix for that gap:
  it turns "a human must remember to build on Windows" into a required status check.
- The `mock_engine` target (PORT-03 scope item 2) removes the `/bin/cat` / `/bin/true` assumption
  that blocks the test suite from running anywhere non-POSIX — value on its own, independent of
  whether anyone ships an MSVC binary.
- The incremental cost is bounded and one-time (a CMake branch + one small executable target + one
  CI YAML job); deferring it just lets the Windows branches rot untested.
