# I18N-07 — Confirm dialogs show English "Yes" / "No" under the Vietnamese UI

**Status:** 🔲 OPEN (Active — Sprint 27)
**Area:** `src/main_window.cpp` (`Gtk::MessageDialog` with `ButtonsType::YES_NO` ~line 849), `src/resources/lang/vi.tsv`, tests
**Priority:** P3
**Source:** I18N-06 visual pass, `docs/audit/2026-10-log.md` (2026-10-08 "Vietnamese UI visual verification") — 2026-10-08
**Design:** none — scoped directly
**Depends on / relates to:** I18N-02, I18N-06

## Problem

With `language=vi`, the "start a new game" and "open a game" confirmations render Vietnamese text but English **No / Yes** buttons.
`ButtonsType::YES_NO` uses GTK's own gettext stock labels, which our TSV catalog does not control, so the dialog is half-translated.
The user-visible effect is on every destructive-action confirmation.

## Scope (in order)

1. Failing-first test: under `vi`, the confirm dialog's buttons are the catalog's strings (not "Yes"/"No").
2. Replace `YES_NO` with explicit `add_button(i18n::tr("Yes"|"No"), ResponseType)` (or more descriptive verbs), keep response ids/default/Escape behaviour.
3. Add the two catalog entries to `vi.tsv` (Claude drafts, user reviews); lint stays green.

## Scope boundary

- Do not touch the dialogs' message text or other dialogs unless they use the same stock labels (list any found; don't expand).
- Do not change response handling semantics.

## Reasoning

Routing the labels through `i18n::tr` is the repo's mechanism (I18N-01); setting GTK's own locale was rejected (would split language control across two systems).

## Knowledge

`docs/knowledge/` UX heuristics note (consistency); `gtk-ui-design` skill.

## Acceptance criteria

- Under `vi` both confirmations show Vietnamese button labels; under `en` unchanged.
- Regression test fails before / passes after; ctest green; live screenshot under Xvfb recorded in the fix-log.
