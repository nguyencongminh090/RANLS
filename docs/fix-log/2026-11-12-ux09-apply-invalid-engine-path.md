# Settings Apply disabled while the engine path is invalid (2026-11-12)

**Status:** Active
**Related:** UX-09

## Prompt

I18N-06 visual pass: with a non-existent engine path, the Settings "Apply" button stays disabled on every tab, so a first-run user cannot switch Language/Theme (the only explanation is on the Engine tab). Decision 2026-11-12 (user): option (a) — Apply always available; while the path is invalid it applies everything else and keeps the previously saved engine path.

## Root cause

`SettingsDialog::updateApplySensitivity()` set `btnApply_` sensitive iff `enginePathValid_`, and `onApply()` early-returned when `!enginePathValid_`, so one invalid field on the Engine tab blocked the whole dialog.

## Fix

`src/ui/settings_dialog.{h,cpp}`: Apply is always sensitive; while the path is invalid its tooltip (`i18n::tr`) says the path will not be saved and other settings will be applied (cleared when valid). `onApply()` has no early return and uses the entry text for `enginePath` only when valid, otherwise keeps `baseEngineConfig_.enginePath`. Test seam: `friend struct RanlsUx09Probe`. `src/resources/lang/vi.tsv`: +tooltip string (draft, for user review).
Left alone: engine launch guards, settings file format, `main_window.cpp` (`EngineController::sendConfig` read: it calls `generateStart` then returns when `!isUsable()`, so the base-path-unchanged `sendConfig()` is harmless with no running engine), other tabs/controls.

## Verification

Regression test `tests/test_ux09_apply_invalid_engine_path.cpp` (ranls-gui-ui-tests, 3 cases). Pre-fix output (unchanged code):
```
test_ux09_apply_invalid_engine_path.cpp:61: ERROR: CHECK( probe.applySensitive() ) is NOT correct!  values: CHECK( false )
test_ux09_apply_invalid_engine_path.cpp:62: ERROR: CHECK_FALSE( probe.applyTooltip().empty() ) is NOT correct!
test_ux09_apply_invalid_engine_path.cpp:67: FATAL ERROR: REQUIRE( got.count == 1 ) is NOT correct!  values: REQUIRE( 0 == 1 )
(same three for the "valid saved path, invalid entry" case; tooltip CHECK also failed in the third case)
[doctest] test cases: 3 | 0 passed | 3 failed
```
Post-fix: `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY RUN_TESTS=1 ./build.sh` -> ctest 5/5; ranls-gui-tests 310/310 cases (5111 assertions); ranls-gui-ui-tests 142/142 cases (968 assertions), 0 skipped; catalog lint green (first run caught a malformed vi.tsv row, fixed).

Live check (Xvfb, x11, `rapfi-gui.settings` with `engine_path=/nonexistent`, `language=en`; file removed afterwards): Settings > UI > Tiếng Việt > Apply -> UI turned Vietnamese; settings file afterwards `engine_path=/nonexistent`, `language=vi`. Screenshots in /tmp/claude-1000/-run-media-ngmint-Data-Programming-Programming-C---ModifyCode-Rapfi-modif-June4-YixinBoard/54bd63ae-cdf5-4e5c-9f47-10a8cb3c44dd/scratchpad/: ux09-4b-apply-tooltip.png (tooltip on Apply, Engine tab), ux09-5-before-apply.png (UI tab, Tiếng Việt chosen, Apply enabled), ux09-6-after-apply.png (Vietnamese UI).

## Known limitation

The engine path cannot be cleared through the dialog: `""` is invalid, so the saved path is kept (same as before, when Apply was blocked entirely).
