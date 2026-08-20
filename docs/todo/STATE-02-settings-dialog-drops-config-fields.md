# STATE-02 — Settings dialog silently resets multiPV and wipes customParams

**Status:** open
**Area:** settings dialog / config persistence
**Priority:** P0
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

`SettingsDialog::onApply` (`src/ui/settings_dialog.cpp:147-177`) constructs a brand-new
`EngineConfig eConfig;` and assigns only the fields the dialog exposes. Two `EngineConfig` members
are never assigned and therefore silently revert to their struct defaults
(`src/model/config.h:37-48`):

- `multiPV` → back to `1`
- `customParams` → emptied

`MainWindow` then persists that reset to disk unconditionally
(`src/main_window.cpp:513-526`, `SettingsStorage::save` at `src/model/settings_storage.cpp:182`),
so the loss is permanent, not just for the session.

`multiPV` is reachable only through the console command `!analyze n`
(`src/command/command_dispatcher.cpp:286-291`) or `!set multipv`
(`src/command/command_dispatcher.cpp:575`). So the sequence "set multiPV to 8, open Settings to
change anything at all, press Apply" silently drops the user back to single-PV analysis with no
message.

The dialog has no multiPV control at all, which is what makes the loss invisible.

## Why it matters

MultiPV is a headline feature of an analysis GUI — the `ui-ux-review` checklist calls out reviewing
the PV view at `multiPV=1` vs `multiPV=8+`. A settings dialog that quietly reverts it is a data-loss
bug, not a cosmetic one.

## Acceptance criteria

- `onApply` starts from the **current** `EngineConfig` and mutates only the fields the dialog owns,
  so any field added to the struct later is preserved by default rather than dropped by default.
- The same audit applied to `ViewConfig` (`src/ui/settings_dialog.cpp:159-173`) — confirm no field
  there is dropped the same way, including `showDatabase`, which the dialog does **not** expose and
  which currently resets to its default `true` on every Apply.
- `multiPV` gets a visible control in the Engine Setting frame — a `Gtk::SpinButton` with a sane
  range, matching the numeric-input pattern already used correctly for board size
  (`src/main_window.cpp:491-492`).
- Round-trip test: set non-default multiPV + a custom param, Apply, reload settings, assert both
  survive.

## Scope boundary

- Engine-path validation and inline error feedback is UX-02, not this item.
- Do not redesign the dialog layout; this is a correctness fix plus one new field.

## Related

- UX-02 (settings validation/feedback), RT-01 (multiPV amplifies the update-rate problem)
