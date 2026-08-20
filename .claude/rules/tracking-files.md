---
paths:
  - "TODO.md"
  - "docs/todo/**"
  - "instruction.md"
  - "docs/instruction/**"
  - "docs/fix-log.md"
  - "docs/fix-log/**"
  - "docs/audit.md"
  - "docs/audit/**"
---

# Tracking-file layout: index + detail files (query and append)

`TODO.md`, `instruction.md`, `docs/fix-log.md`, and `docs/audit.md` are lightweight **indexes** —
structural headings plus one line per item linking to a detail file. Read the index first (it's
small); actual content lives one level down, one file per item:

- `docs/todo/<CODE>-<slug>.md` — `CODE` is the item's code as used in `TODO.md` (e.g. `WALL-01`).
- `docs/instruction/<CODE>-<slug>.md` — `CODE` matches the `instruction.md` heading.
- `docs/fix-log/<YYYY-MM-DD>-<slug>.md` — one file per fix-log row, named by date + opening-words slug.
- `docs/audit/<YYYY-MM-DD>-<slug>.md` — one file per audit row, same naming.

**Query:** grep/scan the index for the item code/keyword, then `Read` only the matched detail
file(s) instead of the whole original file.
**Append:** write one new detail file, then add one new line/row to the matching index. Never re-open
or edit an existing detail file's content when adding unrelated history.

## Index/detail sync: status markers move together, in one edit

`TODO.md` marks a finished item with a leading `✅` on its index line; the matching
`docs/todo/<CODE>-<slug>.md` marks the same fact in its own completion marker. These describe the
same fact and must never be updated one without the other:

- **Finishing a task = one edit that touches both files**, same turn. Neither file is "the real
  one" — an index line without a matching detail-file marker, or vice versa, is a drift bug.
- **Canonical marker format for entries**: `**Status:** ✅ <verb>`, where `<verb>` is one of `DONE`
  (implemented), `FIXED`, `CLOSED` (won't-fix/not-a-bug), or `VERIFIED` (measured/confirmed) — pick
  whichever matches what actually happened, then add a short summary + test/verification notes on
  the same or following lines.
- **Before telling the user a task is/isn't done, read both.** If they disagree, bring the index in
  line with the detail file (it carries the evidence — test output, verification notes).

### Automated enforcement

`scripts/check-tracking-sync.js` exists and works, but is **not yet wired as a `Stop` hook** — a
prior attempt to add it to `.claude/settings.local.json` was blocked by the auto-mode permission
classifier (editing hook config needs explicit user action). To audit the backlog manually, run:
```
node scripts/check-tracking-sync.js --full
```
To enable automatic enforcement (blocks ending a turn if a newly-✅-marked `TODO.md` item's detail
file has no completion marker — only checks drift introduced since the last commit), add this to
`.claude/settings.local.json` yourself:
```json
{
  "hooks": {
    "Stop": [
      { "hooks": [ { "type": "command",
        "command": "node \"${CLAUDE_PROJECT_DIR}/scripts/check-tracking-sync.js\" --hook",
        "timeout": 15 } ] }
    ]
  }
}
```

`instruction.md` ↔ `docs/instruction/*.md` has no done/not-done marker (execution guidance, not a
status tracker) — nothing to sync there beyond "read the matching entry before implementing."

## `docs/fix-log.md` and `docs/audit.md`: append-only, every row timestamped

- A new fix/audit entry = one new detail file (`## Prompt` / `## Action` / `## Decision` /
  `## Summary` as relevant) + one new row in the matching index. Never edit, reword, reorder, or
  delete an existing row/file. A wrong past entry gets a new correcting entry, not a rewrite.
- Every index row's timestamp/date matches its detail file's heading, set to the real wall-clock
  time the entry is *written*, not when the underlying event happened if those differ.
