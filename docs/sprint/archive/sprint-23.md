# Sprint 23 (closed 2026-10-08)

**Goal:** Command palette (Ctrl+K): bilingual classical-NLP search over actions, settings and ! commands, benchmarked against a Whoosh baseline
**Dates:** 2026-10-08 to 2026-10-08.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| PAL-01 | Pure en/vi search engine (BM25F) + shared dataset + Whoosh benchmark | ✅ DONE |
| PAL-02 | Ctrl+K overlay + action/`!`-command registries | ✅ DONE |
| PAL-03 | Declarative settings registry so settings are searchable | ✅ DONE |

Designed, formalised (`features/command-palette/`, PAL-01/02/03) and shipped in one day after the design gate was resolved with the user. Points were not estimated.

## What shipped

- **PAL-01** (PR #56 squash `dccc5ca`): GTK-free `src/command/palette_search.{h,cpp}` — NFC + diacritic folding, lexicon phrase max-match for Vietnamese, stopwords, light stemming, synonym/fuzzy/prefix expansion, BM25F, `!` command filter. Shared dataset `tests/data/palette/` (61 entries, 199 queries, dev/test split), doctest golden-set gate (recall@3 ≥ 0.9, < 5 ms), `ranls-palette-eval` and `scripts/palette_bench.py` (C++ vs Whoosh baseline). Test split: MRR 0.980 vs 0.950 (Whoosh + shared prep) vs 0.719 (stock Whoosh).
- **PAL-03** (PR #57 squash `549c512`): `src/ui/settings_registry.h` (20 rows), registry-driven `SettingsDialog` rows, `showSetting(id)`, `MainWindow::openSettings(id)`; registry ⇄ dataset drift test and a dialog-builds-every-row test.
- **PAL-02** (PR #58 squash `0a46eba`): `CommandPalette` popover on Ctrl+K (+ View ▸ Command Palette…), `palette_catalog` (22 actions + 20 settings + live `!` commands), recent items persisted as `palette_recent=`, lexicon bundled via GResource; driven live under Xvfb. Deferred: match highlighting, enabled-state predicates, noisy tail results on short queries (needs a PAL-01 relative score cutoff).

## Lessons

- Author the shared dataset before the engine, and keep the test split untouched: the benchmark numbers are still optimistic (small, author-written corpus, lexicon written alongside the queries) — treat them as a regression baseline, not a quality claim.
- Drift guards between a registry/catalog and the benchmark dataset (ids checked both ways) cost little and keep the benchmark honest as the app grows.
- Run UI tests and live checks under `xvfb-run` with `GDK_BACKEND=x11` and `WAYLAND_DISPLAY` unset: on a Wayland session GTK otherwise opens windows on the real desktop (and `xdotool` sees nothing).
- Visual checks are still mostly human-owed; the palette was exercised live under Xvfb only.

## Rolled over to Backlog

Nothing rolled over — all committed items finished. Unfiled follow-ups (not in Backlog): relative score cutoff in `palette_search`, match highlighting in palette rows, wiring enabled-state predicates, full UI i18n (Vietnamese labels).

## Next sprint

Sprint 24 — not yet opened. Release `v0.10.0` cut at close (see `CHANGELOG.md`).
