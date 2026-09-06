# 2026-09-06 — Platform-dependency audit (cross-platform build/runtime review)

## Prompt

User asked whether the source compiles cross-platform or Linux-only, then ran
`/systematic-debugging` ("Check for platform dependency") and supplied a parallel
audit from Gemini for cross-checking. This entry records the merged, verified
findings. No code was modified.

## Decision / conclusion

**RANLS targets Linux first.** [README.md](../../README.md) claims
Windows/macOS/Linux support; that is aspirational — [engine_process.cpp:57](../../src/engine/engine_process.cpp)
already notes "this task targets Linux/GTK4 per project scope".

Verified support matrix:

| Target | Build | Tests | Runtime |
|---|---|---|---|
| Linux (GCC/Clang) | PASS | PASS | PASS — reference |
| Windows MSYS2 (MINGW64/UCRT64) | PASS | PARTIAL | PASS, degraded — MinGW provides `<unistd.h>`/`access`/`X_OK`; `PR_SET_PDEATHSIG` compiled out; settings land in launch dir |
| Windows native MSVC | **FAIL** | FAIL | N/A — `<unistd.h>` missing, `-Wall -Wextra -Wpedantic` rejected, no `WIN32` subsystem |
| macOS (Clang) | PASS | PARTIAL | degraded — settings land in CWD, no PDEATHSIG, no native menu-bar integration |

(Gemini's report marked macOS "Build: FAIL"; corrected here — macOS clang has
`<unistd.h>`, `access`, `X_OK` and accepts the GCC-style warning flags. Its
blockers are runtime degradation, not compile failure.)

## Findings

### Guarded — compile everywhere

- **[engine_process.cpp:6,18,48](../../src/engine/engine_process.cpp)** — `#ifdef __linux__`
  around `<sys/prctl.h>` + `prctl(PR_SET_PDEATHSIG, SIGKILL)` child-setup. `#else`
  falls back to `Gio::Subprocess::create`; a crashed/killed GUI on Win/macOS can
  orphan the engine (relies on stdin-EOF self-exit). Known gap, documented in the
  source and `docs/fix-log/2026-09-05-eng03-*`.
- **[settings_storage.cpp:100](../../src/model/settings_storage.cpp)** — `executableDir()`
  reads `/proc/self/exe` under `#ifdef __linux__`, else `std::filesystem::current_path()`.
  On Win/macOS the settings file (`rapfi-gui.settings`) resolves relative to the
  launch CWD, not the binary — launching from a shortcut/file-manager loses settings.
- **[rdb_container.cpp:13,134](../../src/model/rdb/rdb_container.cpp)** — includes
  `<io.h>` on `_WIN32` else `<unistd.h>`, but the durability call is
  `#if !defined(_WIN32)` → `fsync(fileno(f))` with **no `_commit()` fallback**. On
  Windows `.rdb` atomic saves flush userspace buffers (`fflush`) but never force a
  physical-disk commit before the rename.

### Unguarded — the one concrete compile blocker

- **[settings_dialog.cpp:5,28](../../src/ui/settings_dialog.cpp)** — bare
  `#include <unistd.h>` + `access(path.c_str(), X_OK)`, no `#ifdef`. MinGW/macOS
  provide these; **MSVC does not** (`C1083: unistd.h`; `X_OK` undefined in the
  Windows CRT, whose `_access` supports only exists/read/write). On Windows the
  execute-bit check is also semantically meaningless.

### Toolchain (`CMakeLists.txt`)

- No `if(WIN32)` / `if(APPLE)` branches anywhere.
- [`-Wall -Wextra -Wpedantic`](../../CMakeLists.txt) — GCC/Clang only; MSVC rejects
  `-Wextra -Wpedantic`.
- `add_executable(ranls-gui ...)` has no `WIN32` flag → a `cmd.exe` console window
  opens alongside the GUI on native Windows.
- `pkg_check_modules(gtkmm-4.0)` assumes `pkg-config` on `PATH` (fine on Linux/MSYS2,
  absent in vanilla MSVC/vcpkg).
- `git rev-parse` in `execute_process` is already guarded for no-`.git` tarballs.

### Tests

- 7 suites hardcode POSIX mock engines: `/bin/cat`
  ([test_eng03_close_request.cpp:73](../../tests/test_eng03_close_request.cpp),
  test_anlz05_stop_then_move, test_anlz05_no_automove_action, test_anlz06_search_intent_gate,
  test_anlz07_analyze_restart_convergence, test_proto04_stop_flush_race) and `/bin/true`
  ([test_eng01_engine_state.cpp:99,126](../../tests/test_eng01_engine_state.cpp)).
- [test_eng03_pdeathsig.cpp](../../tests/test_eng03_pdeathsig.cpp) uses `fork`/`pipe`/`kill`/
  `/proc/<pid>/stat` directly (already `#ifdef __linux__`-guarded at the suite level).
- Only runnable under Linux or an MSYS2 shell that provides `/bin/cat`.

### Fragile but not platform-specific

- **[application.cpp:34-38](../../src/application.cpp)** — style.css fallback uses
  `std::filesystem::path(__FILE__).parent_path()`, baking the build machine's source
  path into the binary. Any install where CWD ≠ install dir loses all custom styling
  with a `[RANLS] Warning: style.css not found.`

## Follow-up

After a Gemini advisory pass the remediation was tiered by risk/value and split into
three CODEs (2026-09-06):

- **PORT-01** (P2, Sprint 14 Active) — Tier-1 guarded fixes, ~30 lines, no Linux impact:
  `#if !defined(_WIN32)` around `<unistd.h>` + a Windows executable-extension check
  replacing `access(X_OK)`; `_commit()` alongside `fsync()` in `rdb_container.cpp`;
  `GetModuleFileNameW` / `_NSGetExecutablePath` branches in `executableDir()`.
- **PORT-02** (P3, Backlog) — bundle `style.css` via GResource, drop the `__FILE__`
  fallback that bakes a build-host path into the binary.
- **PORT-03** (P3, Backlog, gated) — native MSVC compiler-flag branch + `WIN32` subsystem,
  a CMake-built `mock_engine` target to replace the `/bin/cat` test stand-ins, and a Win32
  Job Object for engine cleanup. Only pursued if native MSVC / Windows CI becomes a goal.

Product decision 2026-09-06: **keep the portable-app model** — settings stay next to the
binary (`GetModuleFileNameW` just fixes the shortcut-launch bug), not `%APPDATA%`, matching
how Yixin/Rapfi-family tools are distributed.

## Non-goals

- Not committing to native-MSVC or first-class macOS support — that is a product
  decision for the user. This entry documents the gap; PORT-01 scopes the work if it
  is taken up.
