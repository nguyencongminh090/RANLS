# REL-02 — version-string-single-source

## Approach

Single literal in `CMakeLists.txt` `project(VERSION)`. `configure_file` a `version.h.in` →
`version.h` in the build tree; add that dir to the target's include path. `APP_VERSION` as a
`constexpr` string / `#define` from `@PROJECT_VERSION@`.

`--version` must be handled at the very top of `main()`, before any `Gtk::Application` /
`gtk_init` — scan `argv` for `--version`/`-v`, print, `return 0`. Check the existing `main()` first
to see how argv is currently consumed (if at all).

## Pitfalls

- `Gtk::AboutDialog::set_version()` currently takes `"2.0"` — a plain string swap, but verify the
  About dialog isn't also setting a program name/version elsewhere.
- Don't conflate with `game_io.cpp` `kFormatVersion` / `yxgame_version` — different concept
  (save-file schema), leave it alone.
- CMake `PROJECT_VERSION` vs `rapfi-gui_VERSION` vs `CMAKE_PROJECT_VERSION` — use `PROJECT_VERSION`
  from within this project's scope.
- Test must not link gtkmm — assert the CLI `--version` output against the CMake version via ctest
  or a `scripts/` check, in the gtkmm-free test target.

## Boundaries — do not touch

- No arg-parsing dependency. No behavior change beyond `--version` and the About string.
- Don't pick the version number here — it comes from REL-01 / the user.
