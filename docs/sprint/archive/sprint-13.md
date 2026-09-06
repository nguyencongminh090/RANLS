# Sprint 13 (closed 2026-09-06)

**Goal:** Open Protocol Extension (`.ptc` runtime commands) plus tracking-tool marker fix
**Dates:** 2026-09-06 to 2026-09-06.

## Final state — all items shipped

| CODE | Summary | Status |
|---|---|---|
| TOOL-02 | `check-task-structure.js` regexes skip `🔲`/`🚧` open items — false orphan reports | ✅ FIXED |
| PROTO-03 | Open Protocol Extension: user-defined `.ptc` runtime commands for Gomocup-family engines | ✅ DONE |

No mid-sprint pulls. Points not estimated (consistent with Sprints 3–12).

## What shipped

- **TOOL-02** (PR #21, squash `7ceeeed`): `BULLET_START_RE` / `TODO_LINE_RE` in
  `scripts/check-task-structure.js` widened to accept `🔲` (open) and `🚧` (in-progress)
  markers alongside `✅` and no-marker; capture-group indices unchanged. Open items with a
  detail file (TOOL-02, PROTO-03 themselves) are no longer falsely reported as orphans;
  orphan and duplicate-code detection unchanged; `check-tracking-sync.js --full` still clean.
  Standalone Node script, no test harness — verified by the manual acceptance-criteria runs.
  The `⛔`/SUPERSEDED-line case (ANLZ-03) was outside TOOL-02's written `🔲`/`🚧` scope and
  was split out to **TOOL-03** (Backlog).

- **PROTO-03** (PR #22, squash `75245d9`): Open Protocol Extension — a `.ptc` (TOML) file
  lets an engine developer add console-triggered commands with no C++ change or YixinBoard
  rebuild. New standalone `src/engine/protocol_extension.{h,cpp}`: a hand-rolled strict
  TOML-subset parser (chosen over vendoring toml++ to keep the trust boundary minimal and
  fully covered by malformed-input tests — no `third_party/` added), a fail-closed
  name-collision check against the full 18-name built-in command registry (Q3), Q5 resource
  limits, and a recursive-descent parser + bounded interpreter for the `if/elif/else` reply
  DSL (Q2) with 3 whitelisted sinks via a dispatch table. New minimal `ICustomCommandSource`
  interface (deliberately **not** a method on `IEngineProtocol`), implemented by
  `GomocupProtocol` with built-in parsing running first and unchanged — extension `on_reply`
  patterns are consulted only on no built-in match. `EngineController::loadExtensionTable()`
  (load-once at start/reload; `customSource_` is null on a `.ptc`-less run),
  `sendCustomCommand()`, `signal_custom_action`. `CommandDispatcher::syncExtensionCommands()`
  registers into the same `!` command namespace with an `[extension]` help group; the
  declared `group` is informational only in v1 (Q9). UI sinks reuse existing widgets:
  `set_status_field` → a dynamic `Gtk::Label` box in `EngineStatusView` (the 6 fixed members
  untouched); `toast` → the crash-banner widget; `log` → `EngineLogModel`. No
  `BoardRenderer` / `BoardViewModel` change (`highlight_cell` dropped from v1);
  `group = "analysis"` real-search integration explicitly out of scope. `EngineConfig::protocolExtensionPath`
  persisted; a `*.ptc` file-chooser row added to Settings. Regression tests:
  `tests/test_proto03_protocol_extension.cpp` (14 cases). Build clean (only the 3 known
  pre-existing `-Wunused-function` warnings); `ranls-gui-tests` 209/209 cases / 2466
  assertions; `ranls-gui-ui-tests` 25/26 (the 1 failure `test_anlz05_no_automove_action`
  reproduces identically on branch base `8b010ed` — pre-existing host limitation, not a
  regression). Live-engine / display "Manual smoke" tier still pending a human (no engine
  binary or display server on the build host).

## Lessons

- The build host has no engine binary and no display server — live-engine and
  display-dependent behaviour cannot be verified here. This is a standing, expected gap on
  every sprint's release checklist (carried from Sprint 12); PROTO-03's end-to-end
  `.ptc` → sink assertions were driven by fake engine lines in the test harness, with the
  real-engine smoke pass documented as an outstanding human step.
- "Analysis-intent vs. move-intent" remains a recurring seam in `EngineController` (carried
  from Sprint 12). PROTO-03 added a new `signal_custom_action` path through this file; it was
  verified **not** to need `SearchIntent` keying because v1 excludes `group = "analysis"` —
  the check was done rather than assumed, per the carried-in lesson.
- A hand-rolled strict parser for an untrusted input format (the `.ptc` TOML subset), fully
  covered by malformed-input tests, kept the trust boundary smaller than vendoring a full
  TOML library would have — a reusable pattern when the input surface is small and
  security-sensitive.

## Rolled over to Backlog

Nothing rolled over — both committed items finished.

## Next sprint

Sprint 14 — goal "Backlog tooling fix + Tier-1 Windows portability" (TOOL-03, PORT-01).
Run `/sprint open 14 …` to commit them. Release `v0.4.0` cut at this close (MINOR bump —
PROTO-03 is a new user-visible feature).
