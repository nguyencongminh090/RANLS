# TOOL-03 — `check-task-structure.js` now recognizes `⛔` SUPERSEDED markers

**Date:** 2026-09-06

## Problem

After TOOL-02 (which added `🔲`/`🚧` markers to the regex patterns), `node scripts/check-task-structure.js` still exited with errors on the ANLZ-03 superseded item:

```
docs/todo/ANLZ-03-persist-winrate-in-save-file.md: not linked from any TODO.md line (orphan)
instruction.md:147: CODE "ANLZ-03" has no matching entry in TODO.md
```

ANLZ-03 was marked with `⛔` (not recognized) and formatted as `**ANLZ-03. SUPERSEDED**` (the ` SUPERSEDED` text inside the bold span prevented CODE capture with the original rigid `.**` pattern).

## Solution

Modified `scripts/check-task-structure.js` regexes at lines 30–32:

**Before:**
```javascript
const BULLET_START_RE = /^-\s*(✅\s*|🔲\s*|🚧\s*)?\*\*([A-Za-z0-9-]+)\.\*\*/;
const TODO_LINE_RE = /^-\s*(✅\s*|🔲\s*|🚧\s*)?\*\*([A-Za-z0-9-]+)\.\*\*.*\(docs\/todo\/\2-([^)]+)\.md\)/;
```

**After:**
```javascript
const BULLET_START_RE = /^-\s*(✅\s*|🔲\s*|🚧\s*|⛔\s*)?\*\*([A-Za-z0-9-]+)\.(?:\s+SUPERSEDED)?\*\*/;
const TODO_LINE_RE = /^-\s*(✅\s*|🔲\s*|🚧\s*|⛔\s*)?\*\*([A-Za-z0-9-]+)\.(?:\s+SUPERSEDED)?\*\*.*\(docs\/todo\/\2-([^)]+)\.md\)/;
```

**Changes:**
1. Added `⛔\s*` to the marker alternation (both patterns)
2. Added `(?:\s+SUPERSEDED)?` non-capturing group to optionally match ` SUPERSEDED` between the CODE and the closing `**`
3. Capture group indices remain stable: CODE is group 2, slug is group 3; backreference `\2` unchanged

## Verification

All acceptance criteria met:

1. **`node scripts/check-task-structure.js`** exits 0 with "OK: TODO.md / instruction.md structure is consistent."
2. **`node scripts/check-tracking-sync.js --full`** exits 0 (full tracking sync audit passes)
3. Orphan detection and duplicate-code detection remain unchanged and fully functional
4. ANLZ-03 is now correctly recognized as a valid superseded item (not an orphan, not a mismatched heading)

## Scope

- Script modification only — no changes to ANLZ-03 index lines in TODO.md or instruction.md
- `check-tracking-sync.js` left completely untouched
- No new structural checks added, only improved regex tolerance
