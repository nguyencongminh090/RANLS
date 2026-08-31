# 2026-08-31 — UI-11: rewrite the About window (custom layout, build info, correct app name)

## Prompt

Tracked task UI-11 (Sprint 8): the Help → About dialog was a bare `Gtk::AboutDialog`
setting only `set_program_name("Rapfi Analysis")`, `set_version(APP_VERSION)`,
`set_comments(...)`. Three problems: wrong app name (it is **RANLS**), almost no
information, stock layout. Replace with a deliberate custom dialog.

## Action

### New `AboutDialog` class — `src/ui/about_dialog.{h,cpp}`

- Subclasses `Gtk::Window`. Constructor takes `Gtk::Window &parent`, sets
  `set_transient_for(parent)` + `set_modal(true)` + `set_resizable(true)`.
- Layout: horizontal root `Gtk::Box` —
  - **Left:** `Gtk::Image` from the themed stock icon `applications-games-symbolic`
    at pixel size 96 (no bespoke logo asset ships; the task boundary forbids
    committing a new binary, so the stock icon is the deliberate fallback).
  - **Right:** vertical `Gtk::Box` — `RANLS` heading (`.title-1` built-in style
    class), `Version <APP_VERSION>` (`.dim-label`, selectable), a tagline, then
    three titled sub-sections:
    - **Developer:** `Developer: Nguyen Minh` + `Copyright © 2026 Nguyen Minh`.
    - **Tech / build info:** a `Gtk::Grid` of dimmed key / value rows — GTK
      (`GTK_*_VERSION`), gtkmm (`GTKMM_*_VERSION`), Cairo (`cairo_version_string()`),
      Build date (`APP_BUILD_DATE`), Commit (`APP_GIT_COMMIT`), License
      (`BSD-style — see LICENSE.md`).
    - **Links & protocol:** repository URL and the Gomocup / Yixin protocol
      reference as `Gtk::Label` with `set_markup` + `<a href>` anchors; a
      plain supported-engine line (Rapfi, Yixin, any Gomocup/Yixin-compatible).
  - Single bottom-right `Close` button (`.suggested-action`); an
    `EventControllerKey` also closes on Esc. Both call `set_visible(false)` so
    the CLEAN-01 `signal_hide → delete` fires.
- No colours hard-coded — only the built-in `.title-1` / `.dim-label` style
  classes, so light and dark themes both render correctly.
- Self-contained: no `GameState` / engine dependency, purely static metadata.

### `MainWindow::onAbout()` — `src/main_window.cpp`

Reduced to the CLEAN-01 lifetime only: `new AboutDialog(*this)`,
`set_hide_on_close(true)`, `signal_hide().connect([dialog]{ delete dialog; })`,
`set_visible(true)`. The `#include "version.h"` there (only used by the old
`set_version`) was replaced with `#include "ui/about_dialog.h"`.

### Build-time metadata — `src/build_info.h.in` (new)

Mirrors `src/version.h.in`. `configure_file` → `build/generated/build_info.h`:
- `APP_BUILD_DATE` from `string(TIMESTAMP APP_BUILD_DATE "%Y-%m-%d" UTC)`.
- `APP_GIT_COMMIT` from `execute_process(COMMAND git rev-parse --short HEAD …
  ERROR_QUIET RESULT_VARIABLE _git_commit_rc)`, falling back to `"unknown"` when
  git is unavailable / the rev-parse fails / output is empty (no-`.git` tarball).

`APP_VERSION` is untouched — still the single CMake `project(VERSION)` literal
(REL-02). No version literal anywhere in the new code.

### CMake wiring

- `src/ui/about_dialog.cpp` added to `GUI_SOURCES` (`rapfi-gui`).
- `src/ui/about_dialog.cpp` + `tests/test_ui11_about_dialog.cpp` added to
  `rapfi-gui-ui-tests`; `${CMAKE_BINARY_DIR}/generated` added to that target's
  include dirs so it resolves `version.h` / `build_info.h`.

### Regression test — `tests/test_ui11_about_dialog.cpp` (new, `rapfi-gui-ui-tests`)

Three real-widget cases (self-skip with no display server, like the other
ui-tests):
1. Builds an `AboutDialog`, walks the `GtkLabel` subtree, asserts it contains
   `RANLS`, `Developer: Nguyen Minh`, `APP_VERSION` (guards REL-02 — no
   hard-coded version), the build-info keys, the repository link and the
   supported-engine line; asserts `"Rapfi Analysis"` is **gone**.
2. Asserts `get_modal()` and `get_transient_for() == &parent`.
3. Instantiates the dialog under both `Adwaita` and `Adwaita-dark`
   (`Gtk::Settings::gtk_theme_name`) — must not crash / assert.

## Verification (2026-08-31)

- `rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4`
  — clean, no new warnings.
- `cd build && ctest --output-on-failure` — **3/3 passed**:
  `rapfi-gui-tests`, `rel02-version-single-source`, `rapfi-gui-ui-tests`.
- `rapfi-gui-ui-tests` ran 13 cases / 81 assertions (was 11 / 70); the UI-11
  cases executed for real (a display server was available in the worktree),
  not self-skipped.
- `build/generated/build_info.h` → `APP_BUILD_DATE "2026-08-31"`,
  `APP_GIT_COMMIT "00ba2cb"`; `version.h` → `APP_VERSION "0.1.0"`.
- Manual light/dark **visual** check of the running GTK app (open Help → About,
  click the links) left for the orchestrator via the `run` skill.

## Scope / boundaries

- App-wide `"Rapfi Analysis" → "RANLS"` rename (window title, `style.css`, GTK
  application id) **deliberately not done** — split out as NAME-01 per the
  dispatch decision. `RANLS` appears only inside the About dialog's own text.
- No new binary logo asset committed — themed stock icon fallback.
- No arg-parsing / templating dependency added.
