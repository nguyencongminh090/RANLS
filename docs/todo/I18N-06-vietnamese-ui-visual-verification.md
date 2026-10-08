# I18N-06 — Visual verification pass of the Vietnamese UI

**Status:** ✅ DONE (verified 2026-10-08; findings in `docs/audit/2026-10-log.md` (2026-10-08 "Vietnamese UI visual verification"), filed as I18N-07, UX-09)
**Area:** verification only (Xvfb + `GDK_BACKEND=x11` screenshots); fixes found become separate CODEs
**Priority:** P3
**Source:** Sprint 25 follow-up (human-owed visual checks) — user asked to file it 2026-10-08
**Design:** none — scoped directly
**Depends on / relates to:** I18N-02, I18N-03, I18N-04, PAL-04

## Problem

Unit tests pin catalog contents and ids, but nobody has looked at the Vietnamese UI in a live window.
Unchecked states: palette open during a live language switch (rows refresh only on next keystroke/
reopen), the palette `!` view in English, engine-on states (PV rows, board tooltips, crash banner,
confirm/file dialogs), the Settings Language row, and text truncation/overflow from longer Vietnamese strings.

## Scope (in order)

1. Run the app per memory recipe (`unset WAYLAND_DISPLAY; GDK_BACKEND=x11` under `xvfb-run`, PIL screenshots,
   `xdotool`) with `language=vi` and with `en`; capture each state listed above (engine-on using `tests/mock_engine`).
2. Record findings (screenshots path + one line each) in a `docs/notes/` note; file each defect as its own
   CODE (bugs via `systematic-debugging` + regression test + fix-log).

## Scope boundary

- Verification only: do not fix anything found inside this task.
- Known leftovers (already-open dialogs/crash banner keep old language) are expected, not findings.

## Reasoning

Cheapest way to catch layout/overflow and refresh gaps that tests cannot see. Alternative — golden
screenshot tests — rejected as out of scope/fragile across themes.

## Knowledge

`ui-ux-review` and `gtk-ui-design` skills; `docs/knowledge/` UX heuristics note if layout issues arise.

## Acceptance criteria

- Every state listed in Problem captured under `vi` (and `!` view under `en`), reviewed, and summarised in a note.
- Each defect found is filed as a separate CODE; nothing silently fixed.
