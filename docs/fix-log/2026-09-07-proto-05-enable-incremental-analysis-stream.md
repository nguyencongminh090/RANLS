# PROTO-05 — enable the engine incremental analysis stream (per-depth PV / Multi-PV / value)

**Status:** ✅ FIXED

Tracked task `PROTO-05` (Sprint 17). Branch `proto-05/enable-incremental-analysis-stream`.
Task detail: [docs/todo/PROTO-05-enable-incremental-analysis-stream.md](../todo/PROTO-05-enable-incremental-analysis-stream.md);
execution guidance: [docs/instruction/PROTO-05-enable-incremental-analysis-stream.md](../instruction/PROTO-05-enable-incremental-analysis-stream.md).
Root-cause diagnosis: [docs/notes/2026-09-07-pv-multipv-display-root-cause.md](../notes/2026-09-07-pv-multipv-display-root-cause.md).

## Prompt

`/implement-task PROTO-05` — with Rapfi at default config the GUI's PV panel, Multi-PV list,
value readout and board PV markers update exactly once (at search end) and only ever show one PV
line. The parser (`parseInfo`/`onPVDone`, `parseMessage` `"(n)"`/`"Depth "`/`"Speed "` branches)
is written for an incremental feed, but `generateConfig()` sent `INFO SHOW_DETAIL 0` and the GUI
never sent `YXSHOWINFO`, so Rapfi stayed at messageMode BRIEF with both the REALTIME and INFO
detail feeds off. Decision taken with the user: **Mechanism C** (both `YXSHOWINFO` and a
configurable `SHOW_DETAIL` defaulting to 3).

## Reproduction

No engine binary / display server on the build host, so the "before" state (only the two
end-of-search `MESSAGE Speed …` / `MESSAGE Bestline …` lines during a multi-second search) was
reproduced by constructing the transcript from Rapfi's documented output formats
(`Rapfi/search/searchoutput.cpp`: `printPvCompletes` / `printDepthCompletes` / `printRootMoves` /
`printSearchEnds`, and `docs/protocol.md` §5). That transcript is the permanent regression
fixture, embedded in `tests/test_proto05_incremental_stream.cpp`. The live-engine progressive
check (analyse ~5 s, watch the PV list / value readout climb, confirm Multi-PV rows 2..N) remains
an outstanding **human** step.

## Action

### Enable the streams (`src/engine/gomocup_protocol.cpp`, `src/model/config.h`)

- **`generateStart()`** now returns `{"YXSHOWINFO", "START <n>"}` — `YXSHOWINFO` unconditionally,
  before `START` (protocol.md §1.1 / §9.2). It is a one-way, idempotent flag flip: upgrades
  Rapfi's `messageMode` BRIEF→NORMAL (so the per-depth `Depth N-M | Eval … | … | <pv>` and
  per-candidate `(n) …` MESSAGE lines the parser already understands get emitted) and silences
  `ERROR Unknown command` for the rest of the session.
  **Tradeoff (accepted):** console typos of unknown commands also stop being reported by the
  engine for that session.
- **`EngineConfig::showDetail`** — new field, default **3** (REALTIME feed + per-depth
  `INFO PV n … INFO PV DONE` blocks). `generateConfig()` emits `INFO SHOW_DETAIL <clamped 0..3>`
  in place of the hardcoded `INFO SHOW_DETAIL 0`, **before** the `customParams` loop, so a user
  `!set show_detail N` (which `command_dispatcher.cpp:680` writes to
  `customParams["SHOW_DETAIL"]`) is emitted last on the wire and still wins — the manual-override
  path is preserved.

### Parser verification / fixes (`parseMessage`)

- **`"Speed "` branch** — parses the leading `Speed <speedText>` token (nodes/sec, possibly with
  a K/M/G suffix) into `currentStatus_.nps` via `parseNodeCount`. Previously this token was never
  read, so the end-of-search NPS was dropped.
- **`"(n)" branch`** — PV moves are now taken from the *trailing* pipe field and `SD <n>` is
  parsed, so Rapfi's 4-part `printRootMoves` ranked list
  (`(n) <v> (W .., D .., S ..) | V <nodes> | SD <sd> | <pv>`) parses as well as the 3-part
  `printPvCompletes` form (`(n) <v> | <d>-<sd> | <pv>`). Before this, moves were read from
  `parts[2]` = `"SD 14"` and silently lost for the ranked list.
- **`commitPV` lambda** — gains `bool roundStart = true`. The `"Depth …"` and `"Bestline …"`
  summary lines commit index 0 but are **not** new-round markers (they carry only PV #1 and are
  interleaved with / follow the ranked `(n)` list), so they pass `roundStart = false` and no
  longer trigger the STATE-03 `resize(1)` truncation that was wiping PVs #2..#N reported by the
  `(n)` stream for the same depth round. The truncation is kept for the genuine `(1)` round
  marker in the `(n)` / UCILIKE `multipv` streams (its regression test,
  `test_gomocup_protocol.cpp` "MESSAGE-stream commitPV drops stale high-index PVs", still passes).
- `onPVDone` / `INFO NUMPV` shrink path was already correct (`currentPVs_.resize(clamped)`
  shrinks) — no change. No "stage-and-swap" round buffering: there is no PVView flicker to fix
  (per-depth 1→N growth was already the pre-PROTO-05 `(n)`-stream behaviour), so it stayed out of
  scope per the instruction.
- `analysisConverged()` / ANLZ-07: unchanged. Re-verified `test_anlz07` green with the richer
  feed — it still compares bestMove + evalText on the final snapshot.

### Settings (`src/ui/settings_dialog.*`, `src/model/settings_storage.cpp`)

SettingsDialog control **was added** — an "Analysis Detail" spin (range 0–3, Search tab, next to
Multi PV) bound to `EngineConfig::showDetail`, read back in the Apply handler. Persisted as
`show_detail=` in the settings file (load + save).

## Scope boundary honoured

No change to the RT-01 throttle / coalescing tick, `PVView` / `BoardRenderer` /
`EngineStatusView` drawing, the `YXNBEST` / `YXBOARD` request shape (only output-config
commands), PROTO-03's extension DSL, or `analysisConverged()` / ANLZ-07 semantics.

## Verification

- Full build clean — only the 3 pre-existing `-Wunused-function` warnings
  (`evalDisplayText` / `signedIntText` / `parseBoolToken`).
- New regression test `tests/test_proto05_incremental_stream.cpp` (2 cases, in
  `ranls-gui-ui-tests` because it drives `BoardViewModel`): replays a NORMAL + `SHOW_DETAIL 3`
  transcript (3 depths, multiPV 3) through `GomocupProtocol → GameState → BoardViewModel` and
  asserts `engineStatus().depth` climbs 10→11→12, `pvLines()` reaches 3 non-empty entries each
  carrying `depth >= 12`, `BoardViewModel::candidateMoves` has 3 markers after a mid-search tick,
  the 4-part ranked lines keep their PV moves, and NPS from `Speed 1234K` is applied (1234000);
  the second case asserts `generateStart()`/`generateConfig()` emit `YXSHOWINFO` +
  `INFO SHOW_DETAIL 3` and that a `customParams` `SHOW_DETAIL` still comes after.
- `ctest` 4/4 green: `ranls-gui-tests` 218 cases / 2532 assertions;
  `ranls-gui-ui-tests` 39 cases / 290 assertions (was 38 / 281);
  `port02-style-css-bundled` + `rel02-version-single-source` pass.
- Named suites re-checked green: `test_rt01_throttle`, `test_proto03_*`, `test_ui07_*`,
  `test_anlz06`, `test_anlz07`.
- Live-engine manual progressive-update check: **not run** — no engine binary / display on the
  build host. Outstanding human step (also flagged in `docs/sprint/current.md`).
