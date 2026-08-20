#include "game_state.h"

GameState::GameState(int boardSize)
    : board_(boardSize)
    , currentTreeNode_(tree_.root())
{
}

void GameState::newGame(int boardSize)
{
    if (analyzing_) return;

    board_ = BoardState(boardSize);
    history_.clear();
    tree_.clear();
    currentTreeNode_ = tree_.root();
    pvLines_.clear();
    engineStatus_ = {};
    currentDatabase_.clear();
    signal_board_changed.emit();
    signal_tree_updated.emit();
    signal_database_updated.emit();
}

bool GameState::loadPosition(const std::vector<std::pair<Coord, Stone>> &stones)
{
    if (analyzing_) return false;

    // Reset to empty first (keep board size).
    board_.clear();
    history_.clear();
    tree_.clear();
    currentTreeNode_ = tree_.root();
    pvLines_.clear();
    engineStatus_ = {};
    currentDatabase_.clear();

    for (const auto &[pos, stone] : stones) {
        if (!pos.isValid(board_.size())) return false;
        if (stone == Stone::Empty) return false;
        if (!board_.placeStone(pos, stone)) return false;

        history_.addMove(pos);
        currentTreeNode_ = tree_.addMove(currentTreeNode_, pos);
    }

    signal_board_changed.emit();
    signal_tree_updated.emit();
    return true;
}

bool GameState::makeMove(Coord pos)
{
    if (analyzing_) return false;

    if (!pos.isValid(board_.size())) return false;
    if (board_.stoneAt(pos) != Stone::Empty)
        return false;

    clearDatabase();

    Stone side = board_.sideToMove();
    board_.placeStone(pos, side);
    history_.addMove(pos);

    // Update the variation tree.
    currentTreeNode_ = tree_.addMove(currentTreeNode_, pos);

    // Clear stale analysis data so old candidate markers don't persist.
    pvLines_.clear();
    engineStatus_ = {};

    signal_board_changed.emit();
    signal_tree_updated.emit();
    return true;
}

bool GameState::undoMove()
{
    if (analyzing_ || history_.moveCount() == 0) return false;

    clearDatabase();

    Coord pos = history_.undoMove();
    if (!pos.isValid(board_.size()))
        return false;

    board_.removeStone(pos);

    // Walk the tree node back to parent.
    if (currentTreeNode_ && currentTreeNode_->parent)
        currentTreeNode_ = currentTreeNode_->parent;

    signal_board_changed.emit();
    return true;
}

bool GameState::redoMove()
{
    if (analyzing_) return false;

    clearDatabase();

    Coord pos = history_.redoMove();
    if (!pos.isValid(board_.size()))
        return false;

    Stone side = board_.sideToMove();
    board_.placeStone(pos, side);

    // Walk the tree node forward.
    if (currentTreeNode_) {
        auto *child = currentTreeNode_->findChild(pos);
        if (child)
            currentTreeNode_ = child;
    }

    signal_board_changed.emit();
    return true;
}

void GameState::undoAll()
{
    while (undoMove()) {}
}

void GameState::redoAll()
{
    while (redoMove()) {}
}

void GameState::gotoMove(int moveIndex)
{
    if (analyzing_) return;

    clearDatabase();

    int current = history_.currentIndex();
    if (moveIndex < current) {
        // Need to undo.
        while (history_.currentIndex() > moveIndex && undoMove()) {}
    } else if (moveIndex > current) {
        // Need to redo.
        while (history_.currentIndex() < moveIndex && redoMove()) {}
    }
}

bool GameState::gotoPath(const std::vector<Coord> &path)
{
    if (analyzing_) return false;

    // Validate that the path exists in the current tree.
    if (!tree_.getNode(path)) return false;

    clearDatabase();

    board_.clear();
    history_.clear();
    pvLines_.clear();
    engineStatus_ = {};
    currentTreeNode_ = tree_.root();

    for (const auto &pos : path) {
        if (!pos.isValid(board_.size())) return false;
        if (board_.stoneAt(pos) != Stone::Empty) return false;

        Stone side = board_.sideToMove();
        if (!board_.placeStone(pos, side)) return false;
        history_.addMove(pos);

        auto *child = currentTreeNode_ ? currentTreeNode_->findChild(pos) : nullptr;
        if (!child) return false;
        currentTreeNode_ = child;
    }

    signal_board_changed.emit();
    signal_tree_updated.emit();
    return true;
}

Coord GameState::lastMove() const
{
    if (history_.moveCount() > 0) {
        return history_.moves()[history_.currentIndex()];
    }
    return Coord{};
}

std::vector<Coord> GameState::currentPath() const
{
    std::vector<Coord> path;
    int count = history_.moveCount();
    for (int i = 0; i < count; ++i)
        path.push_back(history_.moves()[i]);
    return path;
}

void GameState::setAnalysisData(std::vector<PVLine> pvs, EngineStatus status)
{
    pvLines_      = std::move(pvs);
    engineStatus_ = status;

    // Update the current node's evaluation with the best engine PV.
    // We intentionally DO NOT add all candidate PVs as child nodes 
    // to prevent cluttering the visual Variation Tree.
    TreeNode *current_node = tree_.getNode(currentPath());
    bool treeChanged = false;

    if (current_node && !pvLines_.empty()) {
        const auto &bestPv = pvLines_[0];
        if (current_node->depth != bestPv.depth || current_node->nodes != bestPv.nodes) {
            current_node->eval = bestPv.score;
            current_node->nodes = bestPv.nodes;
            current_node->depth = bestPv.depth;
            treeChanged = true;
        }
    }

    if (treeChanged) {
        signal_tree_updated.emit();
    }

    signal_engine_analysis.emit();
}

std::vector<double> GameState::evalHistory() const
{
    std::vector<double> out;
    out.reserve(history_.moveCount());

    const TreeNode *node = tree_.root();
    for (int i = 0; i < history_.moveCount(); ++i) {
        node = node ? node->findChild(history_.moves()[i]) : nullptr;
        if (!node) {
            out.push_back(0.5);
            continue;
        }
        out.push_back((node->depth > 0 || node->nodes > 0) ? node->eval : 0.5);
    }
    return out;
}

void GameState::clearDatabase() {
    currentDatabase_.clear();
    signal_database_updated.emit();
}

void GameState::addDatabaseEntry(const DatabaseEntry& entry) {
    currentDatabase_[entry.pos] = entry;
    // We don't signal on every entry to avoid flicker, signal_database_updated is called when done
}

void GameState::updateDatabase() {
    signal_database_updated.emit();
}

void GameState::setRule(GameRule rule)
{
    rule_ = rule;
    signal_rule_changed.emit();
}

void GameState::setEngineConfig(const EngineConfig &config)
{
    engineConfig_ = config;
    signal_config_changed.emit();
}

void GameState::setViewConfig(const ViewConfig &config)
{
    viewConfig_ = config;
    signal_config_changed.emit();
}
