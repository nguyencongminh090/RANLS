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
#include <functional>
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

    // UX-05: Gtk::Paned stores its divider as an absolute pixel offset, so
    // shrinking the window clamps it down and growing the window back does
    // not restore it (GTK does not rescale proportionally). Fix: track each
    // paned's divider as a *fraction* of its own allocated extent and
    // reassert the equivalent pixel position on every top-level allocation.
    // size_allocate_vfunc is the GTK4-idiomatic hook for this -- gtkmm4
    // widgets no longer expose a public signal_size_allocate(), but a
    // subclass (MainWindow is-a Gtk::Widget via ApplicationWindow) can still
    // override the virtual to run code after every allocation.
    void size_allocate_vfunc(int width, int height, int baseline) override;

    // Called from mainHPaned_/mainVPaned_'s "notify::position" handlers.
    // Only updates the stored fraction when the paned's own extent (width
    // for the horizontal split, height for the vertical one) is unchanged
    // since the last time we looked -- i.e. when the position change is a
    // genuine user drag (or our own reassertion, which is idempotent), not
    // a GTK-internal clamp caused by the window shrinking. This is what
    // keeps the desired ratio stable across a shrink-then-grow cycle instead
    // of latching onto whatever tiny value the shrink clamped it to.
    void trackPanedFraction(Gtk::Paned &paned, double &fraction, int &lastExtent, bool vertical);

    // Recomputes and reasserts both panes' pixel positions from their
    // stored fractions against their current allocated extents. Called from
    // size_allocate_vfunc after the base-class allocation so the panes'
    // extents already reflect the new window size.
    void reapplyPanedFractions();

    // UX-03: confirm before discarding the current game (board/history/tree)
    // when it's non-empty. Shared by onNewGame() and the board-size Apply
    // handler in onBoardSize(). Runs `onConfirmed` only if the user accepts,
    // or immediately if the board is already empty (no prompt needed).
    void confirmDiscardGame(const Glib::ustring &action, std::function<void()> onConfirmed);

    // IO-01: modal error dialog (OK button), self-deleting on response. Used
    // to surface Load/Save failures visibly instead of a silent no-op.
    void showErrorDialog(const Glib::ustring &primary, const Glib::ustring &detail);

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

    // UX-05: divider position as a fraction of the pane's own extent, plus
    // the extent we last observed it at (see trackPanedFraction()). Initial
    // fractions match the absolute values buildLayout() used to set at the
    // 1280x800 default window size (640/1280, 580/800); they get corrected
    // to the real allocated extent as soon as the window is realized.
    double             hPanedFraction_   = 640.0 / 1280.0;
    double             vPanedFraction_   = 580.0 / 800.0;
    int                hPanedLastExtent_ = 0;
    int                vPanedLastExtent_ = 0;

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
