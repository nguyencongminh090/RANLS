#pragma once

// RDB-01: VariationTree <-> GameGraph.
//
// This is the ONLY file in src/model/rdb/ that includes variation_tree.h — the
// DTO, CBOR and container layers stay free of the in-memory model.
//
// RDB-03: toGameGraph now serialises the full per-node analysis — winrate,
// depth, nodes, evalText, pv, glyph, engineRef, analyzedUtc — for every node
// whose eval is not NaN. A NaN node still emits NO analysis block (never
// winrate 0 or 0.5). applyGameGraph restores every field, validates
// winrate ∈ [0,1] (else drops the block, leaving eval NaN) and every pv coord
// against the graph's board size (else drops that node's pv), and never aborts
// the load on a bad analysis value.

#include "game_graph.h"

#include "../variation_tree.h"
#include "../board_state.h" // GameRule

#include <optional>
#include <string>
#include <vector>

namespace rdb {

/// Optional descriptive metadata written into the graph header.
struct GraphMeta {
    std::string            generator;
    std::optional<int64_t> created;
    std::optional<int64_t> modified;
    /// RDB-03: display-only engine metadata (name/version/params). Copied
    /// verbatim into GameGraph::engines. A missing/empty list is fine.
    std::vector<EngineInfo> engines;
};

/// Flatten `tree` into a GameGraph in DFS pre-order. nodes[0] is the root
/// sentinel (no move, no parent); every other node carries a parent index that
/// is strictly less than its own. Sibling order is preserved (first child =
/// mainline).
GameGraph toGameGraph(const VariationTree &tree, int boardSize, GameRule rule,
                      const GraphMeta &meta = {});

/// Rebuild `out` from `g`. Validates schema/board/rule ranges and every
/// parent back-reference (parent must be < node index, and in range) before
/// touching `out`. On any problem returns false, sets `*error`, and leaves
/// `out`/`boardSize`/`rule` unmodified. On success `out` is cleared and
/// rebuilt, and `boardSize`/`rule` are set from the graph header.
bool applyGameGraph(const GameGraph &g, VariationTree &out, int &boardSize,
                    GameRule &rule, std::string *error = nullptr);

/// RDB-03: apply one GraphNode's analysis onto an already-created TreeNode.
/// Shared by applyGameGraph and applyGameGraphToState so both restore the exact
/// same field set with the exact same validation:
///   - no analysis block, or winrate absent / outside [0,1]  => eval = NaN,
///     depth/nodes = 0, TreeNode::analysis cleared (never aborts)
///   - otherwise eval/depth/nodes are set and TreeNode::analysis is populated
///     with evalText / glyph / engineRef / analyzedUtc; the pv is restored only
///     if every coord is on-board for `boardSize` (else that pv is dropped).
/// The caller sets `child.comment` (it is a GraphNode field, not analysis).
void applyNodeAnalysis(const GraphNode &n, int boardSize, TreeNode &child);

} // namespace rdb
