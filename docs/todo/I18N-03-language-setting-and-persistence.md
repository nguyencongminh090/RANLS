# I18N-03 — UI language: Language setting, persistence, system default

**Status:** ✅ DONE (2026-10-08, branch `i18n-03/language-setting-and-persistence`) — `ViewConfig::language` (`system`|`en`|`vi`, default `system`) persisted as `language=` in `SettingsStorage`; missing/unknown values load and save as `system`. Pure `i18n::normalizeLanguageSetting` / `resolveSetting`; GLib locale lookup and the bundled-loader install live in the UI layer (`src/ui/language_setting.{h,cpp}`, `applySetting()`), so `src/i18n` stays GTK-free. `RapfiApplication::on_activate` applies the language before `MainWindow` is constructed; Settings Apply calls the same function (live refresh through the I18N-02 listeners) and saves on the existing save path. `set.language` row on the UI tab (dropdown System / English / Tiếng Việt, native names from `i18n::languages()`), registered in `settings_registry.h` with en/vi title+keywords; palette dataset: `set.language` entry + 8 query rows, lexicon synonym group, `vi.tsv` keys "Language" and its tooltip. The dialog closes on Apply, so the next open is rebuilt in the new language (documented in `src/resources/lang/README.md`). **Verification:** `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY RUN_TESTS=1 ./build.sh` — ctest 5/5 (new `test_i18n03_language_setting.cpp`: round-trip, bad value -> system, resolution vi_VN->vi / fr_FR->en, registry row; `test_i18n03_ui.cpp`: dropdown choices/preselect/Apply output, startup glue, live switch to Tiếng Việt and back, dialog built afterwards translated); `palette_bench.py --split test` (101 -> 105 queries, 61 -> 62 entries): MRR A 0.981 -> 0.982, recall@1 0.970 -> 0.971, no category regressed, all 4 new test-split rows hit at rank 1. Manual Xvfb run with a throwaway HOME and no saved config: `LANG=vi_VN.UTF-8` starts in Vietnamese, `LANG=fr_FR.UTF-8` in English (screenshots read). **Not checked:** a pixel-level screenshot of the Settings dialog row itself (covered by widget tests only); a real `en`/`vi` toggle through the live dialog with a mouse; Windows/macOS locale names (`g_get_language_names` is used as-is).
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
