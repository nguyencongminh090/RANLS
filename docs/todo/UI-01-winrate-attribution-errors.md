# UI-01 — Win-rate graph attributes evals to the wrong side, and evals can go unrecorded

**Status:** ✅ DONE
**Area:** win graph / eval bookkeeping
**Priority:** P1
**Source:** UI/UX + codebase review, 2026-08-21

## Summary

Fixed all three sub-issues:

1. **Side-to-move attribution** — `toDisplayWinrate` (`src/ui/analysis_panel.cpp`) now computes
   `bool blackToMove = (i % 2 == 1)` (was `== 0`). `raw[i]` is the eval of the position *after*
   move `i`; after move 0 (Black's move) it's White to move, so side-to-move is Black only on odd
   `i`.
2. **Eval writeback gating** — `GameState::setAnalysisData` (`src/model/game_state.cpp`) now
   writes whenever depth, nodes, **or** eval differ from the stored values (was: depth-or-nodes
   only), so a revised score at the same depth/nodes (aspiration-window re-search, final
   confirmation line) is no longer discarded. `signal_tree_updated`'s emission condition changes
   the same way as a side effect of sharing the `if`; its *frequency* tuning is RT-04's concern,
   left untouched otherwise.
3. **Unevaluated vs. true 50%** — `GameState::evalHistory` (`src/model/game_state.cpp`) now
   returns `NaN` (via `std::numeric_limits<double>::quiet_NaN()`) for a missing node or a node
   with no analysis (`depth == 0 && nodes == 0`), instead of substituting `0.5`. Consumers
   (`toDisplayWinrate` in `analysis_panel.cpp`, `WinGraphView::onDraw` in
   `src/ui/win_graph_view.cpp`) propagate/check `std::isnan()`: the graph line breaks into
   disjoint segments around NaN runs (pen-up/pen-down), the current-move dot is suppressed for an
   unevaluated current move, and the hover tooltip shows "(no eval)" instead of a percentage.

Mate scores: confirmed by tracing `parseEvalToken` (`src/engine/gomocup_protocol.cpp:121-146`,
clamps `+M`/`−M` to winrate `1.0`/`0.0`) through `setAnalysisData` → `evalHistory` →
`toDisplayWinrate` → `WinGraphView::onDraw`. The clamped 1.0/0.0 is a real (non-NaN) value, so it
renders as a line pinned to the top/bottom axis with a visible dot and a "100.0%"/"0.0%" tooltip —
a meaningful, distinguishable rendering of a forced win/loss, not silently dropped. No code change
was needed for this part; it was a confirmation-only acceptance criterion.

## Verification

- Clean build verified with a fresh from-scratch `build_cmd_clean` directory (Ninja/Release) — no
  errors, no new warnings from the changed files.
- `ctest --output-on-failure`: 66 test cases / 279 assertions, all passing.
- `tests/test_ui01_winrate_attribution.cpp` (new) pins: side-to-move formula per ply (guards the
  exact original inversion), `evalHistory` per-ply raw values, eval overwrite at unchanged
  depth/nodes, eval update when depth/nodes do change (no-regression), NaN for an unanalyzed node
  and a NaN→real transition, and that a genuinely-analyzed 0.5 score still reads back as 0.5 (not
  NaN). `analysis_panel.cpp`'s `toDisplayWinrate` itself is UI-layer (`gtkmm.h`) and is not linked
  into the headless `rapfi-gui-tests` target by design (see `tests/CMakeLists.txt`); the test
  mirrors its one-line attribution formula as a local helper instead, with a code comment
  explaining why direct linkage isn't feasible.
- Fixing this surfaced a pre-existing test breakage: `tests/test_rt01_throttle.cpp`'s "evalHistory
  is cached" test asserted `first == second` on two `vector<double>` reads of an unanalyzed node,
  which is now `[NaN] == [NaN]` — false under `operator==` even though the cache correctly
  returned the identical value both times. Updated that assertion to a NaN-aware element-wise
  comparison rather than weakening or dropping it.

## Problem

### 1. Side-to-move attribution is off by one ply

`toDisplayWinrate` (`src/ui/analysis_panel.cpp:10-30`):

```cpp
double sideToMove = std::clamp(raw[i], 0.0, 1.0);
bool blackToMove = (i % 2 == 0);
```

`raw` comes from `GameState::evalHistory` (`src/model/game_state.cpp:226-241`), which walks the tree
one node per played move — so `raw[i]` is the evaluation of the position **after** move `i`, scored
from the side to move **in that position**.

After move 0 (Black's first move) it is White to move. So for even `i`, the side to move is White,
not Black. The condition is inverted, which flips the black/white series and mirrors the graph
around the 50% line for every other ply.

### 2. Eval writeback is gated on the wrong condition

`GameState::setAnalysisData` (`src/model/game_state.cpp:209-217`):

```cpp
if (current_node->depth != bestPv.depth || current_node->nodes != bestPv.nodes) {
    current_node->eval  = bestPv.score;
    ...
}
```

The eval is only written when depth **or** node count changed. An engine that reports a revised
evaluation at the same depth and node count — an aspiration-window re-search, a final confirmation
line — has that new eval discarded. The graph and the tree table then show a stale number for that
node.

The same condition also decides whether `signal_tree_updated` fires, which couples a correctness
question to an update-rate question (see RT-04).

### 3. Unevaluated nodes are indistinguishable from a true 50%

`evalHistory` substitutes `0.5` both for a missing node and for a node with no analysis
(`src/model/game_state.cpp:235-238`). The graph draws that as a genuine dead-even evaluation with no
visual distinction, so a game that was never analyzed renders as a confident flat line at 50%.

## Acceptance criteria

- Black/white series correspond to the actual side to move at each ply; verified by analyzing a
  position with a clearly one-sided evaluation and confirming the line moves in the expected
  direction.
- A new evaluation for the current node is recorded whenever it differs, independent of depth/nodes.
- Positions with no evaluation are visually distinct from evaluated 50% positions (gap in the line,
  distinct marker, or similar) rather than silently plotted as 0.5.
- Mate scores render sanely at both `+M` and `-M` (`mateStep` sign convention,
  `src/engine/engine_types.h:37`) — currently `parseEvalToken` clamps them to winrate 1.0/0.0
  (`src/engine/gomocup_protocol.cpp:129-146`), so confirm the graph does something meaningful at the
  extremes rather than just pinning to the axis.

## Scope boundary

- Update *frequency* of `signal_tree_updated` is RT-04; this item covers only whether the value
  written is correct.
- Graph styling/axis design is not in scope beyond the "unevaluated" distinction.

## Related

- RT-04 (tree update rate shares the same condition), UI-02 (tree table shows the same evals)
