# Sprint 18 (closed 2026-09-08)

**Goal:** Yixin-Board-style live analysis board overlay — the board's engine overlay becomes a
per-cell, live-only visualization fed directly by the engine's search-progress streams, matching
the reference Yixin-Board.
**Dates:** 2026-09-08 to 2026-09-08.

## Final state — all items shipped (1 of 1)

| CODE | Summary | Status |
|---|---|---|
| PROTO-06 | replace the analysis board overlay with the Yixin-Board live per-cell model (per-cell winrate tags + REALTIME feed) | ✅ DONE |

Single-item sprint, opened the same day PROTO-06's design was resolved with the user (decision (a):
implement only what a stock Rapfi actually emits). Points not estimated (consistent with Sprints
3–17).

## What shipped

- **PROTO-06** (PR #33, squash `ae4dfbd`): ported the original Yixin-Board live per-cell search
  overlay, replacing the old `candidateMoves` → `drawCandidateMoves` heat-map. New
  `AnalysisOverlay` / `AnalysisOverlayCell` (`engine_types.h`). `GomocupProtocol`'s REALTIME branch
  now consumes `POS`→`pos=2`, `DONE`→`pos=1`, `LOST`→`lost`, `REFRESH`→`clearPos()`, and `BEST`→
  `overlay_.bestMove`; `onPVDone()` stamps `pv.moves[0]` with a winrate/mate tag (`+M`/`-M`/`D`/
  `NN%` capped 1–99) plus the root depth it was written at, and on the last PV of a round clears
  tags whose depth is stale. A new `signal_analysis_overlay` runs parallel to `signal_analysis` and
  never through `setAnalysisData` (no tree-node / eval-history writes). `GameState` owns
  `analysisOverlay_` + `overlayDirty_`, coalesced on the existing RT-01 75 ms `tickAnalysis()` /
  `flush()` tick; `EngineController` forwards it behind the `isAnalyzing()` UI-04 gate and clears it
  on search completion, `stopAnalysis()`, and process death; `resetAnalysisState()` clears it on
  every position change. `BoardViewModel::candidateMoves` deleted → `searchOverlay`, resolved
  single-winner per empty cell (`tag > lost > best > pos==1 > pos==2`) and populated only while
  analyzing; `BoardRenderer::drawSearchOverlay` draws it in the same pipeline slot (HSV heat tag +
  shape-distinct lost/best/examined marks). Two persisted `ViewConfig` toggles
  (`showSearchOverlay` master, `showSearchWinrate` tags-only), both default on, each with a View-menu
  checkbox — mirrors Yixin-Board `showanalysis` / `showanalysiswinrate`. Decision (a): `POS`/`DONE`
  are parsed (real for the Yixin engine) but no feature depends on them, and Rapfi's search config
  is untouched — so on a stock Rapfi the overlay is winrate tags + `LOST` + `BEST` + `REFRESH`, the
  accepted result. Regression test `tests/test_proto06_analysis_overlay.cpp` (6 cases / 53
  assertions) replays constructed INFO + REALTIME transcripts and asserts per-cell tag/tagDepth,
  per-round stale-tag cleanup, `LOST` persistence through `REFRESH`, `REFRESH` clearing only `pos`,
  the YXNBEST no-`INFO DEPTH` round-depth case, burst→1-emit coalescing, and the BoardViewModel
  live-only gate + both toggles; `test_proto05_incremental_stream.cpp` updated for the
  `candidateMoves` → `searchOverlay` rename. `ctest` 4/4 green. **Deferred / outstanding:** the
  live-engine run against Rapfi (`show_detail 3`, ~5–10 s — winrate tags per depth, losing moves
  marked, best move highlighted, overlay gone when idle, both View toggles) is a **human** step —
  no engine binary or display on the build host. Detail:
  `docs/fix-log/2026-09-08-proto-06-yixin-board-overlay.md`.

## Lessons

- **The GUI was architected for a streaming feed it never subscribed to** (Sprint 17 lesson) —
  PROTO-06 consumed that feed for rendering, and the up-front check in
  `docs/notes/2026-09-08-rapfi-engine-realtime-output.md` (verify each stream a stock Rapfi
  actually emits before building UI on it) is exactly what made decision (a) possible: `POS`/`DONE`
  need `aspiration_window=false` and were scoped out, `LOST`/`BEST`/`REFRESH` + `INFO PV DONE` are
  what's real. Do this verification before, not after, building the consumer.
- **The build host has no engine binary and no display.** The "watch the overlay update in a
  running app" acceptance is a human step; the automated regression replayed constructed
  transcripts and asserted real per-cell state (tags per depth, stale-tag cleanup, `REFRESH`
  clearing only `pos`), not "the parse path exists". Still true, carried forward.
- **A second model→ui update channel stays parallel and minimal.** `signal_analysis_overlay` was
  added next to `signal_analysis` with its own dirty flag emitted from the *existing* RT-01 tick —
  no second timer/idle, no folding into `setAnalysisData`. The Sprint 17 `sigc::track_obj` /
  `disconnect()` rule for recurring `signal_timeout` / `signal_idle` connects held; the owed
  `src/ui/` audit sweep of every such connect is still a future `CLEAN` item.

## Rolled over to Backlog

Nothing rolled over — the one committed item finished.

## Next sprint

Sprint 19 — not yet opened; the Backlog is currently empty. Run
`/sprint open 19 "<goal>" <CODE...>` once new work is filed and committed. Release `v0.7.0` cut at
close (see `CHANGELOG.md`).
