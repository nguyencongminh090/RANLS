#pragma once

#include "engine_process.h"
#include "i_engine_protocol.h"
#include "model/game_state.h"

#include <functional>
#include <memory>
#include <sigc++/sigc++.h>
#include <string>

/// High-level controller bridging GameState ↔ EngineProcess via IEngineProtocol.
class EngineController {
public:
    /// Explicit engine lifecycle state. Replaces the former started_/analyzing_
    /// bool pair (ENG-01) — reality has more than two states, and the bool pair
    /// could not honestly represent "failed to start", "crashed", "thinking",
    /// or "shutting down".
    enum class EngineState {
        NotStarted, ///< No process; either never started or a clean stop finished.
        Starting,   ///< Spawn requested; not yet confirmed usable.
        Idle,       ///< Process running, not currently analyzing.
        Analyzing,  ///< Process running and mid-search.
        Stopping,   ///< Shutdown requested; waiting for exit or grace-period timeout.
        Crashed,    ///< Process died unexpectedly while it was supposed to be running.
    };

    EngineController(GameState &gameState, EngineProcess &engine);
    ~EngineController();

    /// Start/restart the engine with current config. No-op if already
    /// starting/running. Does not transition to a running state if the
    /// underlying process fails to spawn (see EngineProcess::start()'s
    /// return value) — a visible error is emitted via signal_engine_output
    /// instead.
    void startEngine();

    /// Stop the engine process. Non-blocking: sends a graceful shutdown
    /// request, transitions to Stopping immediately, and completes
    /// asynchronously (force-killing the process if it doesn't exit within
    /// the grace period). `onComplete`, if given, runs exactly once when the
    /// transition back to NotStarted actually happens — including
    /// synchronously, if the engine was already not started.
    void stopEngine(std::function<void()> onComplete = nullptr);

    /// Stop then restart the engine. Asynchronous — restart happens only
    /// after the stop actually completes.
    void reloadEngine();

    /// Send the current board position and request analysis.
    void analyze();

    /// UI-06: send the current board position and ask the engine to produce a
    /// single move for the side to move. No-op unless the engine is Idle. The
    /// engine's reply is delivered through signal_engine_move, exactly like a
    /// move made via analyze() completing — MainWindow plays it on the board.
    void requestEngineMove();

    /// Stop the current analysis.
    void stopAnalysis();

    /// Send all configuration commands (timeout, threads, hash, etc).
    void sendConfig();

    /// Request the engine to refresh database info for current position.
    void queryDatabase();

    /// Send a raw command line to the engine.
    void sendRawCommand(const std::string &command);

    /// Signal for raw engine output (for logging). First string is group ("Message", "Coord", "Output", "Error").
    sigc::signal<void(std::string, std::string)> signal_engine_output;

    /// Signal when engine makes a move (returns coord).
    sigc::signal<void(Coord)> signal_engine_move;

    /// Signal for engine lifecycle state transitions. Emitted only on real
    /// transitions (never spuriously, e.g. stopping an engine that was never
    /// started does not emit).
    sigc::signal<void(EngineState)> signal_state_changed;

    /// Signal when a database entry is received.
    sigc::signal<void(const DatabaseEntry&)> signal_database_entry;

    /// Signal when a database query starts (clearing previous results).
    sigc::signal<void()> signal_database_refresh;
    
    /// Signal when a database refresh is done.
    sigc::signal<void()> signal_database_done;

    EngineState engineState() const { return state_; }

    /// True while a search is in progress. GameState (src/model/game_state.h)
    /// is the single owner of the underlying "analyzing" fact — it needs its
    /// own copy to guard position mutations without depending on the engine
    /// layer — so this reads through to gameState_.isAnalyzing() rather than
    /// keeping a second, independently-updated bool (ENG-01: that second copy
    /// was the "GameState mirrors analyzing_ separately" duplication).
    bool isAnalyzing() const { return gameState_.isAnalyzing(); }

    /// True once a process is confirmed usable (Starting/Idle/Analyzing) and
    /// not yet torn down. False for NotStarted and Crashed.
    bool isStarted() const
    {
        return state_ == EngineState::Starting || state_ == EngineState::Idle
            || state_ == EngineState::Analyzing || state_ == EngineState::Stopping;
    }

private:
    void onEngineLine(const std::string &line);
    void connectProtocolSignals();
    void setState(EngineState next);
    /// True if commands may be sent to the process right now.
    bool isUsable() const;

    /// ANLZ-06: discriminates why the current/most-recent search was
    /// started, so the protocol's signal_move handler can tell a real
    /// requested move apart from an analysis search's completion coordinate.
    /// Both analyze() and requestEngineMove() transition through
    /// EngineState::Analyzing identically, so state_ alone cannot
    /// distinguish them.
    enum class SearchIntent { None, Analysis, Move };
    SearchIntent searchIntent_ = SearchIntent::None;

    GameState     &gameState_;
    EngineProcess &engine_;
    std::unique_ptr<IEngineProtocol> protocol_;

    EngineState state_ = EngineState::NotStarted;

    // ── Async-shutdown lifetime safety ──────────────────────────────────────
    // stopEngine()'s completion runs on a callback that GLib may invoke long
    // after this call returns (Gio::Subprocess::wait_async / a grace-period
    // timeout — see EngineProcess::stopAsync()). If `this` is destroyed
    // before that fires (e.g. the engine was mid-stop when the window closed
    // through some path other than the wait-for-completion one in
    // MainWindow::onQuit()), invoking the callback would use a dangling
    // pointer. `lifetimeGuard_` is a presence marker: the callback holds only
    // a weak_ptr and checks `.expired()` before touching `this`; destroying
    // EngineController destroys the last shared_ptr to it, so the weak_ptr
    // reliably reports "gone" with no dangling access.
    std::shared_ptr<void> lifetimeGuard_ = std::make_shared<int>(0);

    // If stopEngine() is called again while already Stopping, its onComplete
    // is chained here instead of being fired early (firing early could let a
    // caller like onQuit() close/destroy `this` before the original async
    // stop has actually settled).
    std::function<void()> pendingStopComplete_;
};
