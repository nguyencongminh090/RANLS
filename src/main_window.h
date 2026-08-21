#pragma once

#include "model/game_state.h"
#include "model/board_view_model.h"
#include "engine/engine_process.h"
#include "engine/engine_controller.h"
#include "ui/board_view.h"
#include "ui/analysis_panel.h"
#include "ui/bottom_panel.h"
#include "command/command_dispatcher.h"

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <memory>

/// Main application window.
/// Assembles the 2-column layout: Board | Analysis+Tree, with bottom panel.
class MainWindow : public Gtk::ApplicationWindow {
public:
    MainWindow();
    ~MainWindow() override;

private:
    void buildMenuBar();
    void buildToolbar();
    void buildLayout();
    void connectSignals();

    // ── Menu actions ────────────────────────────────────────────────────────
    void onNewGame();
    void onLoadGame();
    void onSaveGame();
    void onQuit();
    void onSetRule(GameRule rule);
    void onBoardSize();
    void onSettings();
    void onAbout();
    void onStartAnalysis();
    void onStopAnalysis();

    void onUndoAll();
    void onUndo();
    void onRedo();
    void onRedoAll();

    /// UI-03: refreshes ruleLabel_'s text from gameState_.rule(). Called once
    /// at startup and on every gameState_.signal_rule_changed emission, so
    /// the active rule stays visible in the header bar (persistent, not
    /// hidden inside the Game > Rule menu) regardless of how it was changed.
    void updateRuleLabel();

    // ── Data ────────────────────────────────────────────────────────────────
    GameState          gameState_;
    BoardViewModel     boardViewModel_;
    EngineProcess        engine_;
    EngineController     controller_;
    std::unique_ptr<CommandDispatcher> commandDispatcher_;

    // RT-01: periodic tick that coalesces gameState_.signal_engine_analysis
    // emissions to a bounded rate instead of one per parsed engine line. See
    // GameState::tickAnalysis()/flush() (src/model/game_state.h) — GameState
    // itself stays free of glibmm so it remains buildable in tests/CMakeLists.txt
    // (no GTK main loop there); the live Glib::signal_timeout lives here instead.
    sigc::connection analysisTickConn_;

    // ── Layout ──────────────────────────────────────────────────────────────
    Gtk::HeaderBar     headerBar_;
    Gtk::PopoverMenuBar menuBar_;
    /// UI-03: persistent rule indicator, always visible in the header bar
    /// (not just inside the Game > Rule menu) -- kept in sync with
    /// gameState_.rule() via updateRuleLabel(), called at startup and on
    /// every gameState_.signal_rule_changed emission.
    Gtk::Label         ruleLabel_;
    Gtk::Paned         mainHPaned_;
    Gtk::Paned         mainVPaned_;
    Gtk::Box           rootBox_{Gtk::Orientation::VERTICAL};

    // ── UI components ───────────────────────────────────────────────────────
    BoardView          boardView_;
    AnalysisPanel      analysisPanel_;
    BottomPanel        bottomPanel_;

    // Navigation buttons (to disable during analysis)
    Gtk::Button       *btnFirst_ = nullptr;
    Gtk::Button       *btnUndo_  = nullptr;
    Gtk::Button       *btnRedo_  = nullptr;
    Gtk::Button       *btnLast_  = nullptr;
    Gtk::Button       *btnNew_   = nullptr;
    Gtk::Button       *btnLoad_  = nullptr;
};
