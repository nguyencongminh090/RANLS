#include "game_graph_convert.h"

#include <cmath>
#include <limits>

namespace rdb {

namespace {

void flatten(const TreeNode *node, uint32_t selfIndex, GameGraph &g)
{
    for (const auto &child : node->children) {
        GraphNode gn;
        gn.parent    = selfIndex;
        gn.hasParent = true;
        gn.move      = Move{child->move.x, child->move.y};
        gn.comment   = child->comment;

        if (!std::isnan(child->eval)) {
            NodeAnalysis a;
            a.winrate = child->eval; // encoder tolerates any value; decoder drops
                                     // it if outside [0,1]
            a.depth   = child->depth;
            a.nodes   = child->nodes;
            gn.analysis = std::move(a);
        }

        const auto childIndex = static_cast<uint32_t>(g.nodes.size());
        g.nodes.push_back(std::move(gn));
        flatten(child.get(), childIndex, g);
    }
}

} // namespace

GameGraph toGameGraph(const VariationTree &tree, int boardSize, GameRule rule,
                      const GraphMeta &meta)
{
    GameGraph g;
    g.schema    = kSchemaVersion;
    g.board     = static_cast<uint8_t>(boardSize);
    g.rule      = static_cast<uint8_t>(rule);
    g.created   = meta.created;
    g.modified  = meta.modified;
    g.generator = meta.generator;

    GraphNode root;         // nodes[0] — sentinel
    root.hasParent = false; // no parent, no move
    g.nodes.push_back(std::move(root));

    flatten(tree.root(), 0, g);
    return g;
}

bool applyGameGraph(const GameGraph &g, VariationTree &out, int &boardSize,
                    GameRule &rule, std::string *error)
{
    auto fail = [&](std::string msg) {
        if (error)
            *error = std::move(msg);
        return false;
    };

    if (g.schema > kSchemaVersion)
        return fail("applyGameGraph: payload schema " + std::to_string(g.schema)
                    + " is newer than this build supports ("
                    + std::to_string(kSchemaVersion) + ")");
    if (g.board < 5 || g.board > MAX_BOARD_SIZE)
        return fail("applyGameGraph: board size " + std::to_string(g.board)
                    + " out of range [5, " + std::to_string(MAX_BOARD_SIZE) + "]");
    if (g.rule > 2)
        return fail("applyGameGraph: rule " + std::to_string(g.rule)
                    + " out of range [0, 2]");
    if (g.nodes.empty())
        return fail("applyGameGraph: graph has no nodes (missing root sentinel)");
    if (g.nodes[0].hasParent || g.nodes[0].move.has_value())
        return fail("applyGameGraph: nodes[0] is not a root sentinel");

    // Validate every back-reference before mutating `out`.
    for (size_t i = 1; i < g.nodes.size(); ++i) {
        const GraphNode &n = g.nodes[i];
        if (!n.hasParent)
            return fail("applyGameGraph: node " + std::to_string(i)
                        + " has no parent index");
        if (n.parent >= i)
            return fail("applyGameGraph: node " + std::to_string(i)
                        + " has forward/self parent reference "
                        + std::to_string(n.parent));
        if (!n.move.has_value())
            return fail("applyGameGraph: node " + std::to_string(i)
                        + " has no move");
    }

    out.clear();
    std::vector<TreeNode *> tnodes(g.nodes.size(), nullptr);
    tnodes[0] = out.root();

    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

    for (size_t i = 1; i < g.nodes.size(); ++i) {
        const GraphNode &n      = g.nodes[i];
        TreeNode        *parent = tnodes[n.parent];
        TreeNode        *child  = out.addMove(parent, Coord{n.move->x, n.move->y});

        child->comment = n.comment;
        if (n.analysis) {
            const NodeAnalysis &a = *n.analysis;
            child->depth = a.depth.value_or(0);
            child->nodes = a.nodes.value_or(0);
            child->eval  = (a.winrate && *a.winrate >= 0.0 && *a.winrate <= 1.0)
                               ? *a.winrate
                               : kNaN;
        } else {
            child->eval  = kNaN;
            child->depth = 0;
            child->nodes = 0;
        }
        tnodes[i] = child;
    }

    boardSize = g.board;
    rule      = static_cast<GameRule>(g.rule);
    return true;
}

} // namespace rdb
