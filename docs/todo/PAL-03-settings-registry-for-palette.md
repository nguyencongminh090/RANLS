# PAL-03 — Declarative settings registry (searchable settings)

**Status:** ✅ DONE (2026-10-08, PR #57 squash `549c512`) — `src/ui/settings_registry.h` (20 rows: id, tab, label, en/vi title + keywords); `SettingsDialog::addRow` by id (fails loudly if a row is missing/wrong tab), `showSetting(id)` + `registeredSettingIds()`, `MainWindow::openSettings(id)`. Tests: `test_pal03_settings_registry.cpp` (well-formed, in sync with `entries.tsv`), `test_pal03_settings_dialog.cpp` (dialog builds exactly the registry rows; tab switch). Release ctest 5/5. Not verified: control focus (dialog not shown in test) and a live-window look.
**Area:** `src/ui/settings_dialog.{h,cpp}`, new `SettingEntry` table
**Priority:** P3
**Source:** user request 2026-10-08 (palette must search settings)
**Design:** features/command-palette/ (Q1)
**Depends on / relates to:** PAL-01, PAL-02

## Problem

`settings_dialog.cpp` builds widgets directly, so there is nothing to index. Settings cannot be found or jumped to.

## Scope (in order)

1. Small `SettingEntry` table (id, page, en/vi title, keywords).
2. Settings dialog can open at a page and focus a widget by id.
3. Register entries with the palette (kind = setting).

## Scope boundary

- Behavior-preserving refactor of the dialog; no new settings, no layout redesign.

## Reasoning

A declarative table is the minimum that makes settings searchable; rejected: searching only a top-level "Settings" entry (poor discoverability).

## Knowledge

`gtk-ui-design`; `docs/knowledge/` SOLID-OCP note.

## Acceptance criteria

- Every setting in the dialog has an entry; selecting one opens the dialog on its page with the control focused.
