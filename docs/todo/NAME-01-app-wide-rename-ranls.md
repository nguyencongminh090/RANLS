# NAME-01 — Consistent app-wide rename "Rapfi Analysis" → RANLS

**Status:** ✅ DONE

Implemented 2026-09-03 on branch `name-01/app-wide-rename-ranls`. Renamed
`"Rapfi Analysis"` → `RANLS` everywhere it still leaked: window title (new
`kAppDisplayName` constant in `main_window.h`), `style.css` header comment,
GTK application id (`com.rapfi.analysis` → `com.ranls.gui`) + the `[Rapfi GUI]`
stderr warning prefix (→ `[RANLS]`). Full build-target rename `rapfi-gui` →
`ranls-gui` (+ `-tests` / `-ui-tests`, `RAPFI_GUI_BUILD_TESTS` →
`RANLS_GUI_BUILD_TESTS`, `RAPFI_GUI_BIN` → `RANLS_GUI_BIN`) across
`CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/check_version.cmake`,
`build.sh`, `build_msys2.sh`, `README.md`. CMake `DESCRIPTION` updated. The
`rapfi-gui.settings` filename is deliberately left (not app-id derived —
renaming it would orphan user settings). Engine binary names untouched.

**Regression test:** `tests/test_name01_window_title.cpp` — constructs a real
`MainWindow` headless in the `ranls-gui-ui-tests` target and asserts
`get_title() == "RANLS"` (and `!= "Rapfi Analysis"`), plus a constant check.

**Verification:** clean `rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
&& cmake --build build -j` — succeeds, produces `build/ranls-gui` (only the 3
pre-existing `-Wunused-function` warnings). `ctest --test-dir build` 3/3:
`ranls-gui-tests` 141 cases / 1112 assertions; `rel02-version-single-source`
pass; `ranls-gui-ui-tests` 16 cases / 88 assertions (+2 NAME-01, MainWindow
case ran with display). `./build/ranls-gui --version` → `0.1.1`.
`grep -rn "Rapfi Analysis" src/` and
`grep -rn "rapfi-gui" CMakeLists.txt tests/ build.sh build_msys2.sh README.md`
both clean.
**Area:** app identity / branding
**Priority:** P3
**Source:** split out of UI-11 (about-window-rewrite) per user decision 2026-08-31

## Problem

The app's name is **RANLS**, but `"Rapfi Analysis"` is still hard-coded in
several places. UI-11 corrected only the About dialog's own displayed text
(explicitly scoped that way at dispatch); the rest is deferred here.

## Scope

- `src/main_window.cpp:114` — `set_title("Rapfi Analysis")`.
- `src/resources/style.css` — header comment `Rapfi Analysis GUI — Adaptive Theme`.
- GTK application id (`src/application.cpp` / `src/main.cpp` — currently
  `org.rapfi.gui` or similar) — changing it affects settings/storage keys and
  the WM identity, so check `SettingsStorage` paths before changing.
- CMake `project(rapfi-gui …)` / `DESCRIPTION` — decide whether the build
  target is renamed too (ripples into `tests/`, packaging scripts, install
  rules).
- A future `.desktop` file, if/when packaging adds one.

## Scope boundary

- Not the About dialog (done in UI-11).
- The engine binary names (`pbrain-rapfi`, etc.) are unrelated — do not touch.

## Acceptance criteria

- No user-visible `"Rapfi Analysis"` string remains.
- Settings/storage continue to load (app-id change handled or avoided).
- Full suite green.

## Related

- UI-11 — renamed the About dialog text only.
