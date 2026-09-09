# Current sprint

## Sprint 19

**Goal:** YXANALZ — native `!yxAnalz`: analyze an explicit root-move allow-list

**Dates:** 2026-09-09 to — (open — no fixed end date set yet)

**Dependency graph:**

- **PROTO-07** — `src/command/command_dispatcher.{cpp,h}` + `src/engine/engine_controller.{cpp,h}`
  + `src/engine/gomocup_protocol.{cpp,h}` (one new send helper). Feature, **not** a `.ptc` command
  (PROTO-03 can't carry the multi-line display, the analysis state, or alphabetic coords). Design
  resolved with the user 2026-09-09 — the 8 decisions are in `docs/todo/PROTO-07-*.md` "Resolved
  design"; do **not** re-open them. Route A: `!yxAnalz h3 h2 h9` (inline, `parseMovesText`) →
  `EngineController::analyzeMoves()` mirroring `analyze()`'s `EngineState`/`SearchIntent::Analysis`
  → new `GomocupProtocol::generateAnalyzeMovesRequest` sends `YXBOARD <path> DONE` + `YXANALZ
  <moves> DONE` via `coordToEngine()` for every coord. Completion coordinate discarded (no stone
  placed — ANLZ-06 gate). Refused while Analyze Mode is ON. Rendering reuses PROTO-05/06 unchanged.
  Must not touch: `parseLine` ordering / PROTO-01 bounds, `parseMovesText` behaviour,
  `protocol_extension.*`, the `YXNBEST` request path, `parseInfo`/`parseMessage`/`parseRealtimePV`/
  `onPVDone`. No `systematic-debugging` needed (new feature, not a bug). Regression tests per
  `docs/instruction/PROTO-07-*.md` "Verification before done".

- **CONS-03** (pulled into Active mid-sprint 2026-09-09, follow-on to PROTO-07) —
  `src/command/command_dispatcher.cpp` only. Help text: enrich the `!yxAnalz` `CommandSpec`
  `summary` (prefer over `usage` — `test_proto07_yxanalz_console.cpp` pins
  `commandUsage("yxanalz")` exactly) and/or one `printHelp()` preamble line, so `!help` conveys
  that `YXANALZ` is an engine-specific protocol extension, a real interruptible search, places no
  stone, and takes alphabetic move text only. No behaviour change: `parseMovesText`,
  `analyzeMoves`, `generateAnalyzeMovesRequest`, the raw-passthrough path — all untouched. No new
  command / group / `README.md` reference / numeric-coord support. No `systematic-debugging`
  (doc change). Verify per `docs/instruction/CONS-03-*.md`.

| CODE | Summary | Depends on | Points | Status |
|---|---|---|---|---|
| PROTO-07 | native `!yxAnalz` console command — analyze an explicit root-move allow-list | — | — | ✅ Done — merged to `main` 2026-09-09 (PR #34, squash `2a912fe`) |
| CONS-03 | `!help` should document `!yxAnalz` / YXANALZ as an engine-protocol extension | PROTO-07 | — | ✅ Done — merged to `main` 2026-09-09 (PR #35, squash `4c5519b`) |

Points not yet estimated (consistent with Sprints 3–18).

**Lesson carried in from Sprint 18:**

- **The build host has no engine binary and no display.** "Run `!yxAnalz` against a real Rapfi and
  watch the per-move PVs render" is a human acceptance step; the automated regression must replay
  constructed engine lines and assert real state (wire output, search-state transition, inbound
  completion-coord suppressed), not "the code path exists".
- **Verify each engine stream a stock Rapfi actually emits before building on it.** PROTO-07 reuses
  the PROTO-05/06 `INFO PV` / `REALTIME` pipeline — confirm `YXANALZ` drives the same lines
  `YXNBEST` does (it should; it *is* `YXNBEST` with a fixed root list) before assuming the panel
  and overlay light up unchanged.
- **A second/parallel model→ui path stays minimal.** `analyzeMoves()` is a sibling of `analyze()`,
  not a new subsystem — reuse `SearchIntent::Analysis`, the existing tick, the existing render.

See `docs/sprint/burndown.md` for the daily remaining-points table, and `docs/sprint/archive/` for
closed sprints. Starting the next sprint = one edit per `/CLAUDE.md` ("Sprint cadence").
