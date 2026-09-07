# PROTO-05 — enable engine incremental analysis stream (per-depth PV / Multi-PV / value / board-PV)

**Status:** 🔲 OPEN (Backlog)
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
