# 2026-09-03 — NAME-01: app-wide rename "Rapfi Analysis" → RANLS

## Prompt

Tracked task NAME-01 (Sprint 9): finish the `"Rapfi Analysis" → RANLS` rename
everywhere UI-11 left out of scope, including the build-target rename (user
confirmed full scope).

## Action

- **Window title** — `src/main_window.cpp` `set_title("Rapfi Analysis")` →
  `set_title(kAppDisplayName)`; new `inline constexpr const char *kAppDisplayName
  = "RANLS"` in `src/main_window.h` as the single WM-identity string.
- **Stylesheet** — `src/resources/style.css` header comment `Rapfi Analysis GUI`
  → `RANLS GUI`.
- **GTK application id** — `src/application.cpp` `"com.rapfi.analysis"` →
  `"com.ranls.gui"`; stderr warning `[Rapfi GUI]` → `[RANLS]`. WM-identity only —
  `settings_storage.cpp` derives its path from `executableDir()`, not the app-id,
  so no settings/storage path moved. `rapfi-gui.settings` filename left untouched
  (renaming it would orphan existing user settings; not user-visible).
- **Build target** — `rapfi-gui` → `ranls-gui` (+ `ranls-gui-tests`,
  `ranls-gui-ui-tests`), `RAPFI_GUI_BUILD_TESTS` → `RANLS_GUI_BUILD_TESTS`,
  `-DRAPFI_GUI_BIN=` → `-DRANLS_GUI_BIN=`; touched `CMakeLists.txt`
  (`project()`, `DESCRIPTION "RANLS — Renju/Gomoku Analysis GUI"`,
  `add_executable`/`target_*`/`install`/POST_BUILD/option), `tests/CMakeLists.txt`,
  `tests/check_version.cmake`, `build.sh`, `build_msys2.sh`, `README.md`, and a
  stale comment in `tests/test_ui07_pv_view_rows.cpp`.
- **Regression test** — `tests/test_name01_window_title.cpp` in the
  `ranls-gui-ui-tests` (gtkmm) target: constructs a real `MainWindow` headless
  and asserts `get_title() == "RANLS"` / `!= "Rapfi Analysis"`, plus a
  `kAppDisplayName` constant check. Wired `src/main_window.cpp` + remaining deps
  (`command/*`, `ui/board_view`, `ui/board_renderer`, `ui/settings_dialog`,
  `model/board_view_model`, `model/game_io`, `model/settings_storage`) into that
  target.
- Historical/append-only tracking docs (`docs/fix-log/**`, `docs/audit/**`,
  `docs/sprint/archive/**`, other `docs/todo/*`, `docs/instruction/*`,
  `features/**`, `.claude/skills/**`) deliberately left with the old target name.
- About dialog (`src/ui/about_dialog.*`) untouched (done in UI-11); the negative
  assertion in `tests/test_ui11_about_dialog.cpp:67` left as-is (still valid).
- Engine binary names (`pbrain-rapfi`, …) untouched — unrelated.

## Summary

Clean `rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug &&
cmake --build build -j` succeeds, producing `build/ranls-gui` (only the 3
pre-existing `-Wunused-function` warnings in `gomocup_protocol.cpp`).
`ctest --test-dir build --output-on-failure` — 3/3:
- `ranls-gui-tests` — 141 cases / 1112 assertions
- `rel02-version-single-source` — pass
- `ranls-gui-ui-tests` — 16 cases / 88 assertions (+2 NAME-01; the real-MainWindow
  case ran, display available on the build host)

`./build/ranls-gui --version` → `0.1.1` (REL-02 guard intact).
`grep -rn "Rapfi Analysis" src/` → empty.
`grep -rn "rapfi-gui" CMakeLists.txt tests/ build.sh build_msys2.sh README.md` → empty.
