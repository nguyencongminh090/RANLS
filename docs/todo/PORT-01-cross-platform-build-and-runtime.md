# PORT-01 — Tier-1 Windows fixes (guarded, isolated, ~30 lines)

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/ui/settings_dialog.cpp`, `src/model/rdb/rdb_container.cpp`, `src/model/settings_storage.cpp`
**Priority:** P2 — small, self-contained, closes a real data-durability gap + the one hard MSVC compile blocker
**Source:** Platform-dependency audit 2026-09-06 (`/systematic-debugging` + cross-check against a Gemini report) — see [docs/audit/2026-09-06-platform-dependency-audit.md](../audit/2026-09-06-platform-dependency-audit.md). Product decision 2026-09-06: keep the portable-app model (settings next to the binary, not `%APPDATA%`); do the Tier-1 quick fixes now; defer GResource bundling (PORT-02) and MSVC/CI harness work (PORT-03).
**Design:** none — three isolated `#ifdef` guards, scoped directly.
**Depends on / relates to:** PORT-02 (style.css / GResource), PORT-03 (MSVC flags + test harness); audit 2026-09-06.

## Problem

The code builds and runs cleanly on Linux and under an MSYS2 MINGW64/UCRT64 shell
(`build_msys2.sh`). Three small, independent platform gaps are worth closing now regardless
of whether native MSVC or first-class macOS is ever pursued:

1. **`settings_dialog.cpp:5,28` — hard compile blocker for any non-MinGW Windows toolchain.**
   Bare `#include <unistd.h>` + `access(path.c_str(), X_OK)`. MSVC has no `<unistd.h>`;
   `X_OK` is undefined in the Windows CRT (its `_access` supports only exists/read/write).
   On Windows the execute-bit check is also semantically meaningless.
2. **`rdb_container.cpp:134` — Windows data-durability gap.** The atomic-save path is
   `.tmp` → `fflush` → `fsync(fileno(f))` → `rename`, but the `fsync` call is
   `#if !defined(_WIN32)` with **no** `_commit(_fileno(f))` fallback. On Windows a `.rdb`
   save is not forced to physical storage before the rename — a crash / power loss between
   the rename and the OS flushing its cache can leave a zero-length or torn `game.rdb`.
3. **`settings_storage.cpp:100` — `executableDir()`** resolves via `/proc/self/exe` only on
   Linux; elsewhere it falls back to `std::filesystem::current_path()`, so
   `rapfi-gui.settings` is read/written relative to the launch CWD. Launching from a desktop
   shortcut or file manager on Windows/macOS silently uses a different settings file.

## Scope (in order)

1. **`settings_dialog.cpp`** — guard `#include <unistd.h>` with `#if !defined(_WIN32)`. In
   `isValidEnginePath`, keep `access(path.c_str(), X_OK)` on POSIX; on `_WIN32` replace it
   with existence + `std::filesystem::is_regular_file` (already checked) + an executable-
   extension check (`.exe` / `.bat` / `.cmd` / `.com`, case-insensitive).
2. **`rdb_container.cpp`** — under `#if defined(_WIN32)` call `::_commit(::_fileno(f))` where
   the POSIX branch calls `::fsync(::fileno(f))`, same error handling (`_commit` returns 0 on
   success, -1 on failure). `<io.h>` is already included on `_WIN32`.
3. **`settings_storage.cpp`** — in `executableDir()` add a `#elif defined(_WIN32)` branch
   using `GetModuleFileNameW` (buffer-grow loop, `<windows.h>`) → `parent_path()`; add a
   `#elif defined(__APPLE__)` branch using `_NSGetExecutablePath` (`<mach-o/dyld.h>`). Keep
   the `current_path()` fallback for anything else. **Do not** switch to `g_get_user_config_dir()`
   — the portable-app model stays (user decision 2026-09-06).

## Acceptance criteria

- Linux build/test/runtime unchanged — same `ctest` result, no new warnings.
- The new `_WIN32` / `__APPLE__` branches compile under those toolchains (verify in an MSYS2
  MINGW64 shell at minimum; MSVC compile-check if available — this cannot be verified on the
  Linux build host, note it in the fix-log).
- Manual reasoning recorded in the fix-log for why each guard is correct, since CI can't
  exercise the Windows paths here.

## Scope boundary

- **Guards only.** No behaviour change on Linux. No new settings location, no GResource, no
  CMake changes — those are PORT-02 / PORT-03.
- Do not touch the `#ifdef __linux__` `PR_SET_PDEATHSIG` block in `engine_process.cpp` (the
  Windows engine-orphan gap is PORT-03).
- MSYS2 smoke-test (build + launch + save a `.rdb` + pick an engine path) required before
  marking DONE — a human step, documented, not claimed from the Linux host.
