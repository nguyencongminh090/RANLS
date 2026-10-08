# Sprint 26 (closed 2026-10-08)

**Goal:** Harden the About dialog's markup against bad translations, and visually verify the Vietnamese UI.
**Dates:** 2026-10-08 to 2026-10-08.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| I18N-05 | Escape translated text before Pango markup in `makeValueLabel` | ✅ DONE |
| I18N-06 | Visual verification pass of the Vietnamese UI | ✅ DONE |

Both items were committed at sprint open (filed from the Sprint 25 follow-ups). Points not estimated (consistent with Sprints 3–25).

## What shipped

- **I18N-05** (PR #65 squash `32b6c04`): hardening, no live defect. New `makeLinkLabel(translatedFormat, linkMarkup)` escapes the translated prefix (`Glib::Markup::escape_text`) before substituting the raw `<a href>` markup, so a `&`/`<` in a translation can no longer blank the "Repository" / "Engine protocol" labels. Regression test in `test_ui11_about_dialog.cpp` (injected catalog; failed before, passes after); fix-log `docs/fix-log/2026-10-08-i18n-05-escape-markup-value-label.md`. Real pointer click on the link not tested.
- **I18N-06** (no PR — verification only, tracking landed on `main`, deviation logged in `docs/audit/2026-10-log.md`, commit `2fa8dca`): live pass of the `vi`/`en` UI under Xvfb with a real engine. Main flows fine; two defects filed to Backlog — **I18N-07** (English Yes/No in confirm dialogs) and **UX-09** (Apply disabled while the engine path is invalid). Tooltips, native file chooser and Esc-on-modal remain human-owed (no WM under Xvfb).

## Lessons

- Run UI tests and live checks under `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset; re-run build, ctest and the benchmark on an agent's branch instead of trusting its report.
- Translated text must be escaped at every point it enters Pango markup (UI-22 heading, I18N-05 link labels); build the markup from escaped pieces, never escape the finished string.
- A visual pass is worth doing even when tests are green: it found a half-translated dialog (stock GTK buttons) and a settings-flow trap that no unit test pins.
- `gh label delete` is case-insensitive: deleting `area:i18n` removed `area:I18N`.

## Rolled over to Backlog

Nothing rolled over — all committed items finished. Backlog holds I18N-07 and UX-09 (filed from the I18N-06 pass).

## Next sprint

Sprint 27 — run `/sprint open 27 …` to commit Backlog items (I18N-07, UX-09). Release `v0.11.2` cut with this close.
