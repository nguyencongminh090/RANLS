# 2026-08-31 — REL-02: single-source the version string (CMake → About + --version)

## Prompt

Tracked task REL-02. The app version was stated in three disagreeing places: `CMakeLists.txt`
`project(... VERSION 1.0.0)`, `src/main_window.cpp` `onAbout()` `set_version("2.0")`, and git had
no tags. Hard constraint (`features/versioning-and-changelog/user_story.md`): exactly one source of
truth. REL-01 (done) agreed the number `0.1.0` (`CHANGELOG.md [0.1.0]`, tag `v0.1.0`).

## Action

- `CMakeLists.txt`: `project(rapfi-gui VERSION 1.0.0 …)` → `VERSION 0.1.0` — the only version
  literal in the tree now.
- New `src/version.h.in` (`#define APP_VERSION "@PROJECT_VERSION@"`); `configure_file(… @ONLY)`
  into `${CMAKE_CURRENT_BINARY_DIR}/generated/version.h`; `build/generated` added to `rapfi-gui`'s
  `target_include_directories`. Minimal approach — no generated-file precedent existed.
- `src/main.cpp`: argv scan for `--version` / `-v` at the very top of `main()`, before
  `RapfiApplication::create()` and any GTK/Gio init — prints `APP_VERSION`, `return 0`. No
  arg-parsing library (scope boundary).
- `src/main_window.cpp` `onAbout()`: `set_version("2.0")` → `set_version(APP_VERSION)`; added
  `#include "version.h"`. `set_program_name("Rapfi Analysis")` / `set_comments(…)` unchanged; no
  other version setter in the method.
- Regression test: `tests/check_version.cmake` + ctest case `rel02-version-single-source` in
  `tests/CMakeLists.txt` — runs the built `rapfi-gui` binary headless (`--version` and `-v`,
  `DISPLAY`/`WAYLAND_DISPLAY` unset) and asserts the output equals CMake `PROJECT_VERSION`.
  Script-based; links no gtkmm, keeping the `rapfi-gui-tests` no-gtkmm invariant intact.

Deliberately untouched: `src/model/game_io.cpp` `kFormatVersion` / `yxgame_version` (save-file
schema version, unrelated concept).

## Verification

- `rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug` — configure clean; cache shows
  `CMAKE_PROJECT_VERSION:STATIC=0.1.0`.
- `cmake --build build -j4` — builds `rapfi-gui`, `rapfi-gui-tests`, `rapfi-gui-ui-tests` clean
  (`-Wall -Wextra -Wpedantic`, no warnings from the changed files).
- `cd build && ctest --output-on-failure` — 3/3 passed:
  `rapfi-gui-tests`, `rel02-version-single-source`, `rapfi-gui-ui-tests`.
- `env -u DISPLAY -u WAYLAND_DISPLAY ./build/rapfi-gui --version` → `0.1.0`, exit 0, no window;
  identical for `-v`.
- Generated `build/generated/version.h` contains `#define APP_VERSION "0.1.0"` — CMake
  `PROJECT_VERSION`, the `--version` CLI output, and the About dialog string all agree, written once.
