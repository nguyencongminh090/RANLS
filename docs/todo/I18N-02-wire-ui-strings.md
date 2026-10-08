# I18N-02 — UI language: wrap UI strings in `tr()` + Vietnamese catalog

**Status:** 🔲 OPEN (Active — Sprint 24)
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
