# UI-01 — Win-rate graph attributes evals to the wrong side, and evals can go unrecorded

**Status:** open
**Area:** win graph / eval bookkeeping
**Priority:** P1
**Source:** UI/UX + codebase review, 2026-08-21

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
