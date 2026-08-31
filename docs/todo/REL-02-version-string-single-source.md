# REL-02 — Single-source version string (CMake → About dialog + --version)

**Status:** ✅ DONE

Implemented 2026-08-31. Single literal is `project(rapfi-gui VERSION 0.1.0)` in the top-level
`CMakeLists.txt` (adopts REL-01's agreed `0.1.0`). `configure_file(src/version.h.in →
build/generated/version.h, @ONLY)` emits `#define APP_VERSION "@PROJECT_VERSION@"`;
`build/generated` added to `rapfi-gui`'s include path. `src/main.cpp` scans `argv` for
`--version`/`-v` at the very top of `main()`, before `RapfiApplication::create()` / any GTK/Gio
init, prints `APP_VERSION` and `return 0`. `onAbout()` now calls `set_version(APP_VERSION)` (was
`"2.0"`); `set_program_name`/`set_comments` unchanged, no other version source in that method.
No arg-parsing dependency added. `game_io.cpp` `kFormatVersion`/`yxgame_version` left untouched.

Regression test: `tests/check_version.cmake` + ctest case `rel02-version-single-source` (added in
`tests/CMakeLists.txt`) — runs the real `rapfi-gui` binary headless with `--version` and `-v` and
asserts stdout == CMake `PROJECT_VERSION`. Script-based, links no gtkmm. **Permanent regression
guard — do not delete.**

Verification (all run 2026-08-31):
- `rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4` — clean.
- `cd build && ctest --output-on-failure` — 3/3 passed (`rapfi-gui-tests`,
  `rel02-version-single-source`, `rapfi-gui-ui-tests`).
- `env -u DISPLAY -u WAYLAND_DISPLAY ./build/rapfi-gui --version` → `0.1.0`, rc=0, no window;
  same for `-v`.
- `build/CMakeCache.txt`: `CMAKE_PROJECT_VERSION:STATIC=0.1.0`; generated `version.h` has
  `#define APP_VERSION "0.1.0"` — CMake version, `--version`, and About dialog all agree.
**Area:** release/versioning
**Priority:** P3
**Source:** `docs/notes/2026-08-30-versioning-and-changelog.md` → `features/versioning-and-changelog/`
**Depends on:** REL-01 (agreed starting version number), planning.md Q2–Q3

## Context

The app's version is stated in three places that already disagree:
- `CMakeLists.txt:3` — `project(rapfi-gui VERSION 1.0.0)`
- `src/main_window.cpp:858` — `dialog->set_version("2.0")` (hardcoded in `onAbout()`)
- git — no tags at all

Hard constraint (`features/versioning-and-changelog/user_story.md`): exactly one source of truth.

## Scope

- Set `CMakeLists.txt` `project(... VERSION ...)` to the `0.x` version agreed in REL-01, and keep
  it the only literal.
- `configure_file` a generated `version.h` exposing `APP_VERSION` (string). Location/pattern per
  planning.md Q2 — pick the minimal approach; no generated-file precedent exists yet.
- `onAbout()`: replace `set_version("2.0")` with `set_version(APP_VERSION)`.
- Add a `--version` (and `-v`) handler in `main()` that prints `APP_VERSION` and exits **before**
  GTK is initialized (hard constraint). Confirm current argv handling first (planning.md Q3).
- Regression test: assert the `--version` output equals the CMake `PROJECT_VERSION` (a small
  ctest / script check; the CLI path is testable without gtkmm).

## Scope boundary

- Do not add an argument-parsing library — an early `argv` scan is enough for one/two flags.
- Do not touch `game_io.cpp`'s `yxgame_version` / `kFormatVersion` — that is the save-file format
  version, unrelated to the app version.

## Acceptance criteria

- Building at version `X` makes `rapfi-gui --version`, the About dialog, and `PROJECT_VERSION` all
  report `X`, with `X` written once.
- `--version` works with no display / no window.
- New test asserts CLI version == CMake version; full suite still green.

## Related

- REL-01 — decides the version number this task adopts.
