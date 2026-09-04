#pragma once

// RDB-01: the serialisation DTO for a single game's variation tree + analysis.
//
// Plain structs mirroring features/rdb-save-format/diagram/container.md. This is
// the wire model: CBOR <-> GameGraph (game_graph_cbor.*) and VariationTree <->
// GameGraph (game_graph_convert.*) are the two adapters. Deliberately depends on
// nothing but the standard library — no gtkmm, no variation_tree.h, no
// board_state.h — so it is trivially testable and reusable.
//
// Optionals model "field absent on disk". In particular NodeAnalysis absent on a
// GraphNode means the node was never evaluated (=> eval is NaN in the model);
// NodeAnalysis::winrate absent (or out of [0,1] on decode) means no trustworthy
// win probability, never 0 or 0.5.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rdb {

/// Payload schema version (the CBOR "schema" key). Independent of APP_VERSION
/// and of kContainerVersion. Bump when the node/graph key set changes
/// incompatibly (adding optional keys does not require a bump — unknown keys are
/// skipped on read).
inline constexpr uint16_t kSchemaVersion = 1;

struct Move {
    int32_t x = 0;
    int32_t y = 0;

    bool operator==(const Move &) const = default;
};

struct SetupStone {
    int32_t x     = 0;
    int32_t y     = 0;
    uint8_t color = 0; ///< 1 = black, 2 = white (matches Stone)

    bool operator==(const SetupStone &) const = default;
};

struct EngineInfo {
    uint16_t    id = 0;
    std::string name;
    std::string version;
    std::string params;

    bool operator==(const EngineInfo &) const = default;
};

struct NodeAnalysis {
    std::optional<double>  winrate;    ///< "w" — only present if inside [0,1]
    std::optional<int32_t> depth;      ///< "d"
    std::optional<int64_t> nodes;      ///< "n"
    std::string            evalText;   ///< "t"
    std::vector<Move>      pv;          ///< "pv"
    std::optional<uint16_t> engineRef; ///< "e" -> EngineInfo::id
    std::optional<int64_t> analyzedUtc;///< "ts"

    bool operator==(const NodeAnalysis &) const = default;
};

struct GraphNode {
    uint32_t                    parent = 0; ///< "p" — index into GameGraph::nodes
    bool                        hasParent = false; ///< false only for nodes[0]
    std::optional<Move>         move;       ///< "m" — absent on nodes[0]
    std::optional<NodeAnalysis> analysis;   ///< "a" — absent => never evaluated
    std::string                 comment;    ///< "c"
    std::string                 glyph;      ///< "g"
    std::optional<uint64_t>     zobrist;    ///< "z"

    bool operator==(const GraphNode &) const = default;
};

struct GameGraph {
    uint16_t    schema = kSchemaVersion;
    uint8_t     board  = 15;
    uint8_t     rule   = 0; ///< 0 Freestyle, 1 Standard, 2 Renju
    std::optional<int64_t> created;
    std::optional<int64_t> modified;
    std::string generator;
    std::vector<SetupStone> setup;
    std::vector<EngineInfo> engines;
    std::vector<GraphNode>  nodes; ///< flat, DFS pre-order, nodes[0] = root sentinel

    bool operator==(const GameGraph &) const = default;
};

} // namespace rdb
