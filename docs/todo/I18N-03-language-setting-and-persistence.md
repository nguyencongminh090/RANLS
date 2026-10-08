# I18N-03 — UI language: Language setting, persistence, system default

**Status:** 🔲 OPEN (Active — Sprint 24)
**Area:** `src/ui/settings_registry.h`, Settings dialog (UI tab), `SettingsStorage`, startup in `src/main.cpp`/`main_window.cpp`
**Priority:** P2
**Source:** user request 2026-10-08; `features/ui-language/planning.md` Q3/Q6
**Design:** features/ui-language/
**Depends on / relates to:** I18N-01; pairs with I18N-02

## Problem

There is no way to choose or persist the UI language, and no system-locale default.

## Scope (in order)

1. `language=` key (`system`|`en`|`vi`, default `system`) in `SettingsStorage`; unknown value → `system`.
2. `set.language` row in Settings → UI tab (dropdown System / English / Tiếng Việt) registered in `settings_registry.h` so it is palette-searchable (en/vi lexicon + dataset rows).
3. Startup: resolve `system` via `i18n::resolveSystem(g_get_language_names())`, call `setLanguage`; changing the dropdown emits the language-changed signal (I18N-02 refresh).

## Scope boundary

- No catalog content or string wrapping (I18N-02). No other settings changed.

## Reasoning

A registry row gives palette discoverability for free (PAL-03) and keeps the persistence format a flat `key=value`.

## Knowledge

`features/ui-language/planning.md`; `docs/todo/PAL-03-settings-registry-for-palette.md`.

## Acceptance criteria

- Round-trip test: save/load `language=`; bad value falls back to `system`.
- Test that `set.language` exists in the registry (PAL-03 drift guard) and the palette dataset has rows for it; `scripts/palette_bench.py` shows no regression.
- First run with `LANG=vi_VN.UTF-8` starts in Vietnamese; with `LANG=fr_FR` in English.
