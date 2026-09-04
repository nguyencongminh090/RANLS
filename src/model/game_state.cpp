#include "game_state.h"

#include <cmath>
#include <limits>

GameState::GameState(int boardSize)
    : board_(boardSize)
    , currentTreeNode_(tree_.root())
{
}

void GameState::resetAnalysisState()
{
    // Never clear analysis results out from under an in-flight search on the
    // same position — callers already guard entry with `if (analyzing_) return`
    // before reaching here, but keep this as a defensive discriminator too.
    if (analyzing_) return;

    // Idempotent: skip the clear (and, importantly, the signal emission) if
    // there is nothing to clear. Keeps undoAll/redoAll's per-ply loop from
    // adding a redundant signal_engine_analysis emission on every step once
    // the analysis state is already empty.
    bool alreadyEmpty = pvLines_.empty()
        && engineStatus_.depth == 0 && engineStatus_.selDepth == 0
        && engineStatus_.nodes == 0 && engineStatus_.nps == 0
        && engineStatus_.timeMs == 0 && engineStatus_.winrate == 0.5
        && engineStatus_.mateStep == 0 && engineStatus_.evalText.empty()
        && engineStatus_.bestMove == Coord {};
    if (alreadyEmpty) return;

    pvLines_.clear();
    engineStatus_ = {};
    // Clear any pending coalesced update too — the data it would have
    // delivered is now stale, and we're emitting the clear synchronously
    // right below (position-change correctness path, not throttled: see
    // STATE-01, which shares this signal but is unrelated to RT-01's rate
    // throttle).
    analysisDirty_ = false;
    invalidateEvalHistoryCache();
    signal_engine_analysis.emit();
}

void GameState::newGame(int boardSize)
{
    if (analyzing_) return;

    board_ = BoardState(boardSize);
    history_.clear();
    tree_.clear();
    currentTreeNode_ = tree_.root();
    resetAnalysisState();
    invalidateEvalHistoryCache();
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
    resetAnalysisState();
    currentDatabase_.clear();

    for (const auto &[pos, stone] : stones) {
        if (!pos.isValid(board_.size())) return false;
        if (stone == Stone::Empty) return false;
        if (!board_.placeStone(pos, stone)) return false;

        history_.addMove(pos);
        currentTreeNode_ = tree_.addMove(currentTreeNode_, pos);
    }

    invalidateEvalHistoryCache();
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
    resetAnalysisState();
    invalidateEvalHistoryCache();

    signal_board_changed.emit();
    signal_tree_updated.emit();
    return true;
}

bool GameState::undoMoveSilent()
{
    if (analyzing_ || history_.moveCount() == 0) return false;

    Coord pos = history_.undoMove();
    if (!pos.isValid(board_.size()))
        return false;

    board_.removeStone(pos);

    // Walk the tree node back to parent.
    if (currentTreeNode_ && currentTreeNode_->parent)
        currentTreeNode_ = currentTreeNode_->parent;

    return true;
}

bool GameState::redoMoveSilent()
{
    if (analyzing_) return false;

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

    return true;
}

bool GameState::undoMove()
{
    if (!undoMoveSilent()) return false;

    clearDatabase();

    // Position changed — the previous position's analysis no longer applies.
    resetAnalysisState();
    invalidateEvalHistoryCache();

    signal_board_changed.emit();
    return true;
}

bool GameState::redoMove()
{
    if (!redoMoveSilent()) return false;

    clearDatabase();

    // Position changed — the previous position's analysis no longer applies.
    resetAnalysisState();
    invalidateEvalHistoryCache();

    signal_board_changed.emit();
    return true;
}

// NAV-01: undoAll/redoAll/gotoMove used to loop the single-step undoMove()/
// redoMove(), each of which calls clearDatabase() (-> signal_database_updated)
// and signal_board_changed.emit(). On a 100-move game that meant 100 engine
// database queries and 100 full UI rebuilds for one click of <<. These now
// loop the *Silent() position-mutation halves and do exactly one
// clearDatabase()/resetAnalysisState()/invalidateEvalHistoryCache()/
// signal_board_changed.emit() for the whole bulk operation, only if the
// position actually moved.
void GameState::undoAll()
{
    bool moved = false;
    while (undoMoveSilent()) moved = true;

    if (!moved) return;

    clearDatabase();
    resetAnalysisState();
    invalidateEvalHistoryCache();
    signal_board_changed.emit();
}

void GameState::redoAll()
{
    bool moved = false;
    while (redoMoveSilent()) moved = true;

    if (!moved) return;

    clearDatabase();
    resetAnalysisState();
    invalidateEvalHistoryCache();
    signal_board_changed.emit();
}

void GameState::gotoMove(int moveIndex)
{
    if (analyzing_) return;

    int current = history_.currentIndex();
    bool moved  = false;
    if (moveIndex < current) {
        // Need to undo.
        while (history_.currentIndex() > moveIndex && undoMoveSilent()) moved = true;
    } else if (moveIndex > current) {
        // Need to redo.
        while (history_.currentIndex() < moveIndex && redoMoveSilent()) moved = true;
    }

    if (moved) {
        clearDatabase();
        resetAnalysisState();
        invalidateEvalHistoryCache();
        signal_board_changed.emit();
    }

    // NAV-01: signal_move_selected was declared and connected but never
    // emitted anywhere (dead signal). gotoMove is the operation it exists
    // for — wire it up here, once per call, with the index actually landed
    // on (not the requested one, in case moveIndex was out of range and the
    // loop stopped early). Emitted even when moved is false so a UI click on
    // the already-current move still gets acknowledged/highlighted.
    signal_move_selected.emit(history_.currentIndex());
}

bool GameState::gotoPath(const std::vector<Coord> &path)
{
    if (analyzing_) return false;

    // Validate that the path exists in the current tree.
    if (!tree_.getNode(path)) return false;

    clearDatabase();

    board_.clear();
    history_.clear();
    resetAnalysisState();
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

    invalidateEvalHistoryCache();
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
        // UI-01: write whenever ANY of depth/nodes/eval actually changed —
        // previously this only fired on depth or node-count changes, so a
        // revised score at the same depth/nodes (aspiration-window re-search,
        // final confirmation line) was silently discarded.
        if (current_node->depth != bestPv.depth || current_node->nodes != bestPv.nodes
            || current_node->eval != bestPv.score) {
            current_node->eval = bestPv.score;
            current_node->nodes = bestPv.nodes;
            current_node->depth = bestPv.depth;
            treeChanged = true;
        }
    }

    // UI-13: the best PV also carries an evaluation for the position AFTER the
    // engine's best move. If that position is already a node on the played
    // line / variation tree, fill its eval from the complementary win% (side to
    // move flips for the child) at a derived depth > 0, so evalHistory() plots
    // a point there instead of a NaN "unevaluated" gap. Constraints:
    //  - Never fabricate a node for an un-played PV move (findChild only — the
    //    graph only walks the played line anyway).
    //  - Only fill a child that has no analysis of its own — a real search on
    //    that position always beats this derived estimate.
    //  - Same "only if actually changed" guard + the treeDirty_/cache path as
    //    the current-node write above (falls through to `if (treeChanged)`).
    if (current_node && !pvLines_.empty()) {
        const auto &bestPv = pvLines_[0];
        if (!bestPv.moves.empty()) {
            TreeNode *child = current_node->findChild(bestPv.moves[0]);
            if (child && child->depth <= 0 && child->nodes <= 0) {
                double childEval  = 1.0 - bestPv.score;
                int    childDepth = bestPv.depth > 1 ? bestPv.depth - 1 : 1;
                if (child->eval != childEval || child->depth != childDepth) {
                    child->eval  = childEval;
                    child->depth = childDepth;
                    treeChanged  = true;
                }
            }
        }
    }

    if (treeChanged) {
        invalidateEvalHistoryCache();
        // RT-04: don't emit signal_tree_updated synchronously here — that drove
        // a full rebuild of both tree views on essentially every parsed engine
        // line. Coalesce onto the same tick/flush that RT-01 already uses for
        // signal_engine_analysis instead of inventing a second mechanism.
        treeDirty_ = true;
    }

    // RT-01: coalesce onto tickAnalysis()/flush() instead of emitting per
    // parsed engine line — with multiPV=8 this call happens up to 8x per
    // depth iteration.
    analysisDirty_ = true;
}

bool GameState::tickAnalysis()
{
    if (!analysisDirty_) return false;
    analysisDirty_ = false;
    signal_engine_analysis.emit();
    if (treeDirty_) {
        treeDirty_ = false;
        signal_tree_updated.emit();
    }
    return true;
}

void GameState::flush()
{
    if (!analysisDirty_) return;
    analysisDirty_ = false;
    signal_engine_analysis.emit();
    if (treeDirty_) {
        treeDirty_ = false;
        signal_tree_updated.emit();
    }
}

std::vector<double> GameState::evalHistory() const
{
    if (!evalHistoryDirty_) return evalHistoryCache_;

    evalHistoryCache_.clear();
    evalHistoryCache_.reserve(history_.moveCount());

    const TreeNode *node = tree_.root();
    for (int i = 0; i < history_.moveCount(); ++i) {
        node = node ? node->findChild(history_.moves()[i]) : nullptr;
        // UI-01: a missing node or a node with no analysis yet is NOT a
        // genuine 50% evaluation — use NaN as an "unevaluated" sentinel so
        // the graph can render a visible gap instead of a false dead-even
        // reading. Consumers must check std::isnan() before using a value.
        if (!node) {
            evalHistoryCache_.push_back(std::numeric_limits<double>::quiet_NaN());
            continue;
        }
        // RDB-03 (D1): trust `node->eval` whenever it is not NaN. `TreeNode::eval`
        // now defaults to NaN, so an unanalysed node is still a genuine gap; a
        // restored node needs nothing but its eval (no reliance on depth/nodes,
        // which a hand-persisted eval may legitimately lack).
        evalHistoryCache_.push_back(std::isnan(node->eval)
                                         ? std::numeric_limits<double>::quiet_NaN()
                                         : node->eval);
    }
    evalHistoryDirty_ = false;
    return evalHistoryCache_;
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

void GameState::setMatchConfig(const MatchConfig &config)
{
    matchConfig_ = config;
    signal_config_changed.emit();
}
