# Instruction — UX-09: Settings Apply disabled while the engine path is invalid

Todo: [`docs/todo/UX-09-apply-disabled-when-engine-path-invalid.md`](../todo/UX-09-apply-disabled-when-engine-path-invalid.md)
Decision: option **(a)** — `docs/audit/2026-11-log.md` (2026-11-12 "UX-09").

## Approach

Behaviour change in the `ui` layer, in `src/ui/settings_dialog.{h,cpp}` only. Treat it as a bug fix
for tracking (failing-first regression test + fix-log entry, `.claude/rules/bugfix.md`).

Current code: `SettingsDialog::updateApplySensitivity()` (~line 366) sets `btnApply_` sensitive iff
`enginePathValid_`; `onApply()` (~line 372) early-returns when `!enginePathValid_`;
`onApply()` builds `eConfig` with `eConfig.enginePath = entryEnginePath_.get_text()`.

1. **Failing-first test** `tests/test_ux09_apply_invalid_engine_path.cpp` in `ranls-gui-ui-tests`
   (self-skips without a display; pattern: `tests/test_pal03_settings_dialog.cpp`). Construct
   `SettingsDialog` with an `EngineConfig` whose `enginePath` is a non-existent path (and a second case
   where the base path is a valid executable, e.g. the built `mock_engine`, and the entry is then set
   to a bogus path). Assert: (i) the Apply button is sensitive; (ii) activating Apply emits
   `signal_applied` exactly once with the changed non-engine fields (e.g. language/theme) and with
   `enginePath == ` the **base** (previously saved) path, not the invalid entry text. Add whatever
   minimal test seam you need (e.g. a public `applyButton()` accessor, or reaching the button by
   walking widgets) — keep it small and documented. Must fail on current code.
2. **Fix:**
   - Apply is always sensitive. While the path is invalid, set the Apply tooltip to a `tr()` string,
     e.g. `"The engine path is invalid and will not be saved. Other settings will be applied."`;
     clear the tooltip when the path is valid.
   - `onApply()`: no early return. `eConfig.enginePath = enginePathValid_ ? entry text :
     baseEngineConfig_.enginePath`. Everything else unchanged.
   - Keep `updateApplySensitivity()` (or rename it to reflect it now updates the tooltip) — no dead code.
3. **Catalog:** add the tooltip string to `src/resources/lang/vi.tsv` (Claude drafts, user reviews),
   e.g. `Đường dẫn engine không hợp lệ và sẽ không được lưu. Các cài đặt khác vẫn được áp dụng.`
   ("engine" stays untranslated per existing catalog usage — check how vi.tsv renders "engine").
   Catalog lint must stay green.

## Pitfalls

- `MainWindow::openSettings` (`src/main_window.cpp` ~line 1115) computes `pathChanged` from the
  emitted `eConfig.enginePath`. Because the invalid path is replaced by the base path, `pathChanged`
  is false and only `sendConfig()` runs — verify `sendConfig()` is harmless when no engine is running
  (read `EngineController::sendConfig`). Do not modify `main_window.cpp` unless that check fails; if
  it does, stop and report rather than widening scope.
- An empty entry: `isValidEnginePath("")` is invalid → base path is kept. That means a user cannot
  *clear* the engine path through the dialog. Accept this (matches pre-UX-09 behaviour, where Apply was
  blocked entirely); note it in the fix-log as a known limitation.
- The Engine-tab inline status (`lblEnginePathStatus_`, "✗ <reason>") stays as is.
- `tr()` arguments must be literals visible to the catalog lint.
- Live GUI checks on this Wayland host: `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY …`.

## Verification before marking this task done

1. The new test fails on pre-fix code (record output in the fix-log detail), passes after.
2. `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY RUN_TESTS=1 ./build.sh` — ctest all green;
   `ranls-gui-tests` and `ranls-gui-ui-tests` pass counts reported, UI tests actually run (0 skipped);
   catalog lint green.
3. Live check under Xvfb: settings file with `engine_path=/nonexistent` and `language=en`; open
   Settings → Interface, pick Tiếng Việt, Apply → UI switches to Vietnamese; the settings file still
   has `engine_path=/nonexistent` (the previously saved value, unchanged) and now `language=vi`.
   Screenshot before/after; back up and restore any existing settings file.
4. Fix-log row in `docs/fix-log.md` + `docs/fix-log/<date>-ux09-apply-invalid-engine-path.md`;
   todo `Status:` → ✅ FIXED with verification notes; `TODO.md` Active line → ✅.

## Boundaries — do not touch

- Engine launch guards (`EngineController::startEngine` empty-path return, `EngineProcess::start`
  failure reporting) — defense in depth stays.
- Settings file format / `SettingsStorage` (`src/model/`).
- Other settings controls, tabs, layout; the inline Engine-tab status text.
- `main_window.cpp` (see Pitfalls).
