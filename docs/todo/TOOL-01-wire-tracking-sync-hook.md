# TOOL-01 — Wire `check-tracking-sync.js` as a `Stop` hook

**Status:** ✅ DONE
**Area:** tooling / `.claude/settings.local.json`
**Priority:** P4
**Source:** `docs/notes/2026-08-20-workspace-setup.md`, re-surfaced during a 2026-08-30 leftover-task sweep
**Verification:** Stop hook wired to `.claude/settings.local.json` (2026-08-30). Hook verified working via manual run of `node scripts/check-tracking-sync.js --hook`.

## Context

`scripts/check-tracking-sync.js` exists and works standalone
(`node scripts/check-tracking-sync.js --full`), enforcing the "Index/detail sync" rule from
`.claude/rules/tracking-files.md`. It has never been wired as an automatic `Stop` hook — a prior
attempt during initial workspace setup was blocked by the auto-mode permission classifier (editing
hook config that runs commands automatically needs explicit user action, not an agent-driven edit),
and the user chose to skip it at the time rather than add it manually.

## Scope boundary

- **This item cannot be completed by an agent editing `.claude/settings.local.json` directly** —
  same permission constraint as the original attempt. The actual edit needs the user's explicit
  action (or explicit go-ahead in a session where that's grantable).
- The exact snippet to add is already documented in `.claude/rules/tracking-files.md`:
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
- Pickup for whoever/whatever takes this: confirm with the user they still want it wired, then add
  the snippet (merging with any existing `hooks` block rather than overwriting it).

## Acceptance criteria

- `.claude/settings.local.json` has the `Stop` hook wired.
- A newly-✅-marked `TODO.md` item with no matching detail-file completion marker blocks session end
  with the hook's `permissionDecisionReason` message.

## Related

- `.claude/rules/tracking-files.md` "Automated enforcement" section.
