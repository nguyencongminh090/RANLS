#pragma once

#include "model/config.h"
#include "model/game_state.h"

#include <functional>
#include <memory>
#include <sigc++/sigc++.h>

class EngineController;

/// ARCH-02: Analyze-Mode restart + "Engine plays <side>" auto-move decisions,
/// extracted from MainWindow so they are unit-testable without a display.
///
/// Toolkit-free by construction (layer: engine/ -> model/, see rule 8). The only
/// platform seam is the injected `postIdle` scheduler: MainWindow supplies a
/// main-loop idle adapter, tests supply a manual queue they drain explicitly.
///
/// Owns: the idle-coalescing latches (autoMoveScheduled_, analyzeModeScheduled_,
/// analyzeModeForce_) and the ENG-02 manual-override revert. Does NOT own menu
/// sync (signal_engine_plays_reverted lets the UI re-sync) or persistence
/// (the revert never saves; MainWindow persists the user-driven changes).
///
/// Lifetime: must not outlive the GameState / EngineController it references.
/// A callback already posted to `postIdle` that runs after the coordinator is
/// destroyed is a no-op (shared liveness token, see alive_).
class AnalysisCoordinator {
public:
    using PostIdle = std::function<void(std::function<void()>)>;

    AnalysisCoordinator(GameState &gameState, EngineController &controller, PostIdle postIdle);
    ~AnalysisCoordinator();
    AnalysisCoordinator(const AnalysisCoordinator &) = delete;
    AnalysisCoordinator &operator=(const AnalysisCoordinator &) = delete;

    /// If MatchConfig says the engine plays the side to move, and the engine
    /// is running and Idle, ask it for a move. Deferred to one coalesced idle
    /// callback so a batch of position changes (game load) settles first.
    /// Inert while enginePlays == Off.
    void maybeStartAutoMove();

    /// If Analyze Mode is on, coalesce a burst of position changes into a single
    /// deferred check that (engine running + Idle) does stopAnalysis();
    /// analyze(). ANLZ-07: `force` bypasses the analysisConverged() skip and is
    /// latched across coalesced calls (never downgraded).
    void scheduleAnalyzeModeRestart(bool force = false);

    /// Sets MatchConfig::enginePlays. Returns true if it changed (caller
    /// persists; this class never saves). Does not trigger an auto-move.
    bool setEnginePlays(EnginePlaysSide side);

    /// Analyze Mode was toggled (ViewConfig already updated + persisted by the
    /// caller). On: forced restart. Off: stop the search, leave the process
    /// running and enginePlays untouched (orthogonal to ENG-02).
    void onAnalyzeModeToggled(bool active);

    /// ENG-02: manual intervention cancels auto-play. enginePlays -> Off in
    /// memory only (NO persistence), then signal_engine_plays_reverted. No-op
    /// if already Off.
    void revertEnginePlaysToOff();
    /// ENG-02: revert only if it is currently the engine's assigned turn.
    void revertEnginePlaysIfEnginesTurn();
    /// ENG-02 manual Stop: revert, then stopAnalysis().
    void stopAnalysis();

    /// Emitted after a revert actually changed enginePlays (MainWindow re-syncs
    /// the menu radio).
    sigc::signal<void()> signal_engine_plays_reverted;

    // Latch views (tests / characterization).
    const bool &autoMoveScheduled() const { return autoMoveScheduled_; }
    const bool &analyzeModeScheduled() const { return analyzeModeScheduled_; }
    const bool &analyzeModeForce() const { return analyzeModeForce_; }

private:
    void runAutoMove();
    void runAnalyzeRestart();

    GameState        &gameState_;
    EngineController &controller_;
    PostIdle          postIdle_;
    /// Posted callbacks hold a weak_ptr to this token; destruction expires it.
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    bool autoMoveScheduled_    = false;
    bool analyzeModeScheduled_ = false;
    /// ANLZ-07: OR'd across coalesced scheduleAnalyzeModeRestart() calls until
    /// the deferred callback consumes and resets it.
    bool analyzeModeForce_     = false;
};
