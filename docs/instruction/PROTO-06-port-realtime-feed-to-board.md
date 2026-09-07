# PROTO-06 — port-realtime-feed-to-board

## Approach

1. **Design gate first.** Walk the 6 open questions in `docs/todo/PROTO-06-*.md` with the user
   before any code. Recommended defaults: Q1 → dedicated `RealtimeSearchState` struct owned by
   `GameState`; Q2 → new `signal_realtime` coalesced on the existing RT-01 75 ms tick
   (`GameState::tickAnalysis` already the single coalescing point — add a `realtimeDirty_` flag
   next to `analysisDirty_`); Q3 → reference priority (winrate tag > lost > best > pos); Q4 → wire
   into `resetAnalysisState()` / `clearAnalysisState()`; Q5 → `ViewConfig` flag default on + a
   "Show search overlay" menu item; Q6 → add the distinct current-best highlight here (cheap,
   `currentStatus_.bestMove` already parsed).
   If the answers diverge much, promote to `features/realtime-search-overlay/` before filing code.
2. Use the `software-architecture` skill for Q1/Q2 (this crosses engine → model → ui and adds a
   second model→ui update channel next to `signal_engine_analysis`), and `gtk-ui-design` for the
   new `BoardRenderer` layer.
3. Reference implementation to match: `RefYXB/Yixin-Board/main.c` — REALTIME parsing at
   `:6408–6460`, per-cell arrays `boardpos` / `boardlose` / `boardbestX/Y`, render-priority
   selection in `refresh_board_at` at `:705–712`.

## Pitfalls

- **Do NOT reuse the `setAnalysisData` path.** `GameState::setAnalysisData` does tree-node eval
  writes + `invalidateEvalHistoryCache()` + `treeDirty_`. Routing a per-move `REALTIME POS` stream
  through it would rebuild eval history hundreds of times per second. The realtime overlay is
  pure view state — new dirty flag + new signal, coalesced on the same tick, nothing else.
- **Coordinate order.** Reference parses `REALTIME POS y,x` as `sscanf("%d,%d", &y, &x)` —
  row then column. Rapfi's `CoordText` output for realtime is `x,y` in engine coords; check what
  `parseEngineCoord` already assumes (the MESSAGE `(n)` / Bestline paths use it) and stay
  consistent — one wrong axis silently mirrors the whole overlay.
- **`REFRESH` clears only `pos`, not `lost`.** Lost moves persist within a search; only a new
  position / search end / stop clears them.
- **High redraw rate.** Never `boardView_.queueRedraw()` directly from the protocol/controller per
  `REALTIME` line. The coalesced `signal_realtime` → one `BoardViewModel::update()` +
  `queue_draw()` per tick. Confirm with a burst transcript that redraws stay ~13 Hz.
- **`clearAnalysisState` is wired to `signal_board_changed`** (EngineController). The realtime
  carrier must be reset on the same signal or a stale overlay survives a position change (the
  STATE-01 / UI-04 class of bug).
- **Compositing with translucent candidate circles.** `drawCandidateMoves` fills a 0.55-alpha
  disc. Decide whether the POS/LOST marks draw under or over it; test legibility on a cell that
  has both a winrate label and a POS mark.
- **PROTO-05's regression test** (`tests/test_proto05_incremental_stream.cpp`) feeds a NORMAL +
  detail transcript — make sure adding REALTIME parsing doesn't change how that test's non-REALTIME
  lines are handled.

## Verification before done

- New test `tests/test_proto06_realtime_overlay.cpp`: feed `REALTIME POS`, `REALTIME DONE`,
  `REALTIME LOST`, `REALTIME REFRESH` lines; assert the carrier's per-cell state, that REFRESH
  clears only the examining cells, and that a `signal_board_changed` / search-end clears all.
- Manual (human, no engine on build host): analyse a position ~5 s against Rapfi with
  `show_detail 3`, confirm cells light up as the engine explores, losing moves get marked, the
  overlay resets each depth, and the winrate labels still update.
- `ctest` fully green — watch `test_rt01_throttle`, `test_proto05_*`, `test_ui04_*`, `test_ui07_*`,
  `test_anlz06/07`.

## Boundaries

- New REALTIME parsing + a view-only realtime-state carrier + one new `BoardRenderer` layer +
  optional toggle. Nothing else.
- No change to `drawCandidateMoves` / `candidateMoves` semantics (compositing order only).
- No `boarddepth` per-cell stale-label tracking for the winrate tags (separate Backlog item).
- No change to `SHOW_DETAIL` / `YXNBEST` / `YXBOARD` (PROTO-05).
- No new engine commands.
