# Sprint 25 (closed 2026-10-08)

**Goal:** Palette titles follow the UI language, and the About dialog heading bug is fixed
**Dates:** 2026-10-08 to 2026-10-08.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| I18N-04 | Palette titles from the catalog | ✅ DONE |
| UI-22 | About "Links & protocol" heading blank | ✅ FIXED |

Both items were committed at sprint open; UI-22 was filed (Backlog) and pulled in the same session from a leftover reported by the I18N-02 agent. Points not estimated (consistent with Sprints 3–24).

## What shipped

- **UI-22** (PR #63 squash `1279aee`): root cause confirmed — `makeSection()` built `set_markup("<b>" + title + "</b>")` from the raw title, and the bare `&` made Pango reject the whole string (`Gtk-WARNING … escape ampersand as &amp;`), leaving the label empty. Fix: `Glib::Markup::escape_text(title)`. Regression test in `test_ui11_about_dialog.cpp` (failed before, passes after); fix-log `docs/fix-log/2026-10-08-ui-22-about-links-protocol-heading.md`. Follow-up not fixed: `makeValueLabel(..., markup=true)` passes translated prefixes into markup unescaped (no defect today).
- **I18N-04** (PR #64 squash `f56e66f`): palette rows show the title in the UI language with the other language dimmed, and a translated Action/Setting/Command badge (`palette_catalog::displayTitle`). Search data is unchanged: `titleVi` is read from `vi.tsv` via `i18n::trIn("vi", …)`; 62 title lines moved into the catalog. Tests pin identical ids/order/scores under en and vi, and catalog-built fields equal `tests/data/palette/entries.tsv`; bench test split unchanged. `keywordsVi` stays in code (ranking input).

## Lessons

- Run UI tests and live checks under `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset; re-run build, ctest and the benchmark on an agent's branch instead of trusting its report.
- A bug found during feature work (the About heading) is cheapest filed as its own CODE immediately; the verified root-cause + failing-test-first flow took one small PR.
- Moving display strings into a catalog can silently couple the catalog to other consumers (here `vi.tsv` now also feeds palette search) — document that coupling where the next editor will look (the lang README).
- Visual checks of live states (palette open during a language switch, engine-on views) are still human-owed.

## Rolled over to Backlog

Nothing rolled over — all committed items finished. Backlog is empty.

## Next sprint

Sprint 26 — run `/sprint open 26 …` to commit Backlog items. Release `v0.11.1` cut with this close.
