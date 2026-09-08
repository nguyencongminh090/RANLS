# PROTO-06 — replace the analysis board overlay with the Yixin-Board model (REALTIME feed + per-cell winrate tags)

**Status:** ✅ DONE (2026-09-08, branch `proto-06/port-realtime-feed-to-board` — not merged; orchestrator drives the PR)

Implemented per decision (a) — only what a stock Rapfi emits drives real features; `POS`/`DONE`
are parsed (for the Yixin engine) but nothing depends on them; Rapfi search config untouched.

- **`AnalysisOverlay` / `AnalysisOverlayCell`** in `src/engine/engine_types.h` — per-empty-cell
  `tag` / `tagDepth` / `tagWinrate` / `pos` (0/1/2) / `lost`, plus one `bestMove`.
- **`GomocupProtocol`** — `parseMessage` REALTIME branch now handles `POS`/`DONE`/`LOST`/`REFRESH`
  (+ `BEST` also feeds `overlay_.bestMove`); `onPVDone` stamps `pv.moves[0]` with
  `overlayTagText(pv)` + `currentStatus_.depth`, and on the last PV of a round
  (`currentPVIndex_+1 == currentNumPV_`) clears tags with `tagDepth < currentStatus_.depth`.
  `REFRESH` clears `pos` only. All coords via `parseEngineCoord` (no axis special-casing).
  `clearAnalysisState()` also clears `overlay_`. New `signal_analysis_overlay(const AnalysisOverlay&)`
  on `IEngineProtocol`, emitted per mutating line via `emitOverlay()`.
- **`GameState`** — owns `analysisOverlay_` + `overlayDirty_`; `setAnalysisOverlay()` (dirty flag
  only, never through `setAnalysisData`), `clearAnalysisOverlay()` (synchronous emit),
  `signal_analysis_overlay`. `tickAnalysis()`/`flush()` coalesce the overlay channel on the same
  RT-01 75 ms point. `resetAnalysisState()` clears the overlay on position change.
- **`EngineController`** — forwards `protocol_->signal_analysis_overlay` → `setAnalysisOverlay`
  (UI-04 in-flight gate); `clearAnalysisOverlay()` on search-completion coordinate, `stopAnalysis()`,
  and process death.
- **`BoardViewModel`** — `candidateMoves` deleted; new `searchOverlay` (`SearchOverlayMark` with
  `Kind` Tag/Lost/Best/Examined/Examining), resolved single-winner per cell
  (`tag > lost > best > pos==1 > pos==2`) in `update()` ONLY while `isAnalyzing()` and
  `viewConfig.showSearchOverlay`; `showSearchWinrate` drops just the tag layer.
- **`BoardRenderer`** — `drawCandidateMoves` → `drawSearchOverlay` (same pipeline slot); winrate
  tag keeps the HSV heat colour, other marks use fixed shape-distinct glyphs.
- **`ViewConfig`** — `showSearchOverlay` + `showSearchWinrate` (both default on), persisted as
  `show_search_overlay` / `show_search_winrate` in `SettingsStorage`, one View-menu checkbox each
  (`win.show-search-overlay` / `win.show-search-winrate`), synced by `syncSearchOverlayMenu()`.

**Verification:** `tests/test_proto06_analysis_overlay.cpp` (6 cases, `ranls-gui-ui-tests`) —
INFO tag/tagDepth + per-round stale cleanup; REALTIME LOST/BEST/POS/DONE/REFRESH; reset via
`signal_board_changed` + `clearAnalysisOverlay`; burst-coalescing (N lines → 1 emit); a YXNBEST
block with no `INFO DEPTH` still cleans stale tags at the right round depth; BoardViewModel
live-only gate + both toggles. `test_proto05_incremental_stream.cpp` updated (candidateMoves →
searchOverlay tag count; fixture first-moves moved to empty cells). Release build clean;
`ctest` 4/4 green (`port02`, `rel02`, `ranls-gui-tests`, `ranls-gui-ui-tests` — incl.
`test_rt01_throttle`, `test_proto05_*`, `test_ui04_*`, `test_ui07_*`, `test_anlz06`/`07`,
`test_game_state`). Debug build in a `/run/media/...` worktree trips `port02` on the `__FILE__`
path regex — pre-existing artifact of Debug + worktree path, not a PROTO-06 regression (Release
passes). Manual live-engine run against Rapfi is human-owed (no engine/display on build host).

---

**Original scoping (kept for reference):** 🔲 OPEN (Active — Sprint 18)
**Area:** `src/engine/gomocup_protocol.cpp` (`parseMessage` REALTIME branch, `parseInfo`/`onPVDone`, `clearAnalysisState`), a new analysis-overlay model struct (owned by `GameState`), `src/model/game_state.{h,cpp}`, `src/model/board_view_model.{h,cpp}`, `src/ui/board_renderer.{h,cpp}` (delete `drawCandidateMoves`, add one per-cell overlay layer), `src/model/config.h` (`ViewConfig` toggles) + settings persistence + View menu items; regression tests under `tests/`
**Priority:** P2 (chunky — one cohesive CODE, but touches engine→model→ui + settings + menu; split only if it gets unwieldy)
**Source:** `docs/notes/2026-09-08-refyxb-multipv-rendering.md` + `docs/notes/2026-09-08-rapfi-engine-realtime-output.md`. User: PROTO-05 shipped but "not reached my expect" — the reference GUI's live board overlay was never ported.
**Design:** none — all design questions resolved with the user 2026-09-08 (see "Resolved decisions" below). Scoped directly.
**Depends on / relates to:** PROTO-05 (shipped `SHOW_DETAIL 3` + `YXSHOWINFO` — `INFO` + `REALTIME` lines arrive now), RT-01 (throttle — realtime coalesces on the same 75 ms tick, never through `setAnalysisData`), STATE-01 / UI-04 (position-change reset), STATE-03 (`currentPVs_` shrink), RT-03 (board redraw cost), reference `RefYXB/Yixin-Board/main.c` (`:6408–6460` REALTIME parse, `:6477–6600` INFO parse, `:705–716` render priority, `:951–953` reset).

## Problem

The current analysis overlay on the board is `BoardViewModel::candidateMoves` → `BoardRenderer::drawCandidateMoves`: one translucent heat-map circle + winrate label per PV's first move, rebuilt from `GameState::pvLines()` every RT-01 tick. It persists after a search ends and has no live "thinking" feedback.

The original Yixin-Board (`main.c`) instead drives a **per-cell** overlay that is live-only (drawn only while the engine is thinking) and is fed by two engine streams:

- **`INFO PV DONE`** → per-cell winrate/mate text tag (`boardtag`) + the depth it was written at (`boarddepth`), on each PV's first move. When the last PV of a depth round completes, tags on cells whose `boarddepth < curdepth` are cleared (stale candidates drop).
- **`MESSAGE REALTIME …`** → `POS`/`DONE` (cell being examined / examined), `LOST` (losing root move), `BEST` (current best root move), `REFRESH` (new depth — clears the "examining" cells).

Render is single-winner per empty cell, priority: **winrate tag > lost > best > examined (`pos==1`) > examining (`pos==2`)**.

The user wants YixinBoard to match this model: **the realtime overlay replaces `candidateMoves` entirely.**

## Engine reality (from `docs/notes/2026-09-08-rapfi-engine-realtime-output.md`) — decision (a)

With a stock Rapfi (default config), the `REALTIME` feed is only:
- `REALTIME LOST x,y`, `REALTIME BEST x,y` (once `rootDepth >= 8` and `elapsed >= 200 ms`, PV 0 only)
- `MESSAGE REALTIME REFRESH` (once per PV per depth — clears `pos`)
- **NOT `REALTIME POS` / `REALTIME DONE`** — gated on `!aspirationWindow`, and aspiration is on by default (only settable via Rapfi's `config.toml`, no runtime command).
- **NOT `REALTIME VAL`** — Rapfi never emits it (Yixin engine only).
- No feed at all for the first ~8 depths / 200 ms, and none under MCTS search.

**Decision (a):** implement what Rapfi actually supports. Parse `POS`/`DONE` too (harmless, and real for the Yixin engine), but **no feature depends on them** — with Rapfi the overlay is winrate-tags + `LOST` + `BEST` + `REFRESH`, and that is the accepted result. Do **not** touch Rapfi's search config to force POS/DONE.

## Resolved decisions (with the user, 2026-09-08)

- **Replace, not add.** Delete `BoardViewModel::candidateMoves` + `BoardRenderer::drawCandidateMoves`. The new per-cell overlay is the only engine overlay. `pvPreview` / `drawPVHighlight` (hover ghost stones) is unrelated — keep it.
- **Model (Q1):** a dedicated overlay struct — `AnalysisOverlay` (working name) — owned by `GameState` alongside `pvLines_` / `engineStatus_`. Per-cell state: `tag` (winrate% / `+M`/`-M` / `D`), `tagDepth`, `pos` (0/1/2), `lost` (bool); plus a single `bestMove` for the `REALTIME BEST` highlight.
- **Update path (Q2):** a new `signal_analysis_overlay` (or reuse a flag on the existing tick). Realtime + `onPVDone` set an `overlayDirty_` flag; `GameState::tickAnalysis()` (the existing RT-01 75 ms coalescing point) emits it. **Never** route realtime updates through `setAnalysisData()` (tree-node writes + eval-history cache). One `BoardViewModel::update()` + `queue_draw()` per tick — never per engine line.
- **Winrate tags from `INFO PV DONE`, not `MESSAGE (n)`** (Q3). `onPVDone` writes `tag` + `tagDepth` for `pv.moves[0]`; on the last PV of a round (`currentPVIndex_ + 1 == currentNumPV_`) clear tags where `tagDepth < currentStatus_.depth`. The `MESSAGE (n)` path still feeds `pvLines_` / `PVView` as today — only the *board* overlay changes source.
- **Render (Q3):** one new `BoardRenderer` layer replacing `drawCandidateMoves`, walking cells that have any overlay state, single-winner priority `tag > lost > best > pos==1 > pos==2`. Winrate tag keeps the HSV heat colour (hue red→cyan by win%, like `winrate2colorstr`).
- **Reset (Q4):**
  - `REALTIME REFRESH` → clear `pos` only (all cells 2/1 → 0). Keep `lost`, `tag`, `bestMove`.
  - last PV of a depth round → clear stale `tag` by depth (above).
  - `signal_board_changed` (position change) / search end / engine stop / crash → clear the whole overlay (wire into the same points as `clearAnalysisState` / `resetAnalysisState`).
  - **Live-only:** `BoardViewModel::update()` only copies overlay state into render fields while `GameState::isAnalyzing()`. Search ends → overlay disappears, board returns to bare (this is intended, matches Yixin-Board; one-shot analyze leaves no board overlay after it converges).
- **Toggles (Q5):** two `ViewConfig` flags, both default on, both persisted (`settings_storage`) + a View-menu item each, mirroring Yixin-Board:
  - `showSearchOverlay` — master on/off for the whole overlay.
  - `showSearchWinrate` — when off, hide the winrate/mate text tags but keep `lost`/`best`/`pos` marks.
- **BEST highlight (Q6):** yes — `REALTIME BEST` gets a distinct current-best-root-move highlight (Yixin-Board `f=8`), separate from `pos`/`lost`.

## Scope (in order)

1. `parseMessage` REALTIME branch: parse `POS` / `DONE` / `LOST` / `REFRESH` into the overlay model; keep `BEST` / `PV` / `VAL` working. Reuse `parseEngineCoord` for all coords (same `CoordText`/`ioCoordMode` path as bestmove — already correct).
2. New `AnalysisOverlay` struct + `GameState` ownership + `overlayDirty_` + emit on `tickAnalysis()` / `flush()`.
3. `onPVDone`: write per-cell `tag` + `tagDepth` for `pv.moves[0]`; stale-tag cleanup on last PV of round. `INFO NUMPV` already resizes `currentPVs_`; use `currentNumPV_` for the round-end test.
4. Reset wiring: `REFRESH` (pos only), position change / search end / stop / crash (all), mirroring `clearAnalysisState`.
5. `BoardRenderer`: delete `drawCandidateMoves`; add the single-winner per-cell overlay layer in the pipeline (same slot). Delete `BoardViewModel::candidateMoves`; add overlay render fields, populated in `update()` only while analyzing.
6. `ViewConfig` `showSearchOverlay` + `showSearchWinrate`, settings persistence, two View-menu items.
7. Regression tests (below).

## Acceptance criteria

- Feeding a captured Rapfi transcript (`INFO PV DONE` rounds + `REALTIME LOST`/`BEST`/`REFRESH`):
  - winrate tags appear on each PV's first move, update per depth, and stale tags (moves that drop out of the top-N) clear once per round;
  - `LOST` cells get a distinct mark that persists until search end / position change;
  - `BEST` cell gets its own highlight;
  - `REFRESH` clears `pos` state only.
- Overlay is drawn only while `isAnalyzing()`; it vanishes on search end and on position change.
- No overlay update goes through `setAnalysisData` / eval-history / tree-node writes; board redraw stays coalesced at the RT-01 tick.
- `POS` / `DONE` parsing works (verified via a synthetic transcript) but nothing regresses when they never arrive (Rapfi default).
- `showSearchOverlay` off → no overlay at all; `showSearchWinrate` off → marks but no numbers. Both persist across restart.
- `PVView` / `pvLines_` / WinGraph unchanged (still fed by the existing paths).
- No regression: RT-01 throttle, STATE-01/UI-04 reset, STATE-03 shrink, PROTO-01 hardening, PROTO-05 incremental stream, ANLZ-06/07.

## Scope boundary

- Do **not** keep `candidateMoves` / `drawCandidateMoves` — this task removes them.
- Do **not** route realtime through `GameState::setAnalysisData` or `signal_engine_analysis`'s tree/eval work.
- Do **not** change Rapfi's search config (no `aspiration_window` toggle, no new engine command) to chase `POS`/`DONE` — decision (a).
- Do **not** change the `MESSAGE (n)` / `Bestline` → `pvLines_` path or `PVView`.
- Do **not** touch `SHOW_DETAIL` / `YXSHOWINFO` / `YXNBEST` / `YXBOARD` (PROTO-05).
- No new realtime states beyond the reference set (tag / pos / lost / best).
