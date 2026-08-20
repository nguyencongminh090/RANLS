#pragma once

#include "board_state.h"
#include "move_history.h"
#include "variation_tree.h"
#include "config.h"
#include "engine/engine_types.h"

#include <sigc++/sigc++.h>
#include <vector>
#include <map>

/// Gomoku game rules.
enum class GameRule {
    Freestyle = 0,   ///< Freestyle Gomoku (rule 0)
    Standard  = 1,   ///< Standard Gomoku / exact-5 (rule 1)
    Renju     = 2    ///< Free Renju with forbidden moves (rule 2)
};


/// Central data model. Holds board, history, tree, and emits signals for UI.
class GameState {
public:
    GameState(int boardSize = DEFAULT_BOARD_SIZE);

    // ── Board manipulation ──────────────────────────────────────────────────
    void newGame(int boardSize = DEFAULT_BOARD_SIZE);
    bool undoMove();
    bool redoMove();
    void undoAll();
    void redoAll();
    void gotoMove(int moveIndex);
    /// Jump to an existing variation path without clearing the variation tree.
    bool gotoPath(const std::vector<Coord> &path);

    /// Load an arbitrary position (YXBOARD-style) into the model.
    /// Rebuilds board/history/tree in the given order and emits board/tree signals.
    /// Returns false if analyzing, invalid coords, or duplicate/occupied cells.
    bool loadPosition(const std::vector<std::pair<Coord, Stone>> &stones);

    // ── Rules ───────────────────────────────────────────────────────────────
    GameRule rule() const { return rule_; }
    void     setRule(GameRule rule);

    // ── Configuration ───────────────────────────────────────────────────────
    const EngineConfig &engineConfig() const { return engineConfig_; }
    void setEngineConfig(const EngineConfig &config);

    const ViewConfig &viewConfig() const { return viewConfig_; }
    void setViewConfig(const ViewConfig &config);

    // ── Accessors ───────────────────────────────────────────────────────────
    const BoardState     &board()   const { return board_; }
    const MoveHistory    &history() const { return history_; }
    const VariationTree  &tree()    const { return tree_; }
    VariationTree        &tree()          { return tree_; }

    int boardSize() const { return board_.size(); }

    Coord lastMove() const;
    std::vector<Coord> currentPath() const;

    // ── Engine analysis data ────────────────────────────────────────────────
    const std::vector<PVLine> &pvLines()      const { return pvLines_; }
    /// True if the engine is currently analyzing.
    bool isAnalyzing() const { return analyzing_; }
    void setAnalyzing(bool a) { analyzing_ = a; }

    /// Attempts to place a move. Returns true if successful, false if invalid or analyzing.
    bool makeMove(Coord pos);
    const EngineStatus        &engineStatus() const { return engineStatus_; }
    void setAnalysisData(std::vector<PVLine> pvs, EngineStatus status);
    std::vector<double> evalHistory() const;

    // ── Database ────────────────────────────────────────────────────────────
    const std::map<Coord, DatabaseEntry>& database() const { return currentDatabase_; }
    void clearDatabase();
    void addDatabaseEntry(const DatabaseEntry& entry);
    void updateDatabase(); // Signal a full refresh done

    // ── Signals ─────────────────────────────────────────────────────────────
    sigc::signal<void()>    signal_board_changed;
    sigc::signal<void()>    signal_engine_analysis;
    sigc::signal<void()>    signal_tree_updated;
    sigc::signal<void(int)> signal_move_selected;
    sigc::signal<void()>    signal_rule_changed;
    sigc::signal<void()>    signal_config_changed;
    sigc::signal<void()>    signal_database_updated;

private:
    BoardState     board_;
    MoveHistory    history_;
    VariationTree  tree_;
    TreeNode      *currentTreeNode_ = nullptr;
    GameRule       rule_             = GameRule::Freestyle;
    EngineConfig   engineConfig_;
    ViewConfig     viewConfig_;

    std::vector<PVLine> pvLines_;
    EngineStatus        engineStatus_;
    bool                analyzing_ = false;

    std::map<Coord, DatabaseEntry> currentDatabase_;
};
