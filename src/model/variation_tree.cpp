#include "variation_tree.h"

// ── TreeNode ─────────────────────────────────────────────────────────────────
TreeNode *TreeNode::addChild(Coord childMove)
{
    // Check if the child already exists.
    if (auto *existing = findChild(childMove))
        return existing;

    auto child     = std::make_unique<TreeNode>();
    child->move    = childMove;
    child->parent  = this;
    auto *ptr      = child.get();
    children.push_back(std::move(child));
    return ptr;
}

TreeNode *TreeNode::findChild(Coord childMove) const
{
    for (auto &child : children) {
        if (child->move == childMove)
            return child.get();
    }
    return nullptr;
}

// ── VariationTree ────────────────────────────────────────────────────────────
VariationTree::VariationTree()
    : root_(std::make_unique<TreeNode>())
{
}

void VariationTree::clear()
{
    root_ = std::make_unique<TreeNode>();
}

TreeNode *VariationTree::addMove(TreeNode *parent, Coord move)
{
    if (!parent)
        parent = root_.get();
    return parent->addChild(move);
}

TreeNode *VariationTree::getNode(const std::vector<Coord> &path)
{
    TreeNode *node = root_.get();
    for (const auto &coord : path) {
        node = node->findChild(coord);
        if (!node) return nullptr;
    }
    return node;
}

const TreeNode *VariationTree::getNode(const std::vector<Coord> &path) const
{
    const TreeNode *node = root_.get();
    for (const auto &coord : path) {
        node = node->findChild(coord);
        if (!node) return nullptr;
    }
    return node;
}

std::vector<Coord> VariationTree::getBranchCoords(const std::vector<Coord> &path) const
{
    const TreeNode *node = root_.get();
    for (const auto &coord : path) {
        node = node->findChild(coord);
        if (!node)
            return {};
    }

    std::vector<Coord> coords;
    for (const auto &child : node->children) {
        coords.push_back(child->move);
    }
    return coords;
}
