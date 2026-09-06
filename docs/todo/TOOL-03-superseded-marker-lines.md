# TOOL-03 — `check-task-structure.js` trips on `⛔` SUPERSEDED index lines

**Status:** ✅ DONE — Modified regexes in `scripts/check-task-structure.js` to recognize `⛔` marker and ` SUPERSEDED` text. All acceptance criteria met: `check-task-structure.js` exits 0, `check-tracking-sync.js --full` exits 0.
**Area:** `scripts/check-task-structure.js` (`BULLET_START_RE`, `TODO_LINE_RE`, `INSTRUCTION_HEADING_RE`) and/or the ANLZ-03 index lines in `TODO.md` + `instruction.md`
**Priority:** P3
**Source:** Split out of TOOL-02 during `/implement-task` 2026-09-06 — TOOL-02 widened the marker set to `🔲`/`🚧` but its scope explicitly excluded `⛔`. `node scripts/check-task-structure.js` still exits 1.
**Design:** none — needs a small decision (see below), then scoped directly
**Depends on / relates to:** TOOL-02 (done); `.claude/rules/tracking-files.md` (canonical marker set); `check-tracking-sync.js` (unaffected — must stay untouched)

## Problem

After TOOL-02, `node scripts/check-task-structure.js` still reports 2 problems:

```
docs/todo/ANLZ-03-persist-winrate-in-save-file.md: not linked from any TODO.md line (orphan)
instruction.md:147: CODE "ANLZ-03" has no matching entry in TODO.md
```

ANLZ-03 was superseded by RDB-01/02/03 (user decision 2026-09-04) and its index lines were left
in a non-canonical shape:

- `TODO.md:74` — `- ⛔ **ANLZ-03. SUPERSEDED** by RDB-01/02/03 …` — leads with `⛔` (not in the
  regex alternation) **and** puts ` SUPERSEDED` inside the `**…**` bold span, so even with `⛔`
  added the `**([A-Za-z0-9-]+)\.**` CODE capture would not match.
- `instruction.md:147` — `## ANLZ-03 — persist-winrate-in-save-file — ⛔ SUPERSEDED by RDB-01/02/03`
  — `INSTRUCTION_HEADING_RE` captures `ANLZ-03` fine, but there is no matching `TODO.md` entry the
  script recognises, so it flags the heading.

## Decision needed (resolve with the user before scoping)

Two ways to make the script pass, pick one:
1. **Teach the script the `⛔`/SUPERSEDED shape** — add `⛔` to the marker alternation and accept a
   trailing ` SUPERSEDED` token before the `.**`, and treat a `⛔`-marked CODE as legitimately
   present (not an orphan). More permissive regexes.
2. **Normalise the ANLZ-03 lines** to canonical form (`- ⛔ **ANLZ-03.** SUPERSEDED by …` with the
   marker word outside the bold; matching instruction heading) and add just `⛔` to the alternation.
   Keeps the regexes strict; touches `TODO.md` / `instruction.md` wording (allowed — doc-only).

## Acceptance criteria

- `node scripts/check-task-structure.js` exits 0 on the current tree.
- A genuine orphan detail file and a genuine duplicate CODE are still reported.
- `node scripts/check-tracking-sync.js --full` still exits 0.

## Scope boundary

- Do not change `check-tracking-sync.js`.
- Do not re-litigate the ANLZ-03 supersession itself — only its line *format* / the script's
  tolerance of it.
- Regex-coverage / line-normalisation only; no new structural checks.
