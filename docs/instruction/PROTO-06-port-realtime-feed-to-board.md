# PROTO-06 — replace analysis board overlay with the Yixin-Board model

All design questions are resolved (detail file "Resolved decisions"). This is now an
implementation task, not a design one. Two source notes are the spec:
`docs/notes/2026-09-08-refyxb-multipv-rendering.md` (reference behaviour) +
`docs/notes/2026-09-08-rapfi-engine-realtime-output.md` (what Rapfi actually emits — decision (a)).

## Approach

1. `software-architecture` skill first — this adds a **second model→ui update channel** next to
   `signal_engine_analysis`. Keep it parallel and minimal: `overlayDirty_` flag + emit from the
   existing `GameState::tickAnalysis()` / `flush()`, a `signal_analysis_overlay`. Do not fold it
   into `setAnalysisData`.
2. `gtk-ui-design` skill for the new `BoardRenderer` layer (single-winner per-cell, replaces
   `drawCandidateMoves` in the same pipeline slot).
3. Build order: overlay struct + `GameState` plumbing → REALTIME parse → `onPVDone` tag/depth
   write + round-end cleanup → reset wiring → renderer swap → `ViewConfig` toggles + menu +
   persistence → tests.
4. Match the reference exactly: `RefYXB/Yixin-Board/main.c` REALTIME parse `:6408–6460`, INFO
   parse `:6477–6600`, render priority `:705–716`, resets `:951–953` / `:2946–2948` /
   `clear_board_tag :583`.

## Pitfalls

- **Never `queue_draw()` per engine line.** `REALTIME LOST`/`BEST` + `INFO PV DONE` can arrive
  dozens/sec. Set `overlayDirty_`, let the 75 ms tick coalesce → one `BoardViewModel::update()` +
  `queue_draw()`. Verify with a burst transcript that redraws stay ~13 Hz.
- **Do NOT reuse `setAnalysisData`.** It does `invalidateEvalHistoryCache()` + tree-node eval
  writes + `treeDirty_`. The overlay is pure view state.
- **Coords: reuse `parseEngineCoord`.** `REALTIME`/`INFO BESTLINE`/bestmove all go through the same
  `CoordText` + `ioCoordMode` path; the existing `(n)` / bestmove parsing is already correct, so
  the overlay coords need no special axis handling. (The GUI assumes Rapfi runs with
  `coord_conversion_mode = flipY_X` in its `config.toml`, or a native Yixin engine — that is a
  pre-existing assumption, not PROTO-06's concern.)
- **`POS` / `DONE` will not arrive from stock Rapfi** (aspiration window). Parse them for the Yixin
  engine, but the acceptance for Rapfi is tags + `LOST` + `BEST` + `REFRESH`. Don't build a
  "cells light up as examined" demo expectation for Rapfi.
- **`REFRESH` clears `pos` only** — not `lost`, not `tag`, not `bestMove`. And it fires **per PV
  per depth**, not once per depth (Rapfi `printPvCompletes` loop). Cheap, but don't assume 1/depth.
- **Stale-tag cleanup key.** On the last PV of a round (`currentPVIndex_ + 1 == currentNumPV_` in
  `onPVDone`), clear `tag` where `tagDepth < currentStatus_.depth`. `printRootMoves` (YXNBEST path)
  omits `INFO DEPTH` from its block — make sure `currentStatus_.depth` is still the right round
  depth there (it is maintained from the `MESSAGE (n)` `SD` field / other INFO). Test the YXNBEST
  transcript specifically.
- **Live-only gate.** `BoardViewModel::update()` copies overlay → render fields only while
  `state_.isAnalyzing()`. On search end the overlay must vanish; confirm no stale marks survive a
  one-shot analyze that converged (this is the STATE-01 / UI-04 failure class — wire the clear
  into `resetAnalysisState()` and the `signal_board_changed` handler EngineController already has).
- **Deleting `candidateMoves`.** Grep every reader: `board_view_model.cpp`, tests
  (`test_ui07_*`, `test_game_state.cpp` reference `pvLines`, check for `candidateMoves`), any
  `BoardRenderer` test. Remove/rewrite them — don't leave a dangling field.
- **Two toggles, not one.** `showSearchOverlay` (master) and `showSearchWinrate` (tags only) —
  mirror Yixin-Board `showanalysis` / `showanalysiswinrate`. Both default on, both persisted.
- **`STATE-03` interaction.** `commitPV`'s `(1)` round truncation and `onPVDone`'s `INFO NUMPV`
  resize already handle `currentPVs_` shrink. The overlay's per-cell `tag` map is separate — its
  own stale cleanup is the depth check, plus a full clear on reset. Don't couple them.

## Verification before done

- `tests/test_proto06_analysis_overlay.cpp` (new):
  - INFO stream: 3 depth rounds, multiPV 3 → assert per-cell `tag` + `tagDepth`, and that a move
    dropping out of the top-N has its tag cleared after the round it disappears.
  - REALTIME: `LOST` sets + persists; `BEST` sets single cell; `REFRESH` clears only `pos`;
    `POS`/`DONE` update `pos` (synthetic — Rapfi won't send them).
  - Reset: `signal_board_changed` / search-end clears the whole overlay.
  - Coalescing: N lines in one batch → one overlay-dirty → one emit.
- Manual (human, no engine on build host): analyse ~5–10 s against Rapfi with `show_detail 3`;
  confirm winrate tags per depth, losing moves marked, best move highlighted, overlay gone when
  idle; toggle both menu items.
- `ctest` fully green — `test_rt01_throttle`, `test_proto05_*`, `test_ui04_*`, `test_ui07_*`,
  `test_anlz06/07`, plus whatever referenced `candidateMoves`.

## Boundaries

- REALTIME parse + overlay model + one renderer layer (replacing `drawCandidateMoves`) + 2 toggles
  + menu + persistence. Nothing else.
- No `setAnalysisData` routing; no `signal_engine_analysis` change.
- No engine-config change (no `aspiration_window`, no new command) — decision (a).
- No change to `pvLines_` / `PVView` / WinGraph / `MESSAGE (n)` parsing.
- No change to PROTO-05's `SHOW_DETAIL` / `YXSHOWINFO` / request shape.
