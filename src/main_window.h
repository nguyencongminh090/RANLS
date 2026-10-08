#pragma once

#include "model/game_state.h"
#include "model/board_view_model.h"
#include "engine/engine_process.h"
#include "engine/engine_controller.h"
#include "engine/analysis_coordinator.h"
#include "ui/board_view.h"
#include "ui/analysis_panel.h"
#include "ui/bottom_panel.h"
#include "command/command_dispatcher.h"
#include "ui/command_palette.h"

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

    /// I18N-02: switch the UI language (i18n::setLanguage) and refresh every
    /// long-lived text: the menu model, header/toolbar tooltips and labels,
    /// the rule chip, panel tabs, status labels. Dialogs are built on open so
    /// they pick the new language by themselves. Returns false for an
    /// unsupported code. (The Settings selector that calls this is I18N-03.)
    bool applyLanguage(const std::string &code);

private:
    friend struct RanlsI18n02Probe;  // test seam: menu model / header texts
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
    // PAL-02: the palette test drives onCommandPalette()/paletteItems() and the
    // palette_ member without synthesising a Ctrl+K key event.
    friend struct RanlsPal02Probe;

    void buildMenuBar();
    /// I18N-02: the hamburger menu model, built from the current language.
    Glib::RefPtr<Gio::Menu> buildMenuModel();
    /// I18N-02: (re)apply every translated tooltip / label of the header bar.
    void retranslateToolbar();
    /// I18N-02: language-changed refresh (see applyLanguage()).
    void refreshTranslatedUi();
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
    /// PAL-03: open Settings, optionally on the tab of `focusSettingId` (settings_registry id).
    void openSettings(const std::string &focusSettingId);
    void onAbout();
    /// PAL-02: Ctrl+K command palette.
    void onCommandPalette();
    void buildCommandPalette();
    std::vector<PaletteItem> paletteItems();
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
    /// ARCH-02: thin forwarders to AnalysisCoordinator (decision logic, idle
    /// coalescing and the ENG-02 revert live there; see engine/analysis_coordinator.h).
    /// Kept as members because the friend probes and signal wiring call them.
    void maybeStartAutoMove() { coordinator_.maybeStartAutoMove(); }
    void revertEnginePlaysToOff() { coordinator_.revertEnginePlaysToOff(); }

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

    // ── PROTO-06: live search-overlay View-menu toggles ────────────────────
    /// Toggle handlers for the two View-menu checkboxes. Push the new state
    /// into ViewConfig, persist all four config blocks (STATE-02), sync the
    /// menu, refresh the board.
    void onToggleSearchOverlay(bool active);
    void onToggleSearchWinrate(bool active);
    /// Mirror ViewConfig::showSearchOverlay / showSearchWinrate onto the two
    /// menu checkbox actions. State-only (no re-entrant persist).
    void syncSearchOverlayMenu();
    /// ARCH-02/ANLZ-07: forwarder; `force` semantics documented on
    /// AnalysisCoordinator::scheduleAnalyzeModeRestart.
    void scheduleAnalyzeModeRestart(bool force = false) { coordinator_.scheduleAnalyzeModeRestart(force); }

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
    /// ARCH-02: Analyze-Mode restart / auto-move / ENG-02 revert logic (GTK-free).
    /// Declared after gameState_/controller_ so it is destroyed first.
    AnalysisCoordinator  coordinator_;
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
    // ARCH-02: read-only views of the coordinator's idle-coalescing latches
    // (the ARCH-04 friend probes read these names).
    const bool &autoMoveScheduled_ = coordinator_.autoMoveScheduled();
    const bool &analyzeModeScheduled_ = coordinator_.analyzeModeScheduled();
    const bool &analyzeModeForce_ = coordinator_.analyzeModeForce();

    // ANLZ-01: the menu-bar "Analyze Mode" checkable (bool) action, kept in
    // sync with gameState_.viewConfig().analyzeMode in both directions.
    Glib::RefPtr<Gio::SimpleAction> analyzeModeAction_;

    // PROTO-06: the two View-menu checkable actions for the live search
    // overlay, kept in sync with gameState_.viewConfig() in both directions.
    Glib::RefPtr<Gio::SimpleAction> searchOverlayAction_;
    Glib::RefPtr<Gio::SimpleAction> searchWinrateAction_;

    // ── Layout ──────────────────────────────────────────────────────────────
    Gtk::HeaderBar     headerBar_;
    Gtk::MenuButton    menuButton_;   // UI-21: hamburger menu replaces the menu-bar row
    /// UI-03: persistent rule indicator, always visible in the header bar
    /// (not just inside the Game > Rule menu) -- kept in sync with
    /// gameState_.rule() via updateRuleLabel(), called at startup and on
    /// every gameState_.signal_rule_changed emission.
    Gtk::Label         ruleLabel_;
    Gtk::Paned         mainHPaned_;
    Gtk::Paned         mainVPaned_;
    Gtk::Box           rootBox_{Gtk::Orientation::VERTICAL};
    std::unique_ptr<CommandPalette> palette_;  ///< PAL-02, anchored to rootBox_

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
    Gtk::Button       *btnSave_  = nullptr;
    Gtk::Button       *btnStart_ = nullptr;
    Gtk::Button       *btnStop_  = nullptr;
    Gtk::Label        *lblStart_ = nullptr;  // I18N-02: re-texted on language change
    Gtk::Label        *lblStop_  = nullptr;
    unsigned           languageListener_ = 0;  // I18N-02: i18n::ListenerId
};
