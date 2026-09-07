# PROTO-05 — enable-incremental-analysis-stream

## Approach

1. **Run `/systematic-debugging` Phase 1–2 first** — the diagnosis is already written
   (`docs/notes/2026-09-07-pv-multipv-display-root-cause.md`), but reproduce it against a real
   engine before changing commands, and capture the "before" engine-log transcript as the
   regression-test fixture.
2. **Design call with the user — pick the enable mechanism:**
   - **A. `YXSHOWINFO` on engine start.** One line in `generateStart` (or a new post-START
     command). Rapfi's `setGUIMode()` auto-upgrades `messageMode` BRIEF→NORMAL and silences
     "unknown command" errors. Gets per-depth `Depth …` + `(n)|…` Multi-PV lines. Does **not**
     enable the `REALTIME` feed.
   - **B. `INFO SHOW_DETAIL 3`** instead of `0` at `gomocup_protocol.cpp:260`. Enables `REALTIME`
     (live "thinking" highlight) + the structured `INFO PV n … INFO PV DONE` per-depth blocks
     (which carry `NUMPV`, so Multi-PV count is explicit). Independent of `messageMode`.
   - **C. Both** — recommended: `YXSHOWINFO` for the NORMAL `MESSAGE` stream + `SHOW_DETAIL`
     configurable (default `3`, overridable via the existing `command_dispatcher.cpp:680` path and
     ideally `SettingsDialog`).
   - Recommended default: **C**, with `SHOW_DETAIL` as an `EngineConfig` field (default 3) so
     power users can dial it back; `YXSHOWINFO` unconditional.
3. Implement in `generateStart` / `generateConfig` / `generateAnalyzeRequest`. If `SHOW_DETAIL`
   becomes configurable, stop hardcoding `0` at `:260` — emit the configured value, and make sure
   the `customParams` loop (`:262`) can still override it (order: emit configured value first,
   then customParams, so a user `SHOW_DETAIL` in customParams wins).
4. Fix the `parseMessage` `"Speed "` branch: parse the leading `Speed <speedtext>` token
   (`speedText` format — may be `"1234K"` / `"1.2M"`) into `currentStatus_.nps` before the
   `|`-split loop, or add a `part.rfind("Speed ", 0)` case inside the loop.

## Pitfalls

- **`gomocup_protocol.cpp:260` currently wins unconditionally.** `command_dispatcher.cpp:680`
  writes `cfg.customParams["SHOW_DETAIL"]`, but `generateConfig` emits the hardcoded
  `INFO SHOW_DETAIL 0` *before* iterating `customParams`, so the last-write-wins only works if the
  customParams line comes after. Verify the final command order actually does what you intend.
- **`YXSHOWINFO` silences unknown-command errors** for the whole session — a mild downside
  (typos in the console stop being reported). Note it in the fix-log.
- **`REALTIME` feed volume.** `SHOW_DETAIL 3` adds `REALTIME POS/DONE/LOST/BEST` per root move per
  depth — high line rate. RT-01's 75 ms tick already coalesces model→UI, but the engine-log
  append path (`bottom_panel`) is separate; confirm it stays bounded (RT-02) and sticky-scroll
  (UI-10/UI-15) still behaves under the heavier stream.
- **`INFO` output tag vs `INFO` input command.** `parseLine` routes `INFO ` prefixed lines to
  `parseInfo`. Rapfi's detail feed lines are `INFO PV 0`, `INFO DEPTH 12`, … — already handled.
  Do not confuse with the `INFO <param> <value>` commands the GUI *sends*.
- **STATE-03 truncation vs the INFO stream.** `onPVDone` (INFO path) has no "new round"
  truncation like `commitPV` (MESSAGE path). If you enable the INFO stream and multiPV is lowered
  mid-search, stale high-index `currentPVs_` entries can persist. `INFO NUMPV` is available —
  consider resizing `currentPVs_` down to `NUMPV` when it arrives.
- **`analysisConverged()` / ANLZ-07.** With a live feed, `captureAnalysisResult()` now samples a
  much richer final state, but its comparison is still just bestMove + evalText. Re-verify Analyze
  Mode doesn't either busy-loop or wrongly converge after this change.
- **PVView RT-03 rebuild flicker.** `PVView::update` rebuilds row widgets whenever the PV *count*
  changes. A per-depth feed that legitimately grows 1→N each round will now exercise this path
  frequently — confirm no visible flicker / hover loss. STATE-03's mid-round `resize(1)` in
  `commitPV` is the risk point; if flicker appears, the fix is to stage the new round in a scratch
  vector and swap atomically at the round boundary, not to emit on every partial state.

## Verification before done

- New regression test feeds a captured NORMAL + `SHOW_DETAIL 3` transcript (multi-depth, multiPV
  ≥ 3) through `GomocupProtocol` → `GameState` and asserts:
  - `engineStatus().depth` increases across the transcript (not just set once at the end);
  - `pvLines()` reaches `multiPV` non-empty entries and each carries its own `depth`;
  - `BoardViewModel::candidateMoves` has `multiPV` markers after a mid-search tick.
- Manual check against a real engine: analyse a position for ~5 s, watch the PV list and value
  readout update progressively; confirm Multi-PV rows 2..N appear.
- `ctest` fully green — pay attention to `test_rt01_throttle`, `test_proto03_*`, `test_ui07_*`,
  `test_anlz06/07`.

## Boundaries

- Output-configuration commands + the one `parseMessage` NPS fix only.
- No changes to `PVView` / `EngineStatusView` / `BoardRenderer` rendering.
- No changes to the `YXNBEST` / `YXBOARD` request shape.
- No RT-01 throttle redesign.
- If the fix ends up needing the "stage-and-swap" round buffering above, that is in scope (it is
  the correct fix for the flicker this change can expose); anything larger is a new Backlog line.
