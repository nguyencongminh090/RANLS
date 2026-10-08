# I18N-05 — Escape translated text before Pango markup in makeValueLabel

**Status:** ✅ DONE (2026-10-08, branch `i18n-05/escape-translated-text-in-value-label-markup`) — hardening, no live defect. New `makeLinkLabel(translatedFormat, linkMarkup)` in `src/ui/about_dialog.cpp` escapes the translated format string (`Glib::Markup::escape_text`) BEFORE substituting the raw `<a href>` markup for `%s`; the two markup=true callers ("Repository: %s", "Engine protocol: %s") use it. `makeValueLabel`'s plain-text path, other About sections and `vi.tsv` untouched. **Verification:** new case in `tests/test_ui11_about_dialog.cpp` (injected vi catalog with `&` and `<` in both prefixes: label text = prefix + link text, `<a href=` kept, no markup warning) failed before (label blank + Gtk-WARNING), passes after; `xvfb-run` (GDK_BACKEND=x11, no WAYLAND_DISPLAY) `RUN_TESTS=1 ./build.sh` ctest 5/5; the UI-22 case confirms no markup warning building the real About dialog in en and vi. Detail: `docs/fix-log/2026-10-08-i18n-05-escape-markup-value-label.md`.
**Area:** `src/ui/about_dialog.cpp` (`makeValueLabel`, its two `markup=true` callers), `tests/test_ui11_about_dialog.cpp`
**Priority:** P3
**Source:** Sprint 25 follow-up (UI-22 lesson; `docs/sprint/archive/sprint-25.md`) — user asked to file it 2026-10-08
**Design:** none — scoped directly
**Depends on / relates to:** UI-22 (same root cause class), I18N-02

## Problem

`makeValueLabel(text, markup=true)` passes `text` straight to `set_markup()`. The two callers build it
from translated prefixes ("Repository: %s", "Engine protocol: %s") with `<a href>` anchors injected
via `i18n::format`. If a translation ever contains a bare `&` or `<`, Pango rejects the whole string
and the label renders blank — the same failure as UI-22. No defect today (current en/vi prefixes are
clean); this is hardening against the next translation edit.

## Scope (in order)

1. Make the markup path escape the translated prefix while keeping the anchors live (e.g. split
   prefix/link: escape the prefix with `Glib::Markup::escape_text`, then append the `<a>` markup).
2. Regression test: a prefix containing `&` and `<` still produces a label with non-empty text and
   a working link (use the existing About dialog test fixture; inject via a test-only catalog entry
   or by testing the helper through the same construction path).

## Scope boundary

- Do not change the plain-text (`markup=false`) path or other About sections.
- Do not touch `vi.tsv` wording.

## Reasoning

Escape at the point markup is assembled (defence in depth after UI-22's `makeSection` fix) rather than
trusting every catalog author. Alternative — a lint rejecting `&`/`<` in catalog values — rejected:
legitimate strings ("Links & protocol") contain `&`.

## Knowledge

`docs/fix-log/2026-10-08-ui-22-about-links-protocol-heading.md`; `gtk-ui-design` skill.

## Acceptance criteria

- A translated prefix containing `&`/`<` renders (label text non-empty) and the link anchor is still clickable.
- New regression test fails before the change, passes after.
- Full `ctest` green; no `Gtk-WARNING` about markup in the About dialog under en and vi.
