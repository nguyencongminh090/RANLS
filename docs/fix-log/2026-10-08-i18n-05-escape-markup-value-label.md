# About dialog: escape translated text in markup value labels (2026-10-08)

**Status:** Active
**Related:** I18N-05, UI-22

## Prompt

Hardening follow-up to UI-22: `makeValueLabel(text, markup=true)` fed translated text straight to
`set_markup()`. **No live defect** — the current en/vi prefixes contain no `&` or `<`.

## Root cause

Same class as UI-22. The two markup callers built `i18n::format(tr("Repository: %s"), "<a ...>")` and
passed the result to `set_markup()`, so a bare `&` or `<` in a future translation would make Pango reject
the whole string and leave the label blank. Reproduced with an injected catalog (`Kho & <ma nguon>: %s`):
`Gtk-WARNING: Failed to set text 'Kho & <ma nguon>: <a href=...' ... Entity did not end with a semicolon`.

## Fix

New `makeLinkLabel(translatedFormat, linkMarkup)` escapes the translated format string with
`Glib::Markup::escape_text`, then substitutes the raw link markup for `%s` (escape first: `%s` is
unaffected by escaping, and the anchor markup must not be escaped). Both callers use it. The plain-text
path, other About sections and `vi.tsv` are unchanged.

## Verification

`tests/test_ui11_about_dialog.cpp` case "I18N-05: a translated link prefix with '&' and '<' still renders
and keeps its link" (test-only catalog loader; asserts label text, `<a href=` retained, no markup
warning). Failed before the fix, passes after. `xvfb-run` with `GDK_BACKEND=x11` and no `WAYLAND_DISPLAY`:
`RUN_TESTS=1 ./build.sh`, ctest 5/5. Not verified: clickability by a real pointer (anchor presence checked).

## Knowledge

`docs/fix-log/2026-10-08-ui-22-about-links-protocol-heading.md`; `.claude/rules/bugfix.md`.
