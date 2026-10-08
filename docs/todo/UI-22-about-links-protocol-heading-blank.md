# UI-22 — About dialog: "Links & protocol" section heading renders blank

**Status:** ✅ FIXED (2026-10-08, branch `ui-22/about-links-protocol-heading-blank`) — root cause: `makeSection()` put the raw title into Pango markup (`"<b>" + title + "</b>"`), so the bare `&` failed to parse ("Entity did not end with a semicolon…") and the heading stayed empty; the other headings have no `&`. Fix: `Glib::Markup::escape_text(title)`. **Verification:** new case in `tests/test_ui11_about_dialog.cpp` (every About heading present, no markup warning, en+vi) failed before, passes after; `GDK_BACKEND=x11 xvfb-run -a env -u WAYLAND_DISPLAY RUN_TESTS=1 ./build.sh` ctest 5/5; before/after screenshots (en, vi) read. Verbatim warning and detail: `docs/fix-log/2026-10-08-ui-22-about-links-protocol-heading.md`. **Not checked:** the live Help menu path (scratch harness presented the dialog). **Follow-up:** `makeValueLabel(markup=true)` does not escape the translated prefix; safe with current strings, would break if a translation gains `&`/`<`.
**Area:** `src/ui/about_dialog.cpp` (`makeSection()`), tests/
**Priority:** P3
**Source:** user request 2026-10-08 ("BugAbout"); found by the I18N-02 agent during the vi visual pass
**Design:** none — scoped directly
**Depends on / relates to:** I18N-02 (the heading is now `tr("Links & protocol")`)

## Problem

In Help ▸ About the section heading "Links & protocol" is blank (the other headings render) and GTK logs a warning. Reported by the I18N-02 agent as pre-existing and unchanged by that task; its hypothesis is that `makeSection()` passes the title into Pango markup with a bare `&`. **That is a hypothesis, not yet verified** — confirm with `systematic-debugging` before fixing.

## Scope (in order)

1. Reproduce: open About under Xvfb (`GDK_BACKEND=x11`), capture the exact GTK warning text and a screenshot; check it in both `en` and `vi`.
2. Trace to the root cause per `.claude/rules/bugfix.md` (`systematic-debugging` first; `gtk-ui-design` for the markup path).
3. Fix at the source (escape or stop using markup for the title), then validate other `makeSection`/markup callers for the same class of bug.
4. Regression test (a widget test that every About section heading has non-empty text, and no markup parse of `&`), plus `docs/fix-log.md` row and `docs/fix-log/<date>-<slug>.md`.

## Scope boundary

- No other About layout or wording changes; no new translations (the `vi.tsv` key stays).
- Do not touch other dialogs unless the same defect is proven there; list them as follow-ups instead.

## Reasoning

Smallest change is to escape the title (or use plain text) at the single place it is built. Rejected: removing the `&` from the string, which would change the translated key and hide the markup bug for any future title.

## Knowledge

`.claude/rules/bugfix.md`; skills `systematic-debugging`, `gtk-ui-design`; `docs/todo/I18N-02-wire-ui-strings.md` (leftover 6).

## Acceptance criteria

- Root cause recorded in the fix-log with the verbatim GTK warning.
- The heading renders in `en` and `vi`; no Pango warning on opening About.
- Regression test fails before the fix and passes after; both ctest suites pass.
