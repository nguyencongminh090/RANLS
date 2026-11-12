# UI-24 — Engine Log gutter clips / crowds the "MESSAGE" tag

**Status:** ✅ FIXED (2026-11-12, branch `ui-24/engine-log-gutter-tag-clipped`; Sprint 27, pulled in on user request) — see [fix-log](../fix-log/2026-11-12-ui-24-engine-log-gutter-tag-clipped.md).
**Area:** `src/ui/bottom_panel.cpp` (`BottomPanel::drawGutter`), `src/resources/style.css` (`.engine-gutter`)
**Priority:** P3
**Source:** found during UI-23 live verification, 2026-11-11 (before/after screenshots of the Engine Log tab)
**Design:** none — scoped directly
**Depends on / relates to:** UI-05 (gutter), UI-23

## Problem

The gutter tag column is sized once as `width("MESSAGE") + 12` in the TextView's font, with the tag
drawn at x=6. On the baseline (before UI-23) the last letter of "MESSAGE" is visibly cut. After
UI-23's monospace stack it fits but touches the log text with no gap. The sizing likely ignores the
`.engine-gutter` CSS padding/border (`padding: 0 4px; border-right: 1px`). (Confirmed 2026-11-12: request 64px → content 55px.)

## Scope (in order)

1. Root-cause with the systematic-debugging pipeline (measure gutter allocation vs request vs probe width).
2. Fix the sizing so the longest tag has a visible gap before the log text at any font size.
3. Regression test (UI test: gutter allocated width ≥ longest tag ink width + gap).

## Scope boundary

- Do not change the gutter-as-sibling design (UI-05), the tag set, colours, or log flushing (RT-02).

## Reasoning

Cosmetic but on a surface users read constantly while analysing. Kept out of UI-23 (pre-existing,
not caused by it; needs its own root cause).

## Knowledge

`.claude/rules/bugfix.md`; `docs/fix-log` UI-05/UI-10 entries for the gutter design.

## Acceptance criteria

- "MESSAGE" fully visible with ≥ 4px gap to the log text in en, default and 14pt fonts; test fails before the fix.
