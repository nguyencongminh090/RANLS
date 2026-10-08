# Sprint 24 (closed 2026-10-08)

**Goal:** Palette scopes (`!` `>` `@`) and a switchable UI language (English / Tiếng Việt)
**Dates:** 2026-10-08 to 2026-10-08.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| PAL-04 | Palette scope prefixes, UI-only default | ✅ DONE |
| I18N-01 | GTK-free catalog loader `i18n::tr` + lint | ✅ DONE |
| I18N-02 | Wire UI strings + vi catalog + live refresh | ✅ DONE |
| I18N-03 | Language setting + persistence + system default | ✅ DONE |

All four items were committed at sprint open; no mid-sprint pulls. Points not estimated (consistent with Sprints 3–23). Design for the I18N chain was resolved at open (`features/ui-language/planning.md` "Resolution").

## What shipped

- **PAL-04** (PR #60 squash `79a9bac`): pure `parseScope` (`!` commands, `>` actions, `@` settings, none = actions+settings) replaces the `bang` special case; scope chip and prefix hints in the palette; 15 scope rows added to the dataset. 8 legacy cmd-only bench rows can no longer match without `!` by design, so typo MRR 0.955→0.926 (noted in the PR).
- **I18N-01** (PR #59 squash `732bbd8`): GTK-free `src/i18n/` (TSV catalog, `tr`/`trc`, registry, `resolveSystem`) plus lint for orphan keys, placeholders and do-not-translate terms.
- **I18N-02** (PR #61 squash `e789d93`): 185 UI strings wrapped, complete user-reviewed `vi.tsv`, coverage lint, language listeners and `MainWindow::applyLanguage` for live refresh.
- **I18N-03** (PR #62 squash `712ff61`): `language=` setting, Settings ▸ UI dropdown, system-locale startup, palette dataset rows for `set.language`; bench MRR 0.981→0.982.

## Lessons

- Run UI tests and live checks under `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset (a stray ctest run without it can open windows on the real desktop).
- Keep drift guards between registries and the benchmark dataset; new settings need dataset rows.
- Splitting a cross-layer feature into loader+lint → wiring → setting let the lint catch every missed string mechanically; keep that order for future localisation work.
- Hand-off reports describe intent: re-running build, ctest and the bench on the agent's branch caught nothing this sprint, but the PAL-04 benchmark deviation only surfaced because the numbers were re-read.
- Visual checks of live states (engine on, PV rows, banners, confirm dialogs) under `vi` are still human-owed.

## Rolled over to Backlog

Nothing rolled over — all committed items finished. Backlog keeps I18N-04 (palette titles from the catalog). Known unfiled bug: About heading "Links & protocol" renders blank (bare `&` in Pango markup) — file with the bugfix pipeline before fixing.

## Next sprint

Sprint 25 — run `/sprint open 25 …` to commit Backlog items. Release `v0.11.0` cut with this close.
