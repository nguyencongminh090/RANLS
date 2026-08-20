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
#include <memory>

/// Main application window.
/// Assembles the 2-column layout: Board | Analysis+Tree, with bottom panel.
class MainWindow : public Gtk::ApplicationWindow {
public:
    MainWindow();
    ~MainWindow() override = default;

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

    // ── Data ────────────────────────────────────────────────────────────────
    GameState          gameState_;
    BoardViewModel     boardViewModel_;
    EngineProcess        engine_;
    EngineController     controller_;
    std::unique_ptr<CommandDispatcher> commandDispatcher_;

    // ── Layout ──────────────────────────────────────────────────────────────
    Gtk::HeaderBar     headerBar_;
    Gtk::PopoverMenuBar menuBar_;
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
