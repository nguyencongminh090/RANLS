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

    /// Stores the latest engine analysis data and marks it dirty. Does NOT
    /// emit signal_engine_analysis synchronously (see RT-01) — that would
    /// mean one full UI rebuild per parsed engine line (up to 8x with
    /// multiPV=8). Emission is coalesced onto tickAnalysis()/flush() instead.
    void setAnalysisData(std::vector<PVLine> pvs, EngineStatus status);

    /// Cached: recomputed only when the tree/board actually changes, not on
    /// every analysis update (evalHistory() used to walk the whole variation
    /// tree on every call from 3 separate AnalysisPanel handlers).
    std::vector<double> evalHistory() const;

    // ── Analysis update throttling (RT-01) ──────────────────────────────────
    // GameState intentionally does NOT own a live Glib::signal_timeout itself:
    // src/model + src/engine/gomocup_protocol.cpp are built standalone for
    // tests/CMakeLists.txt without linking glibmm/gtkmm (see that file's
    // header comment) so the model layer stays testable without a GTK main
    // loop. The mechanism (dirty flag + coalesced storage) lives here; the
    // actual periodic timer that drives tickAnalysis() lives in the UI layer
    // (MainWindow), the documented fallback location for the throttle.

    /// Called periodically (e.g. by a UI timer, target ~10-15 Hz) to emit a
    /// coalesced signal_engine_analysis if new data arrived since the last
    /// tick/flush. Returns true if an emission happened.
    bool tickAnalysis();

    /// Immediately emits any pending coalesced analysis update, bypassing the
    /// timer. Callers must invoke this on search completion / analysis-stopped
    /// so the final update of a search is never delayed or dropped.
    void flush();

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

    /// True when pvLines_/engineStatus_ have changed since the last
    /// signal_engine_analysis emission. Consumed by tickAnalysis()/flush().
    bool analysisDirty_ = false;

    /// True when the current tree node's eval/depth/nodes changed since the
    /// last signal_tree_updated emission (RT-04: reuses the RT-01 dirty-flag/
    /// tick mechanism instead of emitting signal_tree_updated synchronously
    /// from setAnalysisData, which previously drove a full tree-view rebuild
    /// on essentially every parsed engine line). Always set alongside
    /// analysisDirty_ and consumed together by tickAnalysis()/flush() — never
    /// set true while analysisDirty_ is false, so no separate guard is needed
    /// when clearing it. Structural tree changes (new move, gotoPath, etc.)
    /// still emit signal_tree_updated synchronously elsewhere; this flag only
    /// covers the per-tick eval/depth/nodes refresh of the current node.
    bool treeDirty_ = false;

    /// evalHistory() cache — invalidated whenever the tree/board actually
    /// changes (position ops, or a tree-node eval update from setAnalysisData),
    /// not on every analysis tick.
    mutable std::vector<double> evalHistoryCache_;
    mutable bool                evalHistoryDirty_ = true;
    void invalidateEvalHistoryCache() { evalHistoryDirty_ = true; }

    std::map<Coord, DatabaseEntry> currentDatabase_;

    /// Clears pvLines_/engineStatus_ and notifies the analysis UI, but only if
    /// there is actually something to clear (idempotent — safe to call from
    /// every position-changing operation, including undoAll/redoAll's loops,
    /// without flooding signal_engine_analysis on already-empty state).
    /// Must only be called on an actual position change, never while
    /// analyzing_ is true for the *same* position (callers already guard
    /// entry with `if (analyzing_) return` before mutating position state).
    void resetAnalysisState();
};
