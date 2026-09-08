# 2026-09-08 — PROTO-06: port the Yixin-Board live per-cell search overlay

## Prompt

PROTO-05 shipped the incremental analysis stream but the user's expectation was the *original
Yixin-Board board overlay* — per-cell winrate/mate tags fed by `INFO PV DONE` (with per-depth
stale cleanup) plus the `REALTIME` "thinking" feed (`LOST`/`BEST`/`REFRESH`, and `POS`/`DONE` for
Yixin engines) — which was never ported. `parseMessage`'s REALTIME branch received those lines
(since `SHOW_DETAIL 3`) but discarded everything except `BEST`/`PV`/`VAL`. Design was fully
resolved with the user 2026-09-08 (`docs/todo/PROTO-06-*.md` "Resolved decisions", decision (a):
implement only what a stock Rapfi actually emits; parse `POS`/`DONE` but make no feature depend on
them; do not touch Rapfi's search config).

## Action

Not a bug fix in the diagnosis-pipeline sense — a scoped feature port off a resolved design — but
logged here per the project rule that every code change lands a fix-log entry + regression test.

- **`src/engine/engine_types.h`** — new `AnalysisOverlayCell` (`tag` / `tagDepth` / `tagWinrate`
  / `pos` 0..2 / `lost`) + `AnalysisOverlay` (`std::map<Coord, AnalysisOverlayCell> cells` +
  `Coord bestMove`; `empty()` / `clear()` / `clearPos()`).
- **`src/engine/i_engine_protocol.h`** — new `signal_analysis_overlay(const AnalysisOverlay&)`,
  parallel to `signal_analysis`, deliberately independent of it.
- **`src/engine/gomocup_protocol.{h,cpp}`** — `overlay_` member; `parseMessage` REALTIME branch
  now handles `POS`→`pos=2`, `DONE`→`pos=1`, `LOST`→`lost=true`, `REFRESH`→`clearPos()`, and
  `BEST` also sets `overlay_.bestMove`; each mutating line calls `emitOverlay()`. `onPVDone()`
  stamps `pv.moves[0]` with `overlayTagText(pv)` (`+M`/`-M`/`D`/`NN%` capped 1..99) +
  `currentStatus_.depth` + backing winrate, then on the last PV of a round
  (`currentPVIndex_ + 1 == currentNumPV_`) clears tags whose `tagDepth < currentStatus_.depth`.
  `clearAnalysisState()` clears `overlay_`. All coords via the existing `parseEngineCoord`.
- **`src/model/game_state.{h,cpp}`** — owns `analysisOverlay_` + `overlayDirty_`;
  `setAnalysisOverlay()` (dirty flag only — never routed through `setAnalysisData`),
  `clearAnalysisOverlay()` (clear + synchronous `signal_analysis_overlay`), `analysisOverlay()`
  accessor, `signal_analysis_overlay`. `tickAnalysis()` / `flush()` consume `overlayDirty_` on the
  same RT-01 75 ms coalescing point as `analysisDirty_`. `resetAnalysisState()` clears the overlay
  on every position change (added to the `alreadyEmpty` guard too).
- **`src/engine/engine_controller.cpp`** — forwards `protocol_->signal_analysis_overlay` →
  `gameState_.setAnalysisOverlay()` behind the same `isAnalyzing()` UI-04 gate as
  `signal_analysis`; calls `gameState_.clearAnalysisOverlay()` on the search-completion coordinate
  (`signal_move` `wasSearching` branch), in `stopAnalysis()`, and in the `signal_process_died`
  handler.
- **`src/model/board_view_model.{h,cpp}`** — `candidateMoves` (+ its populate loop) deleted; new
  `searchOverlay` (`SearchOverlayMark{ pos, Kind, label, winrate }`), resolved single-winner per
  empty cell (`tag > lost > best > pos==1 > pos==2`) in `update()` ONLY while
  `state_.isAnalyzing()` and `viewConfig.showSearchOverlay`; `showSearchWinrate == false` drops
  just the `Tag` layer, letting the next-priority mark win.
- **`src/ui/board_renderer.{h,cpp}`** — `drawCandidateMoves` → `drawSearchOverlay` in the same
  pipeline slot; `Tag` keeps the `set_source_from_winrate` HSV heat circle + label, `Lost` = red
  ring + diagonal cross, `Best` = cyan disc + ring, `Examined`/`Examining` = small grey/amber
  dots.
- **`src/model/config.h` + `src/model/settings_storage.cpp`** — `ViewConfig::showSearchOverlay` /
  `showSearchWinrate` (both default `true`), persisted as `show_search_overlay` /
  `show_search_winrate`.
- **`src/main_window.{h,cpp}`** — two `Gio::SimpleAction` bool actions
  (`win.show-search-overlay` / `win.show-search-winrate`) in a new **View** menu;
  `onToggleSearchOverlay` / `onToggleSearchWinrate` (push to `ViewConfig`, `persistGameSetup()`,
  refresh board), `syncSearchOverlayMenu()` seeded after `SettingsStorage::load()` and re-synced
  on `signal_config_changed`; `signal_analysis_overlay` wired to
  `boardViewModel_.update()` + `boardView_.queueRedraw()`.

Untouched (per Boundaries): Rapfi engine config / commands, `SHOW_DETAIL`/`YXSHOWINFO`/`YXNBEST`/
`YXBOARD`, the `MESSAGE (n)` → `pvLines_` / `PVView` / WinGraph path, `signal_engine_analysis` /
`setAnalysisData`, `drawPVHighlight` / `pvPreview`.

## Verification

- **New:** `tests/test_proto06_analysis_overlay.cpp` (6 cases, `ranls-gui-ui-tests`):
  1. INFO stream, 3 rounds × multiPV 3 — per-cell `tag` + `tagDepth`; a move dropping out of the
     top-N has its tag cleared exactly on the round it disappears.
  2. REALTIME — `LOST` sets + persists (through `REFRESH`), `BEST` is one cell, `POS`/`DONE`
     set `pos` (synthetic), `REFRESH` clears `pos` only.
  3. Reset — `signal_board_changed` (via `makeMove`) and `clearAnalysisOverlay()` both empty the
     whole overlay.
  4. Coalescing — ~30 overlay lines in one batch → 0 synchronous emits, exactly 1 on the next
     `tickAnalysis()`, 0 on the tick after.
  5. YXNBEST-style block with no `INFO DEPTH` — round depth still taken from the wire
     (`MESSAGE Depth …`), stale tags cleaned at depth 12 not a stale value.
  6. `BoardViewModel::searchOverlay` — live-only gate (empty once `isAnalyzing()` is false) +
     `showSearchOverlay` / `showSearchWinrate` behaviour.
- **Updated:** `tests/test_proto05_incremental_stream.cpp` — `candidateMoves.size()` check →
  count of `Kind::Tag` marks in `searchOverlay`; fixture PV first-moves moved to empty cells
  (the live overlay, like the original, only marks empty intersections).
- Release build (`build_rel`, Ninja) clean; `ctest` 4/4 green — `port02-style-css-bundled`,
  `rel02-version-single-source`, `ranls-gui-tests`, `ranls-gui-ui-tests` (incl.
  `test_rt01_throttle`, `test_proto05_*`, `test_ui04_*`, `test_ui07_*`, `test_anlz06`/`07`,
  `test_game_state`, `test_gomocup_protocol`). PROTO-06 cases: 6/6, 53 assertions.
- A **Debug** build placed in a `/run/media/.../.claude/worktrees/...` worktree trips
  `port02-style-css-bundled` because that guard's "no build-host source path in the binary" regex
  matches the `__FILE__` path Debug info bakes in. Pre-existing property of Debug + a `/run/media`
  build path, unrelated to PROTO-06 — the Release build passes it.
- Manual live-engine run against Rapfi (`show_detail 3`, ~5–10 s: winrate tags per depth, losing
  moves marked, best move highlighted, overlay gone when idle, both View toggles) is **human-owed**
  — no engine or display on the build host.

Branch `proto-06/port-realtime-feed-to-board` — not merged; the orchestrator drives the PR.
