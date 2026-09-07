# PROTO-05 — enable engine incremental analysis stream (per-depth PV / Multi-PV / value / board-PV)

**Status:** ✅ DONE (Sprint 17 Active — branch `proto-05/enable-incremental-analysis-stream`, not yet merged)

Implemented Mechanism C (decided with the user): (1) `YXSHOWINFO` prepended to
`generateStart()` (before `START`) — unconditional, auto-bumps Rapfi BRIEF→NORMAL,
silences unknown-command errors for the session; (2) `EngineConfig::showDetail`
(new field, default 3) emitted as `INFO SHOW_DETAIL <n>` by `generateConfig()`
before the `customParams` loop, replacing the hardcoded `INFO SHOW_DETAIL 0` —
`command_dispatcher.cpp:680`'s `customParams["SHOW_DETAIL"]` override still wins
(emitted last). SettingsDialog control **was added** ("Analysis Detail" 0–3 spin,
Search tab) + persisted via `settings_storage`. (3) `parseMessage` `"Speed "`
branch now parses the leading `Speed <speedText>` token into `currentStatus_.nps`.
(4) Parser verification: fixed the `"(n)"` branch to read PV moves from the
trailing pipe field + parse `SD <n>` so Rapfi's 4-part `printRootMoves` ranked
list parses (moves were read from `"SD 14"` and lost); added `roundStart` to the
`commitPV` lambda so the per-depth `"Depth …"` and end `"Bestline …"` summary
lines (index 0, but not round markers) no longer truncate the `(n)`-stream
Multi-PV list #2..#N for the same round. STATE-03 truncation kept for the real
`(1)` round marker; `onPVDone`/`INFO NUMPV` shrink path already correct — no
change. Stage-and-swap round buffering **not** needed (no PVView flicker to fix;
per-depth 1→N growth was already the pre-PROTO-05 `(n)`-stream behaviour).
ANLZ-07 `analysisConverged()` semantics unchanged — still compares bestMove +
evalText on the (now richer) final snapshot.

**Verification:** full build clean (only the 3 pre-existing `-Wunused-function`
warnings). `ctest` 4/4 green: `ranls-gui-tests` 218 cases / 2532 assertions,
`ranls-gui-ui-tests` 39 cases / 290 assertions (was 38/281 — +`test_proto05_incremental_stream`,
2 cases / 9 assertions), `port02-style-css-bundled` + `rel02-version-single-source`
pass. `test_rt01_throttle`, `test_proto03_*`, `test_ui07_*`, `test_anlz06`,
`test_anlz07` all green. New regression test: `tests/test_proto05_incremental_stream.cpp`.
Live-engine manual check (analyse ~5 s, watch PV list / value readout progress,
confirm Multi-PV rows 2..N) remains an outstanding **human** step — no engine
binary / display on the build host.
**Area:** `src/engine/gomocup_protocol.cpp` (`generateConfig` / `generateStart` / `generateAnalyzeRequest`), possibly `src/model/engine_config.h` + `src/ui/settings_dialog.*`; regression test under `tests/`
**Priority:** P1
**Source:** `docs/notes/2026-09-07-pv-multipv-display-root-cause.md` (architecture report, 2026-09-07) — user asked "how does the engine update the display on a new depth in multi-pv analyze?"
**Design:** none — scoped directly from the diagnosis note
**Depends on / relates to:** RT-01 (throttle), RT-03 (PVView rebuild-on-count-change), STATE-03 (`currentPVs_` shrink), PROTO-01 (parser hardening — must not regress), ANLZ-07 (`analysisConverged` convergence gate)

## Problem

With Rapfi at its default configuration the GUI receives **no per-depth analysis updates at all**.
The PV panel, the Multi-PV list, the engine value readout (`EngineStatusView`) and the board's PV
candidate markers update **exactly once — when the search ends — and only ever show a single PV
line**. Multi-PV candidates #2..#N never reach the GUI.

Root cause (from the note): `GomocupProtocol` is written to consume an incremental analysis feed
(`REALTIME …`, `INFO PV DONE`, per-depth `Depth …`, `(n)|…`, UCILIKE `multipv …`), but the
command generators send the engine exactly the set of instructions that suppress every one of
those streams:

1. `gomocup_protocol.cpp:260` hardcodes `INFO SHOW_DETAIL 0` → turns off both the `REALTIME` feed
   and the `INFO` detail feed (`SearchOptions::infoMode = INFO_NONE`).
2. The GUI never sends `YXSHOWINFO` and never sets `messageMode`, so Rapfi stays at the default
   `BRIEF` message mode. Per `Rapfi/search/searchoutput.cpp`, `BRIEF` emits only the end-of-search
   `MESSAGE Speed … | Depth … | Eval … | Node … | Time …` summary plus one `MESSAGE Bestline
   <pv>` line — no per-depth `Depth …` lines (`printDepthCompletes`, NORMAL only) and no
   per-candidate `(n) …` Multi-PV lines (`printMoveResult` / `printRootMoves`, NORMAL/UCILIKE
   only).

Consequently the RT-01 throttle, the STATE-03 "new round" truncation in `commitPV`, `onPVDone`,
`parseRealtimePV`, and the UCILIKE branch are all effectively dead paths against a stock engine.

This affects manual analyze (one-shot Analyze button) and auto (Analyze Mode ∞) identically —
neither shows depth progression or the Multi-PV set.

Secondary defect noted in the report: `parseMessage`'s `"Speed "` branch splits the final summary
line on `|` and handles the `Depth`/`Eval`/`Node`/`Time` parts but never parses the leading
`Speed <speed>` token itself, so NPS from the end-of-search summary is dropped.

## Scope (in order)

1. Reproduce: run an analysis against Rapfi with default config, capture the engine log, confirm
   only the two end-of-search `MESSAGE` lines arrive during a multi-second search.
2. Decide the enable mechanism with the user (see `docs/instruction/PROTO-05-*.md`): `YXSHOWINFO`
   (minimal, auto-bumps BRIEF→NORMAL), and/or removing the hardcoded `INFO SHOW_DETAIL 0` in
   favour of a configurable value defaulting > 0 (`3` = REALTIME + detail).
3. Implement the chosen command change in `generateStart` / `generateConfig` /
   `generateAnalyzeRequest`. Keep `command_dispatcher.cpp:680`'s manual `SHOW_DETAIL` override
   working (today `:260` unconditionally re-writes `0` before the `customParams` loop).
4. Verify the existing parser paths (`parseInfo` / `onPVDone`, `parseMessage` `"Depth "` / `(n)`
   branches) correctly consume the now-live streams; fix any mismatch found.
5. Fix the `"Speed "` branch NPS drop.
6. Regression test (see below).

## Acceptance criteria

- During a multi-depth analysis against a NORMAL+detail-enabled engine transcript, `GameState`
  receives a monotonically increasing sequence of depth values, and `pvLines()` /
  `BoardViewModel::candidateMoves` reach `multiPV` entries per depth round (not just 1).
- The engine value readout (`D:` / `N:` / `NPS:` / `T:` / `Eval:`) updates during the search, not
  only at the end.
- Manual analyze and Analyze Mode both show per-depth progression.
- NPS from the end-of-search `Speed …` summary line is applied.
- No regression in PROTO-01 parser hardening, RT-01 throttle behaviour, RT-03 hover preservation,
  or STATE-03 shrink behaviour.
- Existing tests (`test_rt01_throttle`, `test_ui07_*`, `test_proto03_*`, `test_anlz0*`) stay green.

## Scope boundary

- Do not rewrite the RT-01 throttle or the coalescing tick.
- Do not change `PVView` / `BoardRenderer` drawing logic — they already handle N rows.
- Do not touch the `YXNBEST` / `YXBOARD` request shape beyond the output-config commands.
- Do not add board-drawing sinks or touch PROTO-03's extension DSL.
- Do not alter `analysisConverged()` / ANLZ-07 convergence semantics (though its inputs get
  richer — verify it still behaves).
- Base the change strictly on the diagnosis note; broader "while we're here" protocol cleanups go
  to a separate Backlog line.
