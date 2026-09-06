# 2026-09-06 — PORT-01: Tier-1 Windows portability fixes (guarded, isolated)

## Prompt

Implement tracked task PORT-01 (`docs/todo/PORT-01-cross-platform-build-and-runtime.md`), the
Tier-1 remediation from the 2026-09-06 platform-dependency audit
(`docs/audit/2026-09-06-platform-dependency-audit.md`): three isolated `#ifdef` guards scoped
directly into the three affected files — **no** shared `platform.h` / abstraction layer — closing
the one hard non-MinGW-Windows compile blocker and the Windows `.rdb` data-durability gap, and
fixing the settings-file-follows-CWD bug on Windows/macOS. No behaviour change on any Linux path.

## Action

### 1. `src/ui/settings_dialog.cpp` / `.h` — `<unistd.h>` guard + Windows exec-extension check

- `#include <unistd.h>` is now `#if !defined(_WIN32)`.
- New pure helper `settings_dialog_detail::hasExecutableExtension(const std::string&)` declared in
  `settings_dialog.h`, defined in the `.cpp`: lowercases `std::filesystem::path(path).extension()`
  and compares against `.exe` / `.bat` / `.cmd` / `.com`.
- In `isValidEnginePath` the final check is now:
  - POSIX (`#else`): `access(path.c_str(), X_OK) != 0` → "Not executable" — **unchanged, byte for byte**.
  - `#if defined(_WIN32)`: `!hasExecutableExtension(path)` → "Not an executable (.exe / .bat / .cmd / .com)".
  - The preceding `exists` + `is_regular_file` checks are untouched and still run on both.

### 2. `src/model/rdb/rdb_container.cpp` — `_commit` alongside `fsync`

The existing `#if !defined(_WIN32)` block calling `::fsync(::fileno(f))` is untouched. A new
`#if defined(_WIN32)` block immediately after it calls `::_commit(::_fileno(f))` with identical
control flow (`!= 0` → `fclose` + `remove(tmp)` + `setError("_commit failed")` + `return false`).
`<io.h>` was already included under `_WIN32`.

### 3. `src/model/settings_storage.cpp` — `executableDir()` real-binary resolution

Include block changed from `#ifdef __linux__ <unistd.h>` to an `#if/#elif` chain adding
`<windows.h>` (`_WIN32`) and `<cstdint>` + `<cstring>` + `<mach-o/dyld.h>` (`__APPLE__`).
`executableDir()`:
- `#if defined(__linux__)` — `read_symlink("/proc/self/exe")` → `parent_path()` — **unchanged**.
- `#elif defined(_WIN32)` — `GetModuleFileNameW` into a `MAX_PATH` `std::wstring`, growing the
  buffer (doubling, capped at 64 KiB) while the call reports truncation
  (`GetLastError() == ERROR_INSUFFICIENT_BUFFER`); on success `resize(len)` then
  `std::filesystem::path(buf).parent_path()`. `len == 0` breaks to the fallback.
- `#elif defined(__APPLE__)` — `_NSGetExecutablePath(nullptr, &size)` to learn the required size,
  then a second call into a `std::string(size, '\0')`; on `0` return, `resize(strlen)` then
  `parent_path()`.
- `return std::filesystem::current_path();` fallback for every other platform, and for any failure
  in the Windows/Apple branches — **unchanged**.

Portable-app model kept: settings still resolve next to the binary, **not** `%APPDATA%` /
`g_get_user_config_dir()` (user decision 2026-09-06).

### Regression test

`tests/test_port01_engine_path_check.cpp` (added to `ranls-gui-ui-tests`, the only target that
compiles `settings_dialog.cpp`). 3 cases / 20 assertions against `hasExecutableExtension` — pure
string logic, so it runs and passes on the Linux host even though the `_WIN32` call site does not
compile here: accepted extensions (incl. Windows and POSIX-style full paths), case-insensitivity,
and rejection of no-extension binaries, look-alike suffixes (`.exe.txt`, `.executable`,
`.command`), `.dll`, `.sh`, and a bare trailing dot.

## Per-guard correctness reasoning (targets not compilable on this Linux host)

**`settings_dialog.cpp` — `#if !defined(_WIN32)` around `<unistd.h>` + `access`/`X_OK`.**
MSVC ships no `<unistd.h>` (`C1083`), and the Windows CRT's `_access` / `_waccess` accept only
mode `0` (exists), `2` (write), `4` (read), `6` (read+write) — there is no execute mode and `X_OK`
is undefined. Guarding the include and replacing the call on `_WIN32` removes both errors. The
replacement is semantically appropriate: NTFS has no POSIX execute bit, and Windows decides
"runnable" by extension (PATHEXT). `std::filesystem::path::extension()` is portable and returns the
substring from the last dot of the filename (empty if none, or if the name ends in a dot), so
`"engine."` and `"engine"` both correctly fail. Lowercasing via `std::tolower` with an
`unsigned char` cast is defined behaviour. MinGW/UCRT (which *does* have `<unistd.h>`) takes the
same `_WIN32` branch — deliberate: extension is the right check on Windows regardless of toolchain,
and it matches the audit's stated intent.

**`rdb_container.cpp` — `#if defined(_WIN32)` → `::_commit(::_fileno(f))`.**
`_fileno(FILE*)` (in `<stdio.h>`, always available) returns the `int` file descriptor;
`_commit(int)` (declared in `<io.h>`, already included) is MSVC/MinGW's documented `fsync`
equivalent — it flushes the OS buffers for that fd to disk and returns `0` on success, `-1` on
failure setting `errno` (`EBADF` / `EIO`), exactly the contract the surrounding code already
assumes for `fsync`. So reusing the identical `!= 0` → cleanup → `return false` path is correct.
Placing it as a separate `#if defined(_WIN32)` block after the untouched `#if !defined(_WIN32)`
block means exactly one of the two compiles on any platform; the intervening `fflush` (userspace
→ OS) already ran, so `_commit` (OS → platter) closes the same durability chain `fsync` does
before the `rename`.

**`settings_storage.cpp` — `GetModuleFileNameW` (`_WIN32`).**
`GetModuleFileNameW(NULL, buf, n)` writes the full path of the current process image, returns the
number of `wchar_t` written **excluding** the NUL, and — since Windows XP — when the buffer is too
small it fills it completely, returns `n`, and sets `GetLastError() == ERROR_INSUFFICIENT_BUFFER`.
The loop therefore treats `len == n` **or** that error code as "truncated, grow and retry", and
only accepts a result when `len < n` and the error is not the truncation sentinel (guarding the
XP-era ambiguity where `len == n` with no error also meant "exactly filled"). `SetLastError(0)`
before each call prevents a stale error from a previous API call being misread. `len == 0` is a
genuine failure (fall through to `current_path()`). `std::filesystem::path` has a `const wchar_t*` /
`std::wstring` constructor on Windows (native encoding is UTF-16), so no manual narrowing is needed.
`<windows.h>` provides the function, `MAX_PATH`, `DWORD`, and `ERROR_INSUFFICIENT_BUFFER`.

**`settings_storage.cpp` — `_NSGetExecutablePath` (`__APPLE__`).**
Declared in `<mach-o/dyld.h>`. Contract (Apple docs): called with a buffer, it copies the path and
returns `0`, or returns `-1` and sets `*bufsize` to the required size **including** the trailing
NUL if the buffer is too small. Calling it first with `size == 0` (buffer ignored) thus loads
`size` with the exact allocation needed; the second call into `std::string(size, '\0')` then
succeeds with `0`. The returned path may be non-canonical (symlinks, `..`, `./`) but
`parent_path()` + later `std::filesystem` use tolerate that, and the audit only requires "next to
the binary", not a realpath. `resize(strlen(c_str()))` trims the string's length down to the
actual C-string content so no embedded NUL leaks into the `path`. `<cstdint>` for `std::uint32_t`,
`<cstring>` for `std::strlen`.

## Summary

3 source files + 1 header changed, 1 test file added, `tests/CMakeLists.txt` updated. Linux:
clean build, no new warnings; `ranls-gui-tests` 209/209 (2466 assertions), `rel02-version` pass,
`ranls-gui-ui-tests` 28/29 — the lone failure (`test_anlz05_no_automove_action`, an engine-
subprocess timing flake) reproduces identically on a clean `main` (`c90b0fd`) checkout, so the
Linux ctest result is unchanged. The `_WIN32` / `__APPLE__` branches are not compiled or run here;
their correctness is argued per-guard above and the MSYS2 MINGW64 smoke test (build + launch +
save a `.rdb` + pick an engine path) remains a required human step before PORT-01 ships.
Out of scope, untouched as required: the `#ifdef __linux__` `PR_SET_PDEATHSIG` block in
`engine_process.cpp` (PORT-03), GResource / `style.css` bundling (PORT-02), `CMakeLists.txt`
compiler-flag / `WIN32`-subsystem work (PORT-03), and any new settings location.
