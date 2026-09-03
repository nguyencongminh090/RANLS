# UI-13 — WinGraph: record the returned win% for every analysed position, regardless of side to move

**Status:** ✅ FIXED

**Resolution (2026-09-03):** Implemented candidate **A** (write the reply-ply child
node from the best PV) + the `flush()`-before-move ordering fix. Candidates B and C
and the broader "evaluate the whole played line" ambition were deferred with the
user (Backlog follow-up recorded in the fix-log detail).

- `src/model/game_state.cpp` `setAnalysisData`: when `pvLines_[0].moves[0]`'s child
  is already a node on the played line and has no analysis of its own, fill it with
  the complementary win% (`1 - bestPv.score`) at `depth = max(bestPv.depth-1, 1)` —
  same "only if changed" guard + `treeChanged`→`invalidateEvalHistoryCache()` /
  `treeDirty_` path as the existing current-node write. Never fabricates a node for
  an un-played PV move.
- `src/engine/engine_controller.cpp` `protocol_->signal_move`: reordered to
  `setAnalyzing(false)` → `flush()` → `signal_engine_move` → `setState(Idle)`, so
  the searched position's final analysis is delivered before the board advances and
  before any `signal_state_changed` handler runs. RT-01 unchanged (one immediate
  flush on completion, no tick wait).
- Boundaries honoured: eval→win% maths, UI-01 attribution, `buildWinGraphSeries`
  perspective logic, UI-09 `enginePlays` decoupling, RT-01 cadence, and graph
  axes/layout/drawing all untouched.

**Verification:** `./build.sh` clean (no new warnings — 3 pre-existing
`-Wunused-function` in `gomocup_protocol.cpp` only). `ctest` 3/3: model suite
`rapfi-gui-tests` 141 cases / 1112 assertions (+4 new UI-13); UI suite
`rapfi-gui-ui-tests` 14 cases; `rel02-version-single-source` pass. Regression test:
`tests/test_ui13_wingraph_eval_coverage.cpp` (4 cases, wired into the model ctest
suite). Detail: `docs/fix-log/2026-09-03-wingraph-record-eval-regardless-of-side.md`.

---

_Original filing (2026-09-03):_ 🔲 OPEN (investigation + fix)
**Area:** `src/model/game_state.cpp` (`setAnalysisData`, `evalHistory`), `src/engine/engine_controller.cpp`
(`connectProtocolSignals` — the `signal_analysis` / `signal_move` handlers),
`src/ui/analysis_panel.cpp` (`connectSignals`, `toDisplayWinrate`). Read-only reference:
`src/ui/win_graph_series.h`, `src/ui/win_graph_view.cpp`.
**Priority:** P2
**Source:** User report, 2026-09-03 — "WinGraph should always record percent whenever the engine
analyses and returns a percentage, no matter whose side."
**Depends on / relates to:** UI-01 (win-rate attribution + NaN "unevaluated" sentinel), UI-09
(SingleSide is always Black; `enginePlays` decoupled), RT-01 (analysis-signal throttle), UX-06
(WinGraph modes). Do **not** re-open UI-01 attribution or UI-09's SingleSide decision.

## Report (verbatim intent)

The win graph shows gaps / missing points on one side's plies. Expectation: whenever the engine
produces a win% for a position, that value is stored and plotted — the side to move in that
position must not gate whether the point is recorded.

## Trace findings (systematic-debugging, 2026-09-03) — read before scoping the fix

**Q1 — Does WinGraph depend on turn / side?**
- **Rendering / attribution: no side gate.** `buildWinGraphSeries` (`src/ui/win_graph_series.h`)
  still takes an `EnginePlaysSide` argument but discards it (`(void)enginePlays;` — UI-09). Its only
  use of "side" is ply parity `blackToMove = (i % 2 == 1)`, which converts a stored
  *side-to-move* eval into Black's perspective (UI-01 attribution). That is correct maths, not a
  gate — every non-NaN entry is plotted.
- **Recording: an indirect turn dependency exists, and it is the actual bug.** `evalHistory()`
  (`game_state.cpp:355`) walks the played line and, per node, returns `node->eval` only when
  `node->depth > 0 || node->nodes > 0`; otherwise NaN ("unevaluated" → visible gap, UI-01).
  Those per-node evals are written **only by `setAnalysisData()`**, and **only onto the single node
  at `currentPath()`** — the position currently being searched:
  ```
  TreeNode *current_node = tree_.getNode(currentPath());
  if (current_node && !pvLines_.empty()) { ... current_node->eval = bestPv.score; ... }
  ```
  So a ply gets a data point only if that exact position was ever the current position while a
  search ran. With **"Engine plays <side>"** auto-play the engine searches almost exclusively on
  its own turns, so the opponent's plies never become `currentPath()` during a search and stay
  NaN → the graph looks like it "skips" one side. The gap is a *coverage* artefact of which
  positions get analysed, not a side check in the graph code.
- **Secondary gap:** even a fully analysed position only writes its *own* node. The engine's
  best PV also implies an eval for the position *after* its best move, but that child node is
  never written, so the very next ply stays NaN until separately analysed.
- **Minor:** `EngineController`'s `signal_analysis` handler early-returns when
  `!gameState_.isAnalyzing()`. `signal_move` sets analyzing false and calls `flush()` *before*
  `signal_engine_move`, so a trailing eval line for the just-produced position can be dropped.

**Q2 — Which components feed WinGraph (data path):**
1. `GomocupProtocol::parseInfo` / `onPVDone` (`gomocup_protocol.cpp`) parse `EVAL` / `WINRATE`
   and emit `signal_analysis(currentPVs_, currentStatus_)` on each `PV DONE`.
2. `EngineController::connectProtocolSignals` forwards to `GameState::setAnalysisData(pvs, status)`
   (guarded by `isAnalyzing()`), which writes `pvLines_[0].score` into the `currentPath()` tree
   node and sets `analysisDirty_` / `treeDirty_`.
3. `MainWindow`'s ~10–15 Hz throttle timer calls `GameState::tickAnalysis()`, and
   `EngineController::signal_move` calls `GameState::flush()` on search completion — both emit
   `signal_engine_analysis`.
4. `AnalysisPanel::connectSignals` handlers for `signal_engine_analysis`, `signal_board_changed`,
   `signal_config_changed` call `gameState_.evalHistory()` → `toDisplayWinrate` →
   `buildWinGraphSeries` → `WinGraphView::setData(...)`.
5. `GameState::evalHistory()` is cached; rebuilt from variation-tree node evals only when
   `evalHistoryDirty_` (position change, or a node eval/depth/nodes change in `setAnalysisData`).

**Q3 — Is WinGraph realtime?** Near-realtime, deliberately throttled (RT-01): updated at the
tick rate (~10–15 Hz) during a search plus one immediate `flush()` at completion — not per parsed
engine line.

## Investigation still to do (confirm before fixing)

1. Reproduce the gap deterministically: (a) manual analyze while stepping through a line, (b)
   "Engine plays Black/White" auto-play a few moves — record which plies end up NaN in each case.
2. Confirm whether Rapfi's analyze output for one position ever carries a usable eval for the
   reply position (BESTLINE / PV), i.e. whether the "also write the child node" option is viable
   without a second search.
3. Decide the intended product behaviour with the user (see options below) — this is a design
   choice, not a pure bug fix.

## Candidate fixes (pick with the user after step 3)

- **A. Write the child node from the PV.** In `setAnalysisData`, if `bestPv.moves` is non-empty,
  also set the eval of the child node for `bestPv.moves[0]` to the complementary win% at a
  derived depth. Cheap, no extra search; fills the "next ply" gap for every analysed position.
- **B. Auto-analyse the resulting position after an engine auto-move** (brief, bounded) so both
  sides get real data during "Engine plays".
- **C. Persist the last engine `WINRATE` even when `pvLines_` is empty** (currently the write is
  gated on `!pvLines_.empty()`), so a `WINRATE`-only line still lands a point.
- Fix the `flush()`-before-move ordering so the final eval line isn't dropped.

## Acceptance criteria

- Every position for which the engine returned a win% during a search has a plotted point (no NaN
  gap), regardless of side to move or `MatchConfig::enginePlays`.
- No false 50% points introduced — genuinely unevaluated plies still render as gaps (UI-01).
- Attribution unchanged: a stored eval is still interpreted as side-to-move-in-that-position and
  converted to Black's perspective for SingleSide.
- Regression test in `tests/` (model layer, gtkmm-free — extend
  `tests/test_ui01_winrate_attribution.cpp` or add a sibling): drives `setAnalysisData` for a
  sequence of positions and asserts `evalHistory()` has no NaN where the engine supplied a score.
- `./build.sh` clean; `ctest` both suites green.

## Scope boundary

- Do not change the eval→win% conversion maths or UI-01 attribution.
- Do not change `buildWinGraphSeries` perspective logic or re-introduce the `enginePlays` coupling
  (UI-09).
- Do not redesign the graph axes/layout or the RT-01 throttle cadence.
- Base the fix on the reported gap only; note any broader "evaluate the whole line" ambition as a
  separate Backlog item.
