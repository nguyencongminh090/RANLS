# 2026-08-21 — Settings dialog silently resets multiPV and wipes customParams (STATE-02)

**Status:** ✅ FIXED

## Summary

`SettingsDialog::onApply` (`src/ui/settings_dialog.cpp`) built a brand-new, default-constructed
`EngineConfig eConfig;` / `ViewConfig vConfig;` and assigned only the fields the dialog exposed
controls for. Any `EngineConfig`/`ViewConfig` member without a control silently reverted to its
struct default on every Apply: `EngineConfig::multiPV` (only settable via the `!analyze n` /
`!set multipv` console commands) back to `1`, `EngineConfig::customParams` (populated from unknown
engine `INFO` keys) wiped to empty, and `ViewConfig::showDatabase` back to `true`. `MainWindow`
persists whatever `onApply` emits straight to disk via `SettingsStorage::save`
(`src/main_window.cpp`), so the loss was permanent, not just for the session.

## Fix

- `SettingsDialog` now stores the config it was constructed with as `baseEngineConfig_` /
  `baseViewConfig_` members (`src/ui/settings_dialog.h`, set in the constructor init list).
- `onApply()` starts each output struct as a copy of the matching base member (`EngineConfig
  eConfig = baseEngineConfig_;` / `ViewConfig vConfig = baseViewConfig_;`) instead of default
  constructing, then overwrites only the fields its own controls own. Any field without a control —
  `customParams`, `showDatabase`, and any future field added to either struct — is preserved by
  default instead of dropped by default.
- Added a `Gtk::SpinButton spinMultiPV_` (range 1–20, matching the board-size spin-button pattern at
  `src/main_window.cpp:509-510`) to the Engine Setting frame, next to Max Nodes, and wired it into
  `onApply` (`eConfig.multiPV = static_cast<int>(spinMultiPV_.get_value());`).
- `SettingsStorage::save`/`load` (`src/model/settings_storage.cpp`) previously never wrote
  `customParams` to disk at all — a separate, pre-existing gap. Extended both: `save` writes one
  `custom_param.<key>=<value>` line per entry (value escaped like other string fields); `load` scans
  loaded key/value pairs for that prefix and rebuilds the map. This was necessary for the round-trip
  regression test below to be satisfiable at all — without it, `customParams` would still vanish
  across a settings-file reload regardless of the `onApply` fix.

## Files changed

- `src/ui/settings_dialog.h` — added `baseEngineConfig_`/`baseViewConfig_` members, `spinMultiPV_`.
- `src/ui/settings_dialog.cpp` — constructor stores base configs; added the multiPV spin button;
  `onApply` copies from base configs instead of default-constructing.
- `src/model/settings_storage.cpp` — `save`/`load` now round-trip `EngineConfig::customParams`.
- `tests/CMakeLists.txt` — added `test_settings_storage.cpp` and
  `src/model/settings_storage.cpp` to the test target.
- `tests/test_settings_storage.cpp` (new) — 2 regression tests.

## Verification

- `bash build.sh` (from a clean `build_cmd/`) — builds `rapfi-gui` and `rapfi-gui-tests` cleanly,
  including the modified `settings_dialog.cpp`/`settings_storage.cpp`. No new warnings from the
  touched files (pre-existing unused-function warnings in `gomocup_protocol.cpp` are unrelated).
- `RUN_TESTS=1 bash build.sh` → ctest: `100% tests passed, 0 tests failed out of 1`.
- Ran the test binary directly with `--test-case="SettingsStorage*" --success`: both new cases pass
  with real assertions (`multiPV == 8`, `customParams.at("some_unknown_info_key") == "some_value"`,
  and the missing-file case returns struct defaults). Full suite: `50 | 50 passed | 0 failed | 0
  skipped`, `203 | 203 passed | 0 failed` — no regressions versus the pre-existing 48 test cases.
- Read the diff in `onApply` directly to confirm the mechanism (not just the test): both output
  structs are now copy-initialized from `baseEngineConfig_`/`baseViewConfig_` rather than default
  constructed; `customParams` and `showDatabase` have no corresponding assignment anywhere in the
  function, so they pass through unchanged from the base copy.

## Left out of scope (per the todo's scope boundary)

- Engine-path validation / inline error feedback on Apply — that's UX-02, untouched here.
- No dialog layout redesign beyond the one new `spinMultiPV_` row in the existing Engine Setting
  grid.
- `SettingsDialog::onApply` itself is gtkmm UI code; `tests/CMakeLists.txt` deliberately excludes
  gtkmm from the test target (see `docs/audit/2026-08-21-test-framework-choice.md`), so it isn't
  unit-testable with the current infrastructure. The regression test instead pins the
  `SettingsStorage` persistence contract the fix's round-trip guarantee depends on, and the
  `onApply` merge mechanism itself was verified by direct source reading (see Verification above).
- `ViewConfig::showDatabase` was audited and confirmed preserved by the same mechanism, but was not
  given disk persistence in `SettingsStorage` (unlike `customParams`) since the acceptance criteria
  only required a round-trip test for `multiPV` + a custom param, not `showDatabase`.

## Detail

Full task record: `docs/todo/STATE-02-settings-dialog-drops-config-fields.md`.
