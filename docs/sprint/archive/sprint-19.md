# Sprint 19 (closed 2026-09-10)

**Goal:** YXANALZ — native `!yxAnalz`: analyze an explicit root-move allow-list
**Dates:** 2026-09-09 to 2026-09-10.

## Final state — all items shipped (2 of 2)

| CODE | Summary | Status |
|---|---|---|
| PROTO-07 | native `!yxAnalz` console command — analyze an explicit root-move allow-list | ✅ DONE |
| CONS-03 | `!help` documents `!yxAnalz` / YXANALZ as an engine-protocol extension | ✅ DONE |

Sprint opened 2026-09-09 with PROTO-07 as its sole commitment; CONS-03 (a help-text follow-on)
was filed and pulled Backlog → Active mid-sprint the same day, immediately after PROTO-07 merged.
Points not estimated, consistent with Sprints 3–18.

## What shipped

- **PROTO-07** (PR #34, squash `2a912fe`): native `!yxAnalz <moveText...>` console command driving
  Rapfi's `YXANALZ` explicit root-move allow-list — a first-class feature, deliberately **not** a
  `.ptc` extension (PROTO-03 can't carry the multi-line PV display, the analysis-state transition,
  or alphabetic coords). One new send helper `IEngineProtocol::generateAnalyzeMovesRequest(path,
  moves)` (impl. in `GomocupProtocol`) emits `YXBOARD` / `<coord>,<color>` … / `DONE` / `YXANALZ` /
  `<coord>` … / `DONE`, position block byte-identical to `generateAnalyzeRequest`'s, every
  coordinate through the single `coordToEngine()` point, one per line. New
  `EngineController::analyzeMoves()` mirrors `analyze()`'s `SearchIntent::Analysis` +
  `setAnalyzing(true)` + `EngineState::Analyzing` bookkeeping inside the PROTO-04 `sendOrDefer()`
  gate; two deliberate differences — an empty list never reaches the wire, and a running search is
  stopped first (not early-returned), with `pendingStopFlush_` queuing the new block behind the
  teardown. The trailing completion coordinate needed **no new code**: the existing ANLZ-06
  `SearchIntent` gate already discards it under an Analysis intent (no stone placed), now pinned by
  an inbound-coordinate test. `CommandDispatcher` registers `!yxAnalz` in the `analysis` help
  group, parsing via the shared `parseMovesText` (unmodified); refuses with a message and **no
  send** when the engine is not running, Analyze Mode is ON, the list is empty, or every listed
  point is off-board/occupied (occupied points inside a valid list are skipped with a note).
  Rendering verified unchanged — `YXANALZ` drives the same `INFO PV` / `REALTIME` lines as
  `YXNBEST` and the gating keys on `isAnalyzing()`, not `analyze()`. Regression tests:
  `tests/test_proto07_yxanalz.cpp` (9 cases, `ranls-gui-tests`) +
  `tests/test_proto07_yxanalz_console.cpp` (7 cases, `ranls-gui-ui-tests`, display-free). `ctest`
  4/4 green. **Known deviation:** numeric `x,y` console input (`7,7`) is *not* implemented —
  `parseMovesText` is alphabetic-only and out of bounds to change; flagged for the user rather than
  silently extended (file a new `CODE` if wanted). Live-engine smoke against a real Rapfi_V2 is
  human-owed (no engine binary / no display on the build host). Detail:
  `docs/fix-log/2026-09-09-proto-07-yxanalz.md`.

- **CONS-03** (PR #35, squash `4c5519b`): `!help` now documents `!yxAnalz` / `YXANALZ` as an
  engine-protocol extension. Help-text only — enriched the `!yxAnalz` `CommandSpec` **`summary`**
  (not `usage` — `test_proto07_yxanalz_console.cpp` pins `commandUsage("yxanalz")` exactly) to
  one denser line conveying the protocol-extension dependency, the interruptible real-search
  nature, no-stone-placed, and alphabetic-moves-only input. One-line diff in
  `src/command/command_dispatcher.cpp`; no behaviour change (`parseMovesText`, `analyzeMoves`,
  `generateAnalyzeMovesRequest`, the raw-passthrough path all untouched), no new command / group,
  no `README.md` / `CHANGELOG.md` churn (the PROTO-07 `[Unreleased]` entry already covers
  `!yxAnalz` for users). Doc-only static string — no new regression test; the existing
  `test_proto07_yxanalz_console.cpp` "registered in analysis group" case already scans `!help` for
  the command. `/implement-task` dispatch (Haiku, isolated worktree); orchestrator re-verified
  independently on the branch — clean build, `ctest` 4/4. Detail:
  `docs/fix-log/2026-09-09-cons-03-help-doc-yxanalz.md`.

## Lessons

- **The build host has no engine binary and no display.** Carried in from Sprint 18 and still
  load-bearing: "run `!yxAnalz` against a real Rapfi and watch the per-move PVs render" is a human
  acceptance step. PROTO-07's automated regression replayed constructed engine lines and asserted
  real state — wire output byte-shape, the `EngineState` / `SearchIntent` transition, and the
  inbound completion coordinate being suppressed — not "the code path exists". Keep carrying this.
- **Verify the reused stream before building on it.** PROTO-07 assumed `YXANALZ` drives the same
  `INFO PV` / `REALTIME` pipeline as `YXNBEST` (it *is* `YXNBEST` with a fixed root list) and that
  assumption was checked against Rapfi's documented output before the panel/overlay reuse was
  taken as free. Same discipline as PROTO-06's decision (a).
- **A sibling command is not a new subsystem.** `analyzeMoves()` reused `SearchIntent::Analysis`,
  the existing RT-01 tick, and the existing render path rather than adding a parallel channel —
  the whole feature landed as one send helper + one controller method + one console command, and
  three of the todo's scope items (completion-coord handling, panel rendering, overlay rendering)
  needed no new code at all, only tests to pin the existing behaviour.
- **A help-text follow-on is cheap when the string is the only lever.** CONS-03 was a one-line
  `summary` change because `printHelp()` builds entirely from the registered `CommandSpec`s;
  changing `usage` instead would have churned a pinned test for no user benefit. Prefer `summary`.

## Rolled over to Backlog

Nothing rolled over — both committed items finished.

## Next sprint

Sprint 20 — not yet opened; the Backlog is currently empty. Run
`/sprint open 20 "<goal>" <CODE...>` once new work is filed and committed. Release `v0.8.0` cut at
close (see `CHANGELOG.md`).
