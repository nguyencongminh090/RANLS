# I18N-02 — UI language: wrap UI strings in `tr()` + Vietnamese catalog

**Status:** ✅ DONE (2026-10-08, branch `i18n-02/wire-ui-strings`) — 185 `tr()` keys wrapped across `main_window.cpp` (hamburger menu model, header/toolbar tooltips + labels, rule chip, confirm/error/file dialogs, board-size dialog), `ui/` (Settings dialog + tabs + tooltips + validation reasons, About, analysis panel tabs/banner, tree-table columns, engine status chip/labels/tooltips, bottom-panel tabs/placeholder, win-graph tooltip, `PV #%d`, palette placeholder/hint/scope chip/empty state) and the model layer (`BoardViewModel::tooltipFor`, `GameFileService` save error). Concatenated/printf strings are single `%s`/`%d` template keys filled by new `i18n::format(tr("..."), args...)`. `src/resources/lang/vi.tsv` is now a complete draft (all 185 keys, terminology table in `src/resources/lang/README.md`, **for user review**); the I18N-01 seed fixture `tests/data/i18n/seed_reference.cpp` is removed. Lint (`i18n_lint`) gained `checkCoverage` (every `tr()` key in `src/` needs a non-empty vi entry) next to orphan / placeholder / do-not-translate checks, with a negative fixture. Live refresh: `i18n::addLanguageListener/removeLanguageListener` (std-only, notified by `setLanguage`); `MainWindow` registers one listener that rebuilds the menu model and re-texts header buttons, rule chip, panel tabs, tree columns, engine status, bottom-panel tabs and palette chrome; public `MainWindow::applyLanguage(code)` for I18N-03; dialogs are built on open. **Verification:** `RUN_TESTS=1 ./build.sh` under `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY` — ctest 5/5 (`ranls-gui-tests` incl. new `test_i18n02_wiring.cpp` + coverage lint; `ranls-gui-ui-tests` incl. `test_i18n02_live_refresh.cpp`: menu/tooltips/rule chip/tabs/status switch to vi and back to en, Settings + About built after the switch are Vietnamese; all pre-existing UI tests pass unchanged under `en`). Visual pass under Xvfb (x11) with `vi`: main window, hamburger menu, command palette, all 5 Settings tabs, About, board-size dialog — no clipped/overflowing text, no layout change needed. **Leftovers / restart-to-apply:** (1) an engine-crash banner already on screen keeps its text until dismissed; (2) a Settings/About/board-size/confirm dialog that is already open keeps its language until reopened; (3) the dim engine-path status line and `.ptc`-driven status fields/toasts are engine-supplied or re-evaluated on next change; (4) palette entry titles and search data and the palette result *group* label are I18N-04; (5) technical parse/IO error details from `game_io`/`rdb` (`res.error` bodies) and Engine Log / `!` / protocol text stay English by design; (6) pre-existing, unchanged: the About section heading "Links & protocol" renders blank because `makeSection()` puts a bare `&` into Pango markup (Gtk warning) — separate bug, record before fixing. No language selector / persistence / `setCatalogLoader` call yet (I18N-03).
**Area:** `src/ui/*.cpp`, `src/main_window.cpp` (`buildMenuBar()`), `src/resources/lang/vi.tsv`
**Priority:** P2
**Source:** user request 2026-10-08; `features/ui-language/planning.md` Q2/Q4/Q5/Q10
**Design:** features/ui-language/
**Depends on / relates to:** I18N-01 (loader + lint); I18N-03 (setting that drives it)

## Problem

Menus, dialogs, tooltips, labels, status/error messages and About are English literals.

## Scope (in order)

1. Wrap in-scope strings (planning Q4) with `i18n::tr()`; keep English literal as the key. Do-not-translate terms untouched.
2. Draft `vi.tsv` for all keys (Claude drafts, user reviews); fallback stays English.
3. Live refresh (planning Q2): menu model rebuilt, header/labels refreshed on the language-changed signal; dialogs built on open. Anything not refreshable is documented in the Settings row as "restart to apply".
4. Visual pass (Q10): Settings tabs, toolbar, header chip, About rendered under Xvfb (`GDK_BACKEND=x11`), check for clipped Vietnamese text.

## Scope boundary

- Not the Engine Log, `!` commands/`!help`, protocol text, rule names, file formats.
- Not palette titles (I18N-04) and not the language selector (I18N-03; tests set the language directly).
- No behaviour change besides text.

## Reasoning

Doing the wiring after the loader+lint lets the lint catch every missed/mismatched string mechanically. Live refresh rather than restart avoids a stale half-translated UI.

## Knowledge

`features/ui-language/planning.md`; `.claude/skills/gtk-ui-design`; memory note on live GUI checks under Xvfb/x11.

## Acceptance criteria

- With `vi` selected, every in-scope surface is Vietnamese; with `en` the UI is byte-identical to before (existing UI tests pass unchanged).
- Lint clean: no missing/orphan keys, placeholders intact.
- Live switch updates menus and header without restart; leftovers listed.
- Screenshot pass of each Settings tab + main window under vi shows no clipping (or fixes made).
