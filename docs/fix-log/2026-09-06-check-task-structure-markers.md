# 2026-09-06 — check-task-structure.js marker regex fix

## Prompt

TOOL-02: Extend `scripts/check-task-structure.js` marker regexes to accept `🔲` (open) and `🚧` (in-progress) markers in addition to `✅` (done) and no marker. Widen both `BULLET_START_RE` and `TODO_LINE_RE` to include the new markers in the alternation; preserve capture group indices so `TODO_LINE_RE`'s `\2` backreference and downstream `match[N]` uses remain unchanged.

## Action

Edited `scripts/check-task-structure.js`:
- Line 30: `BULLET_START_RE` from `/^-\s*(✅\s*)?\*\*([A-Za-z0-9-]+)\.\*\*/` to `/^-\s*(✅\s*|🔲\s*|🚧\s*)?\*\*([A-Za-z0-9-]+)\.\*\*/`
- Line 32: `TODO_LINE_RE` from `/^-\s*(✅\s*)?\*\*([A-Za-z0-9-]+)\.\*\*.*\(docs\/todo\/\2-([^)]+)\.md\)/` to `/^-\s*(✅\s*|🔲\s*|🚧\s*)?\*\*([A-Za-z0-9-]+)\.\*\*.*\(docs\/todo\/\2-([^)]+)\.md\)/`

Capture group indices unchanged: group 1 = optional marker, group 2 = CODE, group 3 = slug (in TODO_LINE_RE only). The backreference `\2` continues to target group 2 (the CODE).

Scanned the rest of the script for other places assuming the old `✅`-or-nothing shape — found none. The script only captures markers and checks structural consistency; no status tallies or per-section counts.

## Summary

**Acceptance criteria verification (all manual, per spec — no test harness):**

1. **Regex now accepts new markers:** Verified via direct regex test:
   - `- 🔲 **TOOL-02.** ... — [detail](docs/todo/TOOL-02-check-task-structure-markers.md)` ✓ Matches
   - `- 🔲 **PROTO-03.** ... — [detail](docs/todo/PROTO-03-open-protocol-extension.md)` ✓ Matches
   - `- ✅ **TEST-01.** ... — [detail](docs/todo/TEST-01-test-infrastructure.md)` ✓ Matches (backwards-compat)

2. **Orphan detection still works:** Created temporary `docs/todo/ZZZ-99-throwaway.md` and ran `node scripts/check-task-structure.js` → correctly reported as orphan, then deleted. ✓

3. **Duplicate code detection unchanged:** Logic at lines 65–69 (TODO.md) and 97–101 (instruction.md) untouched; dedup logic verified still present. ✓

4. **No regression in check-tracking-sync.js:** Ran `node scripts/check-tracking-sync.js --full` → no output (exit 0). ✓

**Current repo state:** The tree has a pre-existing issue (ANLZ-03 marked with `⛔ SUPERSEDED` marker, not supported by any regex) unrelated to this task. TOOL-02 and PROTO-03 (with 🔲 markers) are now recognized and not falsely reported as orphans.

**Note:** These are standalone Node scripts with no test harness. Verification is by manual acceptance-criteria runs only — no automated regression tests exist or were added.
