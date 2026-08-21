#include "analysis_panel.h"

#include <algorithm>

struct WinGraphData {
    std::vector<double> black;
    std::vector<double> white;
};

static WinGraphData toDisplayWinrate(const std::vector<double> &raw, WinGraphMode mode)
{
    WinGraphData out;
    out.black.reserve(raw.size());
    out.white.reserve(raw.size());

    for (size_t i = 0; i < raw.size(); ++i) {
        double sideToMove = std::clamp(raw[i], 0.0, 1.0);
        bool blackToMove = (i % 2 == 0);
        if (mode == WinGraphMode::SingleSide) {
            double black = blackToMove ? sideToMove : (1.0 - sideToMove);
            out.black.push_back(black);
            out.white.push_back(1.0 - black);
        } else {
            // BothSide: one black-scale line only.
            out.black.push_back(blackToMove ? sideToMove : (1.0 - sideToMove));
            out.white.push_back(0.0);
        }
    }
    return out;
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
    treeNotebook_.append_page(treeNodeView_, "Visual");
    treeNotebook_.append_page(treeExplorer_, "Table");
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
        auto data = toDisplayWinrate(gameState_.evalHistory(), gameState_.viewConfig().winGraphMode);
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
        auto data = toDisplayWinrate(gameState_.evalHistory(), gameState_.viewConfig().winGraphMode);
        winGraph_.setData(data.black, data.white, gameState_.history().currentIndex(),
                          gameState_.viewConfig().winGraphMode);
        // Update Table with the new position's history
        treeExplorer_.update(gameState_.history(), gameState_.tree(), gameState_.boardSize());
        treeNodeView_.update(gameState_.tree().root(), gameState_.currentPath());
    });

    gameState_.signal_config_changed.connect([this]() {
        auto data = toDisplayWinrate(gameState_.evalHistory(), gameState_.viewConfig().winGraphMode);
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
