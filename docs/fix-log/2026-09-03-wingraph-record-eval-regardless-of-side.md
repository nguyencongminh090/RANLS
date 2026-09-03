# 2026-09-03 — WinGraph: record the returned win% for every analysed position, regardless of side (UI-13)

Tracked task UI-13. Trace + candidate fixes were pre-filed in
`docs/todo/UI-13-wingraph-record-eval-regardless-of-side.md` (systematic-debugging,
2026-09-03). Scope was narrowed with the user to **candidate A (write the child
node from the best PV)** plus the `flush()`-before-move ordering fix — candidates
B and C explicitly deferred.

## Prompt

> "WinGraph should always record percent whenever the engine analyses and returns
> a percentage, no matter whose side." The win graph shows NaN gaps on one side's
> plies, especially under "Engine plays <side>" auto-play.

Spec: `docs/todo/UI-13-*.md` (Candidate fixes, Acceptance criteria, Scope
boundary) + `docs/instruction/UI-13-*.md` (Pitfalls, Boundaries).

## Investigation

Started from the todo file's "Investigation still to do" list (data path already
traced there — not re-derived).

1. **Reproduce (headless, via the model layer).** Drove `GameState::setAnalysisData`
   for a sequence of positions the way `EngineController` does, mirroring the
   "engine only searches its own turns" pattern (search roots at plies 0 and 2,
   each returning a best line into the next ply). `evalHistory()` returned NaN for
   every ply that was never itself a search root — including the plies the engine
   *did* evaluate implicitly as the position after its best move.
2. **Confirm the write target.** `setAnalysisData` writes `bestPv.score` onto
   exactly one node: `tree_.getNode(currentPath())` — the searched position. The
   position after `bestPv.moves[0]` (one ply deeper, opposite side to move) is
   never touched, so it stays at the UI-01 "unevaluated" NaN sentinel and renders
   as a gap.
3. **Ordering.** `EngineController`'s `protocol_->signal_move` handler ran
   `setState(EngineState::Idle)` — which synchronously fires `signal_state_changed`
   handlers (auto-move scheduling, control-sensitivity toggles) — *before*
   `gameState_.flush()` and *before* `signal_engine_move` played the move, so a
   state-change handler could run against a half-updated world and the final
   analysis for the searched position was delivered later than the state
   transition that consumers react to.

Root cause: `setAnalysisData` records the eval of the search root only; the
best PV's implied eval for the very next ply is discarded.

## Fix

### 1. `src/model/game_state.cpp` — `GameState::setAnalysisData`

After the existing current-node write, if `pvLines_[0].moves` is non-empty and
`current_node->findChild(bestPv.moves[0])` is already a node on the tree, fill
that child's eval:

- `child->eval = 1.0 - bestPv.score` (side to move flips for the child).
- `child->depth = max(bestPv.depth - 1, 1)` — a derived depth > 0 so
  `evalHistory()` does not treat it as unevaluated.
- Only when the child has **no analysis of its own** (`depth <= 0 && nodes <= 0`)
  — a real search on that position always beats the derived estimate.
- Same "only if actually changed" guard as the current-node write; sets the same
  `treeChanged` flag, so it flows through the existing
  `invalidateEvalHistoryCache()` + `treeDirty_` path (RT-04 coalescing intact).
- Never calls `addChild` — an un-played PV move is not fabricated as a node
  (the graph only walks the played line anyway).

### 2. `src/engine/engine_controller.cpp` — `protocol_->signal_move` handler

Reordered to: `setAnalyzing(false)` → `flush()` → `signal_engine_move.emit(move)`
→ `setState(EngineState::Idle)`. The final coalesced analysis for the searched
position is now delivered while the board is still on that position and before
any `signal_state_changed` handler runs; the state transition (and the auto-move
/ sensitivity handlers it drives) happens last, after the move is on the board.
RT-01 guarantees unchanged: still exactly one immediate `flush()` on completion,
no wait for the next throttle tick.

## Tests

New `tests/test_ui13_wingraph_eval_coverage.cpp` (model layer, gtkmm-free, wired
into the `rapfi-gui-tests` ctest suite), 4 cases:

- child node on the played line is filled from the best PV (complementary win%,
  depth > 0); `evalHistory()` has no NaN gap at that ply.
- a child with its own real (deeper) analysis is **not** overwritten.
- an un-played PV move is never fabricated as a tree node.
- full-line sweep: no NaN where the engine supplied a score (directly or via the
  PV child-fill); a genuinely never-searched ply still reads NaN, not a false 0.5.

The `EngineController` reorder is a pure statement reorder inside one signal
handler; it has no gtkmm-free harness that feeds analysis + a move through a real
`EngineController` (`protocol_` is privately owned; the ENG-01/ENG-02 tests use
real short-lived subprocesses that emit neither). Guarded by the ENG-01/ENG-02
controller tests and the RT-01 throttle tests staying green.

## Verification

- `./build.sh` (clean `build_cmd`): success, no new warnings (3 pre-existing
  `-Wunused-function` warnings in `gomocup_protocol.cpp` only, untouched).
- `ctest` — model suite `rapfi-gui-tests`: 141 cases / 1112 assertions, all pass
  (was 137 cases; +4 UI-13). UI suite `rapfi-gui-ui-tests`: 14 cases / all pass.
  `rel02-version-single-source`: pass. 3/3 ctest.
- Acceptance criteria: a position the engine scored during a search now has a
  plotted point whether it was the search root or the position after the best
  move, independent of `MatchConfig::enginePlays`; genuinely unsearched plies
  still render as gaps (NaN, not 0.5); UI-01 attribution and
  `buildWinGraphSeries` perspective logic untouched.

## Deliberately out of scope

- Candidate B (auto-analyse the reply position after an engine auto-move) and
  candidate C (persist a `WINRATE`-only line with empty `pvLines_`).
- "Evaluate the whole played line" — filling every ply, not just the immediate
  reply to a search root. Recommended as a separate Backlog item (see below).
- eval→win% maths, UI-01 attribution, UI-09 `enginePlays` decoupling, RT-01
  cadence, graph axes / layout / drawing.

## Follow-up recommended for Backlog

`GRAPH-xx` — evaluate the whole played line: under "Engine plays <side>" the
engine still never searches the opponent's turns, so plies two-or-more deep from
a search root remain gaps. Options: a bounded background sweep that analyses each
un-scored ply briefly, or persisting deeper PV nodes (needs a "derived vs real"
marker on `TreeNode` and a decay/refresh policy).
