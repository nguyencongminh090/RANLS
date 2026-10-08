# I18N-01 — UI language: GTK-free catalog loader (`i18n::tr`) + lint tests

**Status:** 🔲 OPEN (Active — Sprint 24)
**Area:** new `src/i18n/` (no gtk, no engine), `src/resources/lang/{en,vi}.tsv` + `ranls.gresource.xml`, `tests/`
**Priority:** P2
**Source:** user request 2026-10-08 (UI Language setting); design resolved in `features/ui-language/planning.md` "Resolution"
**Design:** features/ui-language/
**Depends on / relates to:** — (first of I18N-02, I18N-03)

## Problem

The UI has no translation mechanism; strings are C++ literals. Every later I18N task needs a loader that is unit-testable without GTK and a lint that keeps catalogs consistent.

## Scope (in order)

1. `i18n::Catalog` (parse TSV `key<TAB>translation`, `\n`/`\t` escapes, `ctx|text` keys) and `i18n::tr(std::string_view)` / `trc(ctx, text)`; unknown key or empty translation → the English key.
2. Language registry: `en` (identity), `vi`; `setLanguage(code)`, `currentLanguage()`; `resolveSystem(const std::vector<std::string>& localeNames)` pure, for L2.
3. Load catalogs from the GResource in the app (loader takes text, so tests feed strings/files).
4. Lint (ctest): `vi.tsv` keys ⊆ reference keys, placeholder multiset equal per key, do-not-translate terms preserved; reference list generated from the English literals wrapped in `tr()` (script or ctest scan).
5. Seed `vi.tsv` with a handful of entries only to exercise the lint; real strings come in I18N-02.

## Scope boundary

- No UI file edited, no setting added (I18N-02/03). `src/i18n/` must not include gtk or `engine/`.
- No plural rules.

## Reasoning

Own TSV matches how `palette_lexicon.tsv` ships and avoids msgfmt/libintl on MSYS2/MSVC (planning Q1). Pure header keeps it covered by `ranls-gui-tests`.

## Knowledge

`features/ui-language/planning.md`; `docs/knowledge/principle-layering-dependency-rule.md`.

## Acceptance criteria

- Unit tests: parse/escapes, fallback to English, ctx keys, `resolveSystem` (`vi_VN.UTF-8` → vi, `fr_FR` → en).
- Lint test fails on an orphan key, a missing placeholder, and an altered do-not-translate term (each proven by a negative fixture).
- `arch01` and build remain green; full ctest passes.
