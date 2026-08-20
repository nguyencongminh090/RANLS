---
name: data-architecture
description: Domain data structures for YixinBoard — board/position representation, move history, variation tree, engine analysis data. Use when adding a new data model, extending an existing one (BoardState, VariationTree, PVLine, DatabaseEntry), or reasoning about ownership/addressing of game data. Not for UI data-binding mechanics like Gio::ListStore (see gtk-ui-design) or module layering (see software-architecture).
---

# Data architecture — YixinBoard

All domain data lives in `src/model/`, is plain C++ (no GTK includes), and is owned/mutated only
through `GameState`. Every other layer (`ui/`, `engine/`, `command/`) reads it or asks `GameState`
to change it — never mutates it directly. Keep new data structures inside this boundary.

## The core structures

| Type | File | Shape | Notes |
|---|---|---|---|
| `Coord` | `board_state.h` | position | canonical addressing unit — see below |
| `BoardState` | `board_state.h/.cpp` | grid of `Stone` per `Coord` | current position only, no history |
| `MoveHistory` | `move_history.h/.cpp` | linear sequence | undo/redo along the *current* line, not branches |
| `VariationTree` / `TreeNode` | `variation_tree.h/.cpp` | tree, `unique_ptr` children + raw `parent` | full branching move tree — see below |
| `PVLine` / `EngineStatus` | `engine/engine_types.h` | flat structs | one engine analysis snapshot, MultiPV-indexed |
| `DatabaseEntry` | `engine/engine_types.h` | struct, keyed by `Coord` in `GameState::currentDatabase_` | engine's opening-book/eval database entries |

## `VariationTree`: ownership and addressing

```cpp
struct TreeNode {
    Coord move;
    std::vector<std::unique_ptr<TreeNode>> children;  // owns
    TreeNode *parent = nullptr;                         // non-owning, back-reference only
};
```

- **The tree owns nodes via `unique_ptr`; `parent` is a bare non-owning pointer.** Never store a raw
  `TreeNode*` across an operation that could rebuild the tree (`clear()`, `loadPosition()`) — it will
  dangle. `GameState` itself does this wrong-looking-but-intentional thing carefully: `currentTreeNode_`
  is a raw pointer, but it's only ever set immediately before use and reset on every structural change.
- **Canonical addressing is `std::vector<Coord>` (a path from root), not a node pointer.** `getNode()`,
  `getBranchCoords()`, `gotoPath()`, and every UI signal that identifies a tree position
  (`signal_node_clicked`, `signal_node_selected`) pass a path, not a `TreeNode*`. This is deliberate:
  paths survive a tree rebuild (re-derive the node by walking from root), pointers don't. **Any new
  code that needs to remember "a place in the tree" across a signal boundary or an async callback
  must use a path, not a cached pointer.**
- The root node is a sentinel with no move (`VariationTree::root()`) — its children are the game's
  first moves. Don't special-case "first move" logic against `parent == nullptr`; check against the
  root sentinel instead if you need to distinguish "no parent" from "is the first move."
- `addMove()` is idempotent per parent+move (`findChild` first) — adding the same move twice under
  the same node returns the existing child rather than duplicating. New tree-mutation code should
  preserve this (don't bypass `addChild`/`addMove` to push into `children` directly).

## Engine analysis data: snapshot, not accumulated history

`PVLine`/`EngineStatus` (`GameState::pvLines_`, `engineStatus_`) hold only the *current* analysis —
each new `INFO`/`MESSAGE` batch from the engine replaces them wholesale via
`GameState::setAnalysisData()`, it doesn't merge into prior state. If a feature needs eval *history*
across moves (e.g. the win-rate graph), that's `GameState::evalHistory()` deriving from
`MoveHistory`/`VariationTree` node `eval` fields already stored per-node, not from `pvLines_`. Don't
conflate "current analysis" state with "historical eval" state — they're different lifetimes.

## Extending the data model

- New per-move data (an annotation, a custom rule flag, etc.) belongs as a field on `TreeNode`
  alongside `eval`/`nodes`/`depth`/`comment`, addressed by path — not as a separate
  `std::map<Coord, T>` unless the data is genuinely position-keyed rather than move-in-tree-keyed
  (that distinction matters: two different lines can reach the same `Coord` sequence via different
  paths in Renju/Standard rules with transpositions... actually transpositions aren't merged here,
  each path is its own node — confirm this assumption against `addMove`/`findChild` before relying
  on it for new code).
- New signals for new data follow `GameState`'s existing pattern: a `sigc::signal<void()>` (or
  `void(T)` for data that must travel with the signal) emitted after the mutating method changes
  state — see `signal_board_changed`, `signal_tree_updated` for the shape to copy.
