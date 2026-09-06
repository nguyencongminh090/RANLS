# Current sprint

## Sprint 13

**Goal:** Open Protocol Extension (`.ptc` runtime commands) plus tracking-tool marker fix

**Dates:** 2026-09-06 to — (open — no fixed end date set yet)

**Dependency graph:**

- **TOOL-02** — repo tooling only (`scripts/check-task-structure.js`). Widen `BULLET_START_RE` /
  `TODO_LINE_RE` to accept the `🔲` and `🚧` markers alongside `✅` so open items with a detail file
  stop being flagged as orphans. Must **not** touch `check-tracking-sync.js`, `TODO.md` line
  formatting, or `.claude/rules/tracking-files.md`. No `/systematic-debugging` needed (known cause,
  quoted in the detail file). No design call. Standalone Node script, no test harness — verified by
  the manual runs in the acceptance criteria. Independent of PROTO-03.
- **PROTO-03** — engine layer, large feature; design fully resolved in
  `features/protocol-extension/` (Q1–Q9 accepted 2026-09-06). New `src/engine/protocol_extension.{h,cpp}`
  (`.ptc` TOML loader + bounded `if/elif/else` DSL interpreter, 3 whitelisted sinks), new minimal
  `ICustomCommandSource` interface reached via `dynamic_cast`, `EngineController::sendCustomCommand()`
  + `signal_custom_action`, `CommandDispatcher` `!`-namespace registration, `EngineConfig` /
  `SettingsDialog` `.ptc` path field, one new dynamic-row widget in `EngineStatusView`. Must **not**
  modify `IEngineProtocol`, `EngineProcess`, or any existing Gomocup parsing/hardening (PROTO-01,
  PROTO-02); **no** `BoardRenderer` / `BoardViewModel` change (`highlight_cell` sinks dropped from
  v1); `group = "analysis"` real-search integration explicitly **out of scope**. Load the
  `software-architecture` skill before implementing — this touches four layers; consider a
  `feat/protocol-extension` integration branch + sub-task split at `/implement-task` time given the
  8-item scope. Independent of TOOL-02.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| TOOL-02 | `check-task-structure.js` regexes skip `🔲`/`🚧` open items — false orphan reports | — | — | ✅ Done — PR #21, squash `7ceeeed` (2026-09-06). `⛔`/SUPERSEDED-line handling split to TOOL-03 (Backlog). |
| PROTO-03 | Open Protocol Extension: user-defined `.ptc` runtime commands for Gomocup-family engines | — | — | ✅ Done — branch `proto-03/open-protocol-extension` (2026-09-06). All 8 build-order steps; `ranls-gui-tests` 209/209 (14 new PROTO-03 cases); live-engine smoke still pending a human. |

Points not yet estimated (consistent with Sprints 3–12).

**Lessons carried in from Sprint 12:**

- "Analysis-intent vs. move-intent" is a recurring seam in `EngineController` — PROTO-03 adds a new
  signal/command path through this file, so check whether `signal_custom_action` needs to key off
  `SearchIntent` (it should not, since v1 excludes `group = "analysis"` — but verify, don't assume).
- The build host has no engine binary and no display server — live-engine / display-dependent
  behavior cannot be verified here. PROTO-03's end-to-end `.ptc` → sink assertions must be driven by
  fake engine lines in the test harness; a real-engine smoke pass stays a documented human step.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
