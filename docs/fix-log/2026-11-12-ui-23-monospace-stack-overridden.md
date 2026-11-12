# UI-23 monospace font stack overridden for `.monospace` widgets (2026-11-12)

**Status:** Active
**Related:** UI-23 (follow-up, in-branch, unmerged)

## Prompt

Found while instrumenting UI-24: the Engine Log TextView's resolved font was `monospace 12.267px`.
UI-23's 0.92em size applied, but its monospace font stack did not.

## Root cause

`src/resources/style.css` kept a pre-UI-23 rule `.monospace { font-family: monospace; }` further down
the file than UI-23's `.monospace, textview { font-family: "JetBrains Mono", … }`. Both have the same
specificity at the same provider priority, so the later one won. A probe with the real stylesheet showed
it: a plain TextView (Move Log) resolved to `JetBrains Mono,…,monospace`, while a `.monospace` TextView
(Engine Log) and a `.monospace` Label (console rows) resolved to `monospace`. A one-rule stylesheet
`.monospace { font-family: "Hack" }` applied correctly, which ruled out a theme or priority problem.

## Fix

Deleted the leftover rule (a comment marks where it stood). The class is now styled only in the "Type
roles" block.

## Verification

New `tests/test_ui23_monospace_stack.cpp` (loads the real `style.css`): plain TextView, `.monospace`
TextView and `.monospace` Label must all resolve to the stack. It failed before (2/3 resolved to `monospace`)
and passes after. `ctest` 5/5. Live Engine Log screenshot renders in Hack (first installed font in the
stack on this host); the UI-24 gutter still fits with a gap; no GTK warnings.

## Knowledge

`systematic-debugging` skill; `.claude/rules/bugfix.md`.
