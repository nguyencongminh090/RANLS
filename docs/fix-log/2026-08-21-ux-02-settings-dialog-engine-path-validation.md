# 2026-08-21 — Settings dialog validates the engine path inline (UX-02)

## Summary

`SettingsDialog::onApply` (`src/ui/settings_dialog.cpp`) read `entryEnginePath_.get_text()` and
applied it with zero validation. A bogus path produced no GUI feedback at all: the dialog closed,
the config persisted, `MainWindow` restarted the engine, and the only trace of failure was a
`std::cerr` line inside `EngineProcess::start` — invisible in the UI.

## Fix

- Added `SettingsDialog::onEnginePathChanged()`, connected to `entryEnginePath_.signal_changed()`
  (fires on every keystroke and also when `onChooseEngine()`'s file chooser calls `set_text()`).
  It runs a local `isValidEnginePath()` helper (`std::filesystem::exists` +
  `std::filesystem::is_regular_file` + POSIX `access(path, X_OK)`) and updates a new
  `lblEnginePathStatus_` label placed directly under the engine-path entry, inside the same grid
  cell (via a small vertical `Gtk::Box` wrapping the existing path row) — inline next to the field,
  not a popup dialog and not console-only.
- On failure the label reads `✗ <reason>` (`Path is empty` / `Path does not exist` /
  `Not a regular file` / `Not executable`) and both the label and the entry gain an `error` CSS
  class. On success it reads `✓ Executable found` and the class is removed.
- Added `btnApply_` (stored pointer to the previously-local Apply button) and
  `updateApplySensitivity()`, which desensitizes Apply whenever the path is invalid — blocking
  Apply rather than only warning. `onApply()` also gained a redundant `if (!enginePathValid_)
  return;` guard as defense in depth.
- The initial validation pass runs once at the end of the constructor (after `btnApply_` is set),
  so opening Settings with an already-bad configured path shows the error immediately rather than
  waiting for the first keystroke.
- `onChooseEngine()` (the `Browse…` file chooser) was left untouched — `set_text()` already fires
  GTK's `changed` signal, so it re-validates automatically and keeps working as the "easy, already
  valid" path per the acceptance criteria.
- Investigated the `spinMaxNodes_` adjustment's `100000000000` (1e11) upper bound feeding
  `static_cast<int64_t>(spinMaxNodes_.get_value())` (a `double` round-trip): a `double` mantissa
  represents all integers exactly up to 2^53 (≈9.007e15); 1e11 needs only 37 bits. Verified with a
  standalone check (`double(1e11) -> int64_t -> double` round-trips exactly, `== 100000000000`
  bit-for-bit). No precision loss at the top of the range; no code change needed for this
  criterion. The other numeric fields (`maxDepth` 1–225, `threads` 1–256, `hashSizeMB` 1–65536MB,
  `multiPV` 1–20) already carry engine-meaningful `Gtk::Adjustment` bounds, not just arbitrary
  widget defaults, so no further range changes were made.
- Every input already had a visible `Gtk::Label` (no placeholder-only fields existed); the new
  `lblEnginePathStatus_` is an additional status line, not a replacement for the "Engine Path"
  label, so this criterion needed no change beyond not regressing it.

## Files changed

- `src/ui/settings_dialog.h` — new members `lblEnginePathStatus_`, `btnApply_`, `enginePathValid_`;
  new methods `onEnginePathChanged()`, `updateApplySensitivity()`.
- `src/ui/settings_dialog.cpp` — `isValidEnginePath()` helper, wiring described above.

## Verification

- `bash build.sh build_ux02` — clean build, no new warnings from `settings_dialog.cpp`.
- `ctest` in `build_ux02` — `rapfi-gui-tests` passes (1/1, unaffected by this change; no test
  infrastructure exists for `SettingsDialog` itself since it's a GTK dialog with no headless
  harness in this repo — noting this explicitly per the bug-fix workflow rather than skipping
  silently).
- Ran `rapfi-gui` under `xvfb-run` for a few seconds: starts and exits cleanly, no crash/stderr
  output, confirming the changed constructor path (including the new initial validation call)
  doesn't break startup.
- Reasoned through the control flow by hand for the three required interactive scenarios (no
  scripted GTK/AT-SPI driver available in this sandbox to click through the dialog):
  - Bogus path typed → `signal_changed` fires → `isValidEnginePath` fails at `exists` →
    label shows `✗ Path does not exist`, entry/label get `error` class, `btnApply_->set_sensitive
    (false)`.
  - Valid, executable path typed/selected → all three checks pass → label shows
    `✓ Executable found`, `error` class removed, Apply re-enabled.
  - `Browse…` still opens `Gtk::FileDialog` and calls `entryEnginePath_.set_text(...)` unchanged;
    that call triggers the same `changed` signal, so the new validation path and the browse path
    are the same code, not two divergent behaviors.
- Standalone C++ snippet (compiled and run, see summary above) confirmed the `double`→`int64_t`
  round-trip at `1e11` is lossless.

## Scope notes

- Did not touch STATE-02 or ENG-01 — both already fixed, used as-is per the task's stated
  assumption.
- Did not add validation to the hotkey text fields or other entries; the task's acceptance
  criteria scoped validation to the engine path and to confirming (not necessarily changing)
  numeric ranges.
