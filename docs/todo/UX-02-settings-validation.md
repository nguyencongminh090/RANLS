# UX-02 — Settings dialog accepts an invalid engine path with no feedback

**Status:** ✅ DONE

Engine path is now validated live via `SettingsDialog::onEnginePathChanged()` (connected to
`entryEnginePath_.signal_changed()`), checking existence / regular-file / executable via
`std::filesystem` + POSIX `access(X_OK)`. Result shown inline in a new `lblEnginePathStatus_`
label directly under the field (✓/✗ + reason), not a popup and not console-only. Apply is
desensitized while the path is invalid (plus a redundant guard in `onApply()`). `Browse…` is
untouched — `set_text()` already fires `changed`, so it re-validates through the same path.
Confirmed via a standalone check that `spinMaxNodes_`'s `100000000000` adjustment max round-trips
through `double`→`int64_t` exactly (needs 37 bits; doubles are exact up to 2^53) — no precision
loss, no code change required there. Verified: clean build (`build.sh`), `ctest` passes, app runs
under `xvfb-run` without crashing. Full detail: `docs/fix-log/2026-08-21-ux-02-settings-dialog-engine-path-validation.md`.
**Area:** settings dialog
**Priority:** P2
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

`SettingsDialog::onApply` (`src/ui/settings_dialog.cpp:147-177`) reads the engine path straight out
of the entry and applies it with no validation:

```cpp
eConfig.enginePath = entryEnginePath_.get_text();
```

Nothing checks that the path exists, is a file, or is executable. The dialog closes, the config is
persisted (`src/main_window.cpp:517`), and `MainWindow` immediately restarts the engine
(`src/main_window.cpp:519-522`). The failure then surfaces only as a `std::cerr` line inside
`EngineProcess::start` (`src/engine/engine_process.cpp:40`) — invisible in the GUI — while the status
indicator reports **● ON** (see ENG-01).

So the user's feedback for "I typed the wrong path" is: nothing happens, and the UI claims success.

The `ui-ux-review` checklist requires errors near the field, not only on submit and not only in a
log.

## Acceptance criteria

- Engine path is validated when it changes (exists / is a regular file / is executable), with the
  result shown inline next to the field — not as a dialog on Apply and not only in the console.
- Apply is blocked, or clearly warns, when the path is invalid.
- The `Browse…` file chooser (`src/ui/settings_dialog.cpp:179-192`) remains the easy path; manual
  entry is what needs the validation.
- Numeric fields are checked against their engine-meaningful ranges, not just the `Gtk::Adjustment`
  bounds. Note `spinMaxNodes_` uses an adjustment max of `100000000000`
  (`src/ui/settings_dialog.cpp:109`) against an `int64_t` field via `double` — confirm no precision
  surprise at the top of that range.
- Every input has a visible label (currently true — keep it that way; no placeholder-only fields).

## Scope boundary

- Losing `multiPV`/`customParams` on Apply is STATE-02 — a separate, more severe bug in the same
  function. Fix that first; this item assumes it is done.
- Engine start-failure reporting is ENG-01.

## Related

- STATE-02 (config field loss), ENG-01 (start failure feedback)
