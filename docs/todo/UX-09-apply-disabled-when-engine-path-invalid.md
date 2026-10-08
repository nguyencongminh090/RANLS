# UX-09 — Settings "Apply" is disabled on every tab while the engine path is invalid

**Status:** 🔲 OPEN (Backlog)
**Area:** `src/ui/settings_dialog.cpp` (`btnApply_` sensitivity = `enginePathValid_`), tests
**Priority:** P3
**Source:** I18N-06 visual pass, `docs/audit/2026-10-log.md` (2026-10-08 "Vietnamese UI visual verification") — 2026-10-08
**Design:** none — scoped directly (design call below needs the user)
**Depends on / relates to:** I18N-03

## Problem

Observed: with a non-existent engine path, "Apply" stays disabled on all tabs. On the Interface tab the user can pick a Language or
Theme but cannot apply it, and the only explanation ("path does not exist") is on the Engine tab. A first-run user with no engine
configured therefore cannot switch language from the dialog. Not Vietnamese-specific.

## Scope (in order)

1. Decide with the user: (a) apply non-engine settings while the path is invalid, only blocking the engine-path field; or
   (b) keep blocking but surface the reason on every tab (e.g. tooltip/inline hint on Apply).
2. Implement the chosen option with a regression test.

## Scope boundary

- Do not weaken the invalid-path guard for actually launching the engine (defense in depth stays).
- No change to settings file format.

## Reasoning

Recorded as a decision point because (a) changes what Apply means. Alternative: leave as is — rejected, it hides a core setting.

## Knowledge

`docs/knowledge/` UX heuristics (visibility of system status, error prevention); `gtk-ui-design` skill.

## Acceptance criteria

- On the Interface tab with an invalid engine path the user either can apply, or sees why they cannot.
- Regression test; ctest green; fix-log entry (UX behaviour change, not a crash bug).
