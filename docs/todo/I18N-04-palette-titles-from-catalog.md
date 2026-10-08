# I18N-04 — UI language: palette titles from the catalog

**Status:** ✅ DONE (2026-10-08, branch `i18n-04/palette-titles-from-catalog`) — the palette row's primary title is now `displayTitle()` (the catalog text of the English title for the UI language; the other language stays as a dim second line) and the result-group badge (Action / Setting / Command) is `tr()`'d, so a Vietnamese UI shows Vietnamese titles. The 62 Vietnamese *titles* (22 actions, 21 settings, 19 built-in `!` commands) moved out of `palette_catalog.cpp` / `settings_registry.h` into `vi.tsv` (`palette|<English title>`, `palette-cmd|<name>`, plus `Action` / `Setting` / `Command`); same wording as before, so nothing the user saw changes. **Search is untouched:** `buildEntries()` still fills `Entry::titleVi` for the index, now read from the same vi catalog through the new `i18n::trIn("vi", ...)` / `lookupIn` (language-independent, so ranking never depends on the UI language, L7); `keywordsEn/Vi`, the lexicon, `palette_search`, scoring and the `!`/`>`/`@` scope logic were not edited. New std-only helpers: `i18n::trNoop/trcNoop` (key markers the lint recognises), `lookupIn`, `trIn`; the do-not-translate lint accepts a camel-case join (`Nhiều biến (MultiPV)` keeps `PV`, so that search title is byte-identical). **Deviation from the acceptance wording:** "no Vietnamese literals left" holds for titles only; the Vietnamese *keyword* lists (`keywordsVi`) stay in code because they are ranking inputs with no display role. **Verification:** (1) pre-change dump of `buildEntries()` (all entries' search fields) plus ids/scores of 33 queries compared byte-for-byte before/after: identical; a new test also pins catalog-built search fields to `tests/data/palette/entries.tsv`. (2) `python3.13 scripts/palette_bench.py --split test` before/after: identical (A: MRR 0.982, recall@1 0.971, @3 0.990, @8 1.000, nDCG@8 0.953; note the benchmark reads the TSV dataset, so it guards the engine, the dump + dataset test guard the catalog data). (3) `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY RUN_TESTS=1 ./build.sh` — ctest 5/5, incl. `test_i18n04_palette_titles.cpp` (en vs vi display for an action, a setting and a console command; search data and ids/order/scores identical under en/vi; every title has a vi entry; lint camel-case rule) and `test_i18n04_ui.cpp` (real palette rows en vs vi, same ids/order for 17 queries); coverage lint green. (4) Live Xvfb/x11 look in `vi` and `en` (`settings`, `!undo`): Vietnamese primary line + English dim line + `Thao tác`/`Cài đặt`/`Lệnh` badges; English unchanged. **Leftovers:** extension (`.ptc`) commands have no Vietnamese text (English summary shown, as before); an already-open palette keeps its rows until the next keystroke/open.
**Area:** `src/ui/palette_catalog.cpp`, `src/ui/settings_registry.h`, `src/command/palette_search` display path
**Priority:** P3
**Source:** `features/ui-language/planning.md` Q7 (follow-up task)
**Design:** features/ui-language/
**Depends on / relates to:** I18N-01..03

## Problem

Palette entries carry duplicated hard-coded Vietnamese titles; the displayed title should follow the UI language while search keeps matching both languages (story L7).

## Scope (in order)

1. Display title via `tr()`; keep both-language search fields (lexicon) unchanged.
2. Remove duplicated vi strings from code, move into `vi.tsv`; benchmark rerun.

## Scope boundary

- No change to ranking or the lexicon; no new UI-language features.

## Reasoning

Single source of truth for Vietnamese strings; search accuracy must not depend on UI language.

## Knowledge

`features/ui-language/planning.md`; `docs/audit/2026-10-log.md` (PAL-01 benchmark).

## Acceptance criteria

- Title shown follows UI language; queries in either language return the same ranking as before (benchmark MRR unchanged).
- No Vietnamese literals left in `palette_catalog.cpp`/`settings_registry.h`.
