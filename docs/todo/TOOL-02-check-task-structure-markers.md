# TOOL-02 — `check-task-structure.js` marker regexes skip `🔲` open items

**Status:** ✅ FIXED — Widened both regexes to accept 🔲 and 🚧 markers; TOOL-02 and PROTO-03 now recognized; orphan detection unchanged; check-tracking-sync.js --full still passes (no regression)
**Area:** `scripts/check-task-structure.js` (`BULLET_START_RE`, `TODO_LINE_RE`)
**Priority:** P3
**Source:** Surfaced 2026-09-04 while scaffolding ANLZ-03 — an open Backlog item with a detail file tripped the orphan check. Filed to Backlog same day; pulled into Sprint 13 Active 2026-09-06.
**Design:** none — scoped directly
**Depends on / relates to:** `check-tracking-sync.js` (the sprint-command gate — unaffected, already passes); `.claude/rules/tracking-files.md` (the sync rule these scripts enforce)

## Problem

`scripts/check-task-structure.js` complements `check-tracking-sync.js` by catching layout problems
(duplicate CODEs, index lines with no detail file, orphan detail files). Its two line-matching
regexes only recognise an optional leading `✅` marker or no marker at all:

```js
const BULLET_START_RE = /^-\s*(✅\s*)?\*\*([A-Za-z0-9-]+)\.\*\*/;
const TODO_LINE_RE     = /^-\s*(✅\s*)?\*\*([A-Za-z0-9-]+)\.\*\*.*\(docs\/todo\/\2-([^)]+)\.md\)/;
```

A `TODO.md` line that leads with the `🔲` open-marker (`- 🔲 **CODE.** …`) — the canonical format
for an open Backlog or Active item per `.claude/rules/tracking-files.md` — does not match, so the
script silently skips the line. An open item that *does* have a `docs/todo/<CODE>-*.md` detail file
is then falsely reported as an orphaned detail file (index line "missing"). The in-progress `🚧`
marker has the same gap.

## Scope (in order)

1. Extend the marker alternation in both `BULLET_START_RE` and `TODO_LINE_RE` to accept `🔲` and
   `🚧` in addition to `✅` (and still no marker). Keep the CODE capture group index stable so the
   `\2` backreference in `TODO_LINE_RE` and all downstream `match[N]` uses are unchanged — widen the
   existing optional group (e.g. `(✅\s*|🔲\s*|🚧\s*)?`) rather than adding a new group.
2. Scan the rest of the script for any other place that assumes the `✅`-or-nothing shape (status
   tallies, per-section counts).
3. Run `node scripts/check-task-structure.js` against the current tree and confirm it now passes
   with the open `🔲` items present (TOOL-02 itself, PROTO-03).

## Acceptance criteria

- `🔲`-marked open items that have detail files (TOOL-02, PROTO-03) are no longer falsely reported
  as orphans by `node scripts/check-task-structure.js`.
- A genuinely orphaned detail file (no index line at all) is still reported.
- A genuine duplicate CODE is still reported.
- `node scripts/check-tracking-sync.js --full` still exits 0 (no regression to the other script).

> **Amended 2026-09-06 during `/implement-task`:** the original wording "`check-task-structure.js`
> exits 0 on the current `TODO.md`" is not met — the `⛔`-marked, non-canonically-formatted
> ANLZ-03 lines (`- ⛔ **ANLZ-03. SUPERSEDED** …` in `TODO.md`; `## ANLZ-03 — … — ⛔ SUPERSEDED`
> in `instruction.md`) still trip the script, and handling the `⛔` / SUPERSEDED shape is outside
> TOOL-02's written scope (`🔲` / `🚧` only). Split out as follow-up **TOOL-03**.

## Scope boundary

- Do not change `check-tracking-sync.js` — it only checks `✅` done-marker sync and is working.
- Do not change `TODO.md` line formatting or `.claude/rules/tracking-files.md` — the marker format
  is correct; the script is what's behind.
- Do not add new structural checks — this is a regex-coverage fix only.
- No test-infrastructure work: these are standalone Node scripts with no existing test harness;
  verification is the manual runs in the acceptance criteria.
