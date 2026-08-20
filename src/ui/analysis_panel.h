#pragma once

#include "engine_status.h"
#include "pv_view.h"
#include "tree_explorer.h"
#include "tree_node_view.h"
#include "win_graph_view.h"
#include "model/game_state.h"

#include <gtkmm.h>

/// Right-column container stacking: EngineStatus | WinGraph | PV View | TreeNodeView | TreeExplorer.
class AnalysisPanel : public Gtk::Box {
public:
    explicit AnalysisPanel(GameState &gameState);

    /// Access sub-components for signal wiring.
    EngineStatusView &engineStatus() { return engineStatus_; }
    WinGraphView     &winGraph()     { return winGraph_; }
    PVView           &pvView()       { return pvView_; }
    TreeExplorer     &tree()         { return treeExplorer_; }
    TreeNodeView     &treeNodeView() { return treeNodeView_; }

private:
    void connectSignals();

    GameState &gameState_;

    EngineStatusView engineStatus_;
    WinGraphView     winGraph_;
    PVView           pvView_;
    TreeNodeView     treeNodeView_;
    TreeExplorer     treeExplorer_;
    Gtk::Paned       graphTreePaned_;   // WinGraph+PV | TreeViews split
    Gtk::Notebook    treeNotebook_;     // Tab: Visual | Table
};
