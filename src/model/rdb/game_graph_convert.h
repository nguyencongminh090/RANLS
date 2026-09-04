#pragma once

// RDB-01: VariationTree <-> GameGraph.
//
// This is the ONLY file in src/model/rdb/ that includes variation_tree.h — the
// DTO, CBOR and container layers stay free of the in-memory model.
//
// Scope boundary (RDB-03 lifts it): toGameGraph serialises only the TreeNode
// fields that exist today — move, eval, nodes, depth, comment. A node whose
// eval is NaN emits NO analysis block (never winrate 0 or 0.5). applyGameGraph
// restores only those same fields.

#include "game_graph.h"

#include "../variation_tree.h"
#include "../board_state.h" // GameRule

#include <optional>
#include <string>

namespace rdb {

/// Optional descriptive metadata written into the graph header.
struct GraphMeta {
    std::string            generator;
    std::optional<int64_t> created;
    std::optional<int64_t> modified;
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

} // namespace rdb
