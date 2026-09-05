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

/// NAME-01: the application's display name — shown as the window title.
/// Single source for the WM-visible identity string.
inline constexpr const char *kAppDisplayName = "RANLS";

/// Main application window.
/// Assembles the 2-column layout: Board | Analysis+Tree, with bottom panel.
class MainWindow : public Gtk::ApplicationWindow {
public:
    MainWindow();
    ~MainWindow() override;

private:
    // ANLZ-05: the widget-level regression test drives the auto-move /
    // analyze-restart idle callbacks against real engine + controller state,
    // which requires reaching the private gameState_/engine_/controller_
    // members and the scheduler methods. Test-only seam — no production API.
    friend struct RanlsAnlz05Probe;
    // ANLZ-07: same rationale as RanlsAnlz05Probe — the restart-convergence
    // regression test needs to drive scheduleAnalyzeModeRestart()'s `force`
    // parameter and inspect controller_ directly.
    friend struct RanlsAnlz07Probe;
    // ENG-03: the close-request regression test needs to drive
    // requestGracefulClose()/the signal_close_request handler and observe
    // closeInFlight_ + controller_ state without a live WM to click the X.
    friend struct RanlsEng03Probe;

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
    // ENG-03: shared body of onQuit() and the signal_close_request handler —
    // both must route through the same graceful controller_.stopEngine(...)
    // shutdown rather than closing (or exiting) immediately. Idempotent aside
    // from closeInFlight_ bookkeeping; the caller decides whether to veto the
    // current close attempt (close-request handler) or not (menu Quit, which
    // isn't itself a close attempt GTK is waiting on an answer to).
    void requestGracefulClose();
    void onSetRule(GameRule rule);
    void onBoardSize();

    // STATE-04: persist the current rule (global preference) + board size
    // (new-game default) to the settings file. Called from onSetRule() and the
    // Board Size dialog's Apply handler. Passes all four config blocks so a
    // save never wipes engine/view/match state (STATE-02 hazard).
    void persistGameSetup();
    void onSettings();
    void onAbout();
    void onStartAnalysis();
    void onStopAnalysis();

    // UI-06: "Engine plays <side>" auto-move.
    /// Menu-activate handler: pushes the choice into MatchConfig (via
    /// GameState::setMatchConfig) and persists all settings, then re-checks
    /// whether it is now the engine's turn to move.
    void onSetEnginePlays(EnginePlaysSide side);
    /// Push gameState_.matchConfig() back into the "engine-plays" radio action
    /// so the menu reflects persisted / externally-changed state. Both
    /// directions must stay in sync (see docs/instruction/UI-06...).
    void syncEnginePlaysMenu();
    /// If MatchConfig says the engine plays the side to move, and the engine
    /// process is running and Idle, ask it for a move. Scheduled on an idle
    /// callback so it runs once after a batch of position changes (e.g. a game
    /// load) settles, never re-entrantly. Inert while enginePlays == Off.
    void maybeStartAutoMove();
    /// ENG-02: manual intervention cancels auto-play. If enginePlays != Off,
    /// set it back to Off in the in-memory MatchConfig and sync the menu radio.
    /// Transient session action — deliberately does NOT call
    /// SettingsStorage::save (unlike onSetEnginePlays), so the persisted
    /// user-chosen side is restored on next launch. No status message / toast.
    /// No-op when enginePlays is already Off.
    void revertEnginePlaysToOff();

    // ── ANLZ-01: Analyze Mode (continuous background analysis) ──────────────
    /// Toggle handler (menu checkbox or analysis-panel button): push the new
    /// state into ViewConfig, persist all four config blocks (STATE-02), sync
    /// both toggle surfaces, and either kick an analysis restart (on) or stop
    /// the current search (off). Orthogonal to "Engine plays" / ENG-02 — never
    /// touches MatchConfig.
    void onToggleAnalyzeMode(bool active);
    /// Mirror gameState_.viewConfig().analyzeMode onto the menu checkbox action
    /// and the analysis-panel toggle button. State-only (no re-entrant persist),
    /// same shape as syncEnginePlaysMenu().
    void syncAnalyzeModeMenu();
    /// If Analyze Mode is on, coalesce a burst of position changes into a single
    /// deferred check that, when the engine is running + Idle, does
    /// stopAnalysis(); analyze() on the new current position. Copy of
    /// maybeStartAutoMove()'s idle-coalescing structure.
    ///
    /// ANLZ-07: `force` bypasses the "skip if the last analysis-intent search
    /// already converged to the same result on this position" check
    /// (EngineController::analysisConverged()) — pass true for a genuine
    /// reason to restart regardless of any cached result: a real position
    /// change (signal_board_changed) or the user explicitly toggling Analyze
    /// Mode off/on. The default (false) is for the routine
    /// engine-just-went-Idle re-arm, where a converged, unchanged result
    /// means re-running the search would just repeat it forever (the
    /// busy-loop this task fixes). Multiple coalesced calls before the
    /// deferred check runs latch `force` if ANY of them requested it — see
    /// analyzeModeForce_.
    void scheduleAnalyzeModeRestart(bool force = false);

    void onUndoAll();
    void onUndo();
    void onRedo();
    void onRedoAll();

    /// UI-03: refreshes ruleLabel_'s text from gameState_.rule(). Called once
    /// at startup and on every gameState_.signal_rule_changed emission, so
    /// the active rule stays visible in the header bar (persistent, not
    /// hidden inside the Game > Rule menu) regardless of how it was changed.
    void updateRuleLabel();

    // ENG-03: set on the first signal_close_request (WM "X" / titlebar close)
    // to veto that close and kick off requestGracefulClose(); the completion
    // callback's close() re-triggers signal_close_request, and this flag
    // makes that second pass return false so GTK actually closes. Not used
    // by the menu-Quit path (onQuit() isn't answering a pending close
    // request, so there's nothing to veto/re-issue there) — stopEngine()'s
    // own Stopping-state completion chaining (see EngineController::
    // stopEngine) already covers "Quit already in flight, then X clicked".
    bool closeInFlight_ = false;

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

    // UI-06: the menu-bar "Engine plays" radio action, kept in sync with
    // gameState_.matchConfig() in both directions. `autoMoveScheduled_`
    // coalesces the idle-callback that fires the engine's auto-move so a
    // burst of signal_board_changed emissions (a game load replays moves one
    // by one) triggers at most one move request, for the final position.
    Glib::RefPtr<Gio::SimpleAction> enginePlaysAction_;
    bool autoMoveScheduled_ = false;

    // ANLZ-01: the menu-bar "Analyze Mode" checkable (bool) action, kept in
    // sync with gameState_.viewConfig().analyzeMode in both directions.
    // `analyzeModeScheduled_` coalesces the idle-callback that restarts the
    // engine's analysis so a burst of signal_board_changed emissions (a game
    // load, undoAll/redoAll) triggers at most one restart, for the final
    // position — same rationale as autoMoveScheduled_ above.
    Glib::RefPtr<Gio::SimpleAction> analyzeModeAction_;
    bool analyzeModeScheduled_ = false;

    // ANLZ-07: latches whether ANY scheduleAnalyzeModeRestart() call
    // coalesced into the pending idle callback requested `force` — a
    // non-forced call arriving after a forced one (or vice versa) must not
    // downgrade the eventual restart, so this is OR'd in, never overwritten,
    // until the deferred callback consumes and resets it.
    bool analyzeModeForce_ = false;

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
