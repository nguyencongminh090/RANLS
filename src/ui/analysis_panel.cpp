#include "analysis_panel.h"

#include "ui/win_graph_series.h"

#include <algorithm>
#include <cmath>
#include <limits>

// UX-06: the perspective/mode conversion lives in ui/win_graph_series.h
// (header-only, no gtkmm) so it can be unit-tested. `WinGraphData` is kept as
// a thin alias for the existing call sites below.
using WinGraphData = WinGraphSeries;

static WinGraphData toDisplayWinrate(const std::vector<double> &raw, WinGraphMode mode,
                                     EnginePlaysSide enginePlays)
{
    return buildWinGraphSeries(raw, mode, enginePlays);
}

AnalysisPanel::AnalysisPanel(GameState &gameState)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
    , gameState_(gameState)
{
    add_css_class("panel");
    set_hexpand(true);
    set_vexpand(true);

    // ── Engine Status (always visible, fixed height) ────────────────────────
    append(engineStatus_);

    // ── Crash announcement banner (ENG-01) ──────────────────────────────────
    // No libadwaita linked in this build, so a crash is announced with an
    // inline dismissible banner instead of an Adw::Toast.
    auto *bannerBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    bannerBox->add_css_class("crash-banner");
    crashBannerLabel_.set_hexpand(true);
    crashBannerLabel_.set_xalign(0.0f);
    crashBannerLabel_.set_wrap(true);
    auto *dismissBtn = Gtk::make_managed<Gtk::Button>("Dismiss");
    dismissBtn->signal_clicked().connect([this]() { hideEngineCrashBanner(); });
    bannerBox->append(crashBannerLabel_);
    bannerBox->append(*dismissBtn);

    crashBannerRevealer_.set_child(*bannerBox);
    crashBannerRevealer_.set_reveal_child(false);
    crashBannerRevealer_.set_transition_type(Gtk::RevealerTransitionType::SLIDE_DOWN);
    append(crashBannerRevealer_);

    // Engine crashed → active announcement (not just a label flip).
    engineStatus_.signal_crashed.connect([this]() {
        showEngineCrashBanner(gameState_.engineConfig().enginePath);
    });

    // ── Upper section: WinGraph + PV ────────────────────────────────────────
    auto *upperBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    upperBox->append(winGraph_);
    upperBox->append(pvView_);

    // ── Lower section: Tree views in a Notebook (Visual | Table) ────────────
    // The two tabs show different datasets (UI-02), so the labels say which:
    // Visual is the whole variation tree (all branches); Table is only the
    // current line (moves from game start to the current position).
    auto makeTabLabel = [](const std::string &text, const std::string &tooltip) {
        auto *label = Gtk::make_managed<Gtk::Label>(text);
        label->set_tooltip_text(tooltip);
        return label;
    };
    treeNotebook_.append_page(treeNodeView_,
        *makeTabLabel("Visual (All Branches)",
            "Shows the full variation tree, including all explored branches."));
    treeNotebook_.append_page(treeExplorer_,
        *makeTabLabel("Table (Current Line)",
            "Shows only the current line: moves from the game start to the current position."));
    treeNotebook_.add_css_class("bottom-panel");

    // ── Vertical Paned: upper | lower ───────────────────────────────────────
    graphTreePaned_.set_orientation(Gtk::Orientation::VERTICAL);
    graphTreePaned_.set_start_child(*upperBox);
    graphTreePaned_.set_end_child(treeNotebook_);
    graphTreePaned_.set_resize_start_child(true);
    graphTreePaned_.set_shrink_start_child(false);
    graphTreePaned_.set_resize_end_child(true);
    graphTreePaned_.set_shrink_end_child(false);
    graphTreePaned_.set_vexpand(true);

    append(graphTreePaned_);

    connectSignals();
}

void AnalysisPanel::connectSignals()
{
    // Engine analysis → update all sub-panels.
    gameState_.signal_engine_analysis.connect([this]() {
        engineStatus_.update(gameState_.engineStatus(), gameState_.pvLines(), gameState_.boardSize());
        pvView_.update(gameState_.pvLines(), gameState_.boardSize());
        auto data = toDisplayWinrate(gameState_.evalHistory(), gameState_.viewConfig().winGraphMode,
                                     gameState_.matchConfig().enginePlays);
        winGraph_.setData(data.black, data.white, gameState_.history().currentIndex(),
                          gameState_.viewConfig().winGraphMode);
    });

    // Tree updates → refresh both tree views.
    gameState_.signal_tree_updated.connect([this]() {
        // Table shows history of moves up to the current position.
        treeExplorer_.update(gameState_.history(), gameState_.tree(), gameState_.boardSize());
        // Visual tab shows entire tree.
        treeNodeView_.update(gameState_.tree().root(), gameState_.currentPath());
    });

    // Board changes → update the WinGraph highlight and tree path.
    gameState_.signal_board_changed.connect([this]() {
        // UI-07: the PV list and engine-status readout belong to one specific
        // board position. Refresh them straight from gameState_ here rather
        // than relying on GameState::resetAnalysisState() choosing to emit
        // signal_engine_analysis — it early-returns (no emit) when the model
        // side is already empty (RT-01's `alreadyEmpty` guard), which left a
        // stale row on screen whenever the view outran the model (e.g. a
        // trailing engine line painted a PV for the old position after the
        // model had already been cleared). Every position-changing GameState
        // op runs resetAnalysisState() while !analyzing_, so pvLines() is
        // guaranteed empty by the time this fires; this call then guarantees
        // the panel drops to zero rows. In-place row reuse in PVView::update
        // keeps RT-03's hover preservation intact.
        pvView_.update(gameState_.pvLines(), gameState_.boardSize());
        engineStatus_.update(gameState_.engineStatus(), gameState_.pvLines(),
                             gameState_.boardSize());

        auto data = toDisplayWinrate(gameState_.evalHistory(), gameState_.viewConfig().winGraphMode,
                                     gameState_.matchConfig().enginePlays);
        winGraph_.setData(data.black, data.white, gameState_.history().currentIndex(),
                          gameState_.viewConfig().winGraphMode);
        // Update Table with the new position's history
        treeExplorer_.update(gameState_.history(), gameState_.tree(), gameState_.boardSize());
        treeNodeView_.update(gameState_.tree().root(), gameState_.currentPath());
    });

    gameState_.signal_config_changed.connect([this]() {
        auto data = toDisplayWinrate(gameState_.evalHistory(), gameState_.viewConfig().winGraphMode,
                                     gameState_.matchConfig().enginePlays);
        winGraph_.setData(data.black, data.white, gameState_.history().currentIndex(),
                          gameState_.viewConfig().winGraphMode);
    });

    // WinGraph click → jump to move.
    winGraph_.signal_move_jumped.connect([this](int moveIndex) {
        gameState_.gotoMove(moveIndex);
    });

    // Visual tree node click -> jump to that branch position.
    treeNodeView_.signal_node_clicked.connect([this](std::vector<Coord> path) {
        gameState_.gotoPath(path);
    });

    // Table row selection -> jump to that position (UI-02: parity with the
    // Visual tab's click-to-jump).
    treeExplorer_.signal_node_selected.connect([this](std::vector<Coord> path) {
        gameState_.gotoPath(path);
    });
}

void AnalysisPanel::showEngineCrashBanner(const std::string &enginePath)
{
    std::string path = enginePath.empty() ? "(no engine path configured)" : enginePath;
    crashBannerLabel_.set_text("Engine crashed: " + path);
    crashBannerRevealer_.set_reveal_child(true);
}

void AnalysisPanel::hideEngineCrashBanner()
{
    crashBannerRevealer_.set_reveal_child(false);
}
