#pragma once

#include "board_state.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// RDB-03: the per-node engine analysis that is persisted to `.rdb` but has no
/// home among TreeNode's scalar fields. `eval`/`nodes`/`depth` stay where they
/// are (many call sites); this optional member carries the rest and doubles as
/// the "was this node ever analysed by a real engine search" flag.
struct NodeAnalysisExtras {
    std::string        evalText;         ///< engine eval text, e.g. "+0.53"
    std::vector<Coord> pv;               ///< principal variation from this position
    std::string        glyph;            ///< annotation glyph / label, e.g. "?!"
    int                engineRef   = -1; ///< index into the persisted engine list, -1 = none
    int64_t            analyzedUtc = 0;  ///< analysed-at unix seconds, 0 = unknown
};

/// A node in the variation tree.
/// Each node represents a single move and may have multiple children (branches).
struct TreeNode {
    Coord                                move;
    /// Evaluation at this node. RDB-03: defaults to NaN ("never evaluated") so
    /// GameState::evalHistory() can treat "eval is not NaN" as the single source
    /// of truth. A freshly-added, unanalysed node must read back as a NaN gap,
    /// never a false 0.0 (= catastrophic loss for Black).
    double                               eval       = std::numeric_limits<double>::quiet_NaN();
    int64_t                              nodes      = 0;     ///< Nodes searched
    int                                  depth      = 0;     ///< Search depth
    std::string                          comment;
    std::optional<NodeAnalysisExtras>    analysis;           ///< RDB-03: persisted extras; set iff analysed
    std::vector<std::unique_ptr<TreeNode>> children;
    TreeNode                            *parent     = nullptr;

    /// Add a child branch for the given move. Returns the new child node.
    TreeNode *addChild(Coord childMove);

    /// Find a child with the given move. Returns nullptr if not found.
    TreeNode *findChild(Coord childMove) const;

    /// Does this node have alternative branches (more than 1 child)?
    bool hasBranches() const { return children.size() > 1; }

    /// Is this a leaf node?
    bool isLeaf() const { return children.empty(); }
};

/// Manages the full variation tree for a game.
/// The root node is a sentinel (no move); its children are the first moves.
class VariationTree {
public:
    VariationTree();

    /// Reset the tree.
    void clear();

    /// Get the root sentinel node.
    TreeNode       *root() { return root_.get(); }
    const TreeNode *root() const { return root_.get(); }

    /// Find the node at the end of the given path.
    TreeNode       *getNode(const std::vector<Coord> &path);
    const TreeNode *getNode(const std::vector<Coord> &path) const;

    /// Add a move as a child of the given parent node.
    /// If the move already exists as a child, returns the existing node.
    TreeNode *addMove(TreeNode *parent, Coord move);

    /// Collect all coordinates that have branches at the current position.
    /// Walks the given path to find the node, then reports children coords.
    std::vector<Coord> getBranchCoords(const std::vector<Coord> &path) const;

private:
    std::unique_ptr<TreeNode> root_;
};
