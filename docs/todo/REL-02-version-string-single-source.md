# REL-02 — Single-source version string (CMake → About dialog + --version)

**Status:** Backlog
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
