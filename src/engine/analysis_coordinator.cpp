#include "analysis_coordinator.h"

#include "engine_controller.h"

AnalysisCoordinator::AnalysisCoordinator(GameState &gameState, EngineController &controller,
                                         PostIdle postIdle)
    : gameState_(gameState), controller_(controller), postIdle_(std::move(postIdle))
{}

AnalysisCoordinator::~AnalysisCoordinator() = default;  // alive_ expires pending callbacks

void AnalysisCoordinator::maybeStartAutoMove()
{
    if (autoMoveScheduled_) return;
    if (gameState_.matchConfig().enginePlays == EnginePlaysSide::Off) return;

    // Defer to an idle callback: signal_board_changed can fire many times in
    // one synchronous batch (a game load replays every move), and GameState
    // rejects a makeMove() while analyzing_ is set — so requesting a move
    // mid-batch would both fire on the wrong position and break the replay.
    // The flag coalesces the burst into a single deferred check.
    autoMoveScheduled_ = true;
    std::weak_ptr<bool> alive = alive_;
    postIdle_([this, alive]() {
        if (alive.expired()) return;  // coordinator destroyed meanwhile
        runAutoMove();
    });
}

void AnalysisCoordinator::runAutoMove()
{
    autoMoveScheduled_ = false;

    const auto plays = gameState_.matchConfig().enginePlays;
    if (plays == EnginePlaysSide::Off) return;
    // ANLZ-05: while Analyze Mode is on the engine never auto-plays — not even
    // on its own assigned turn. It only ever analyses the current position
    // (scheduleAnalyzeModeRestart() covers the engine's-turn position too).
    if (gameState_.viewConfig().analyzeMode) return;
    if (!controller_.isRunning()) return;
    if (controller_.engineState() != EngineController::EngineState::Idle) return;

    const Stone toMove = gameState_.board().sideToMove();
    if (!isEnginesTurn(plays, toMove)) return;  // ENG-02: shared predicate

    // After the engine's move lands, side-to-move flips to the other
    // colour, so this check fails next time — no infinite loop.
    controller_.requestEngineMove();
}

void AnalysisCoordinator::scheduleAnalyzeModeRestart(bool force)
{
    if (!gameState_.viewConfig().analyzeMode) return;

    // ANLZ-07: latch `force` across coalesced calls. A later non-forced call
    // must never downgrade an earlier forced one still waiting on the idle
    // callback.
    if (force) analyzeModeForce_ = true;

    if (analyzeModeScheduled_) return;

    // Defer to a single idle callback — same rationale as maybeStartAutoMove():
    // the engine must analyse only the final settled position, once.
    analyzeModeScheduled_ = true;
    std::weak_ptr<bool> alive = alive_;
    postIdle_([this, alive]() {
        if (alive.expired()) return;  // coordinator destroyed meanwhile
        runAnalyzeRestart();
    });
}

void AnalysisCoordinator::runAnalyzeRestart()
{
    analyzeModeScheduled_ = false;
    const bool doForce = analyzeModeForce_;
    analyzeModeForce_ = false;

    if (!gameState_.viewConfig().analyzeMode) return;
    if (!controller_.isRunning()) return;
    if (controller_.engineState() != EngineController::EngineState::Idle) return;

    // ANLZ-05: the engine's-turn position is analysed too (Analyze Mode is a
    // pure study mode; no isEnginesTurn bail).

    // ANLZ-07: skip re-arming when the search that just finished already
    // converged to the same result as the one before it — re-running would
    // repeat the same request/response forever. `doForce` (a genuine position
    // change, or the user toggling Analyze Mode) always bypasses this.
    if (!doForce && controller_.analysisConverged()) return;

    // Restart order matters: analyze() early-returns unless state == Idle,
    // so stopAnalysis() (Idle + RT-01 flush) must precede it.
    controller_.stopAnalysis();
    controller_.analyze();
}

bool AnalysisCoordinator::setEnginePlays(EnginePlaysSide side)
{
    MatchConfig mc = gameState_.matchConfig();
    if (mc.enginePlays == side) return false;
    mc.enginePlays = side;
    gameState_.setMatchConfig(mc);
    return true;
}

void AnalysisCoordinator::onAnalyzeModeToggled(bool active)
{
    if (active) {
        // The idle-coalesced check re-verifies engine running / Idle.
        // ANLZ-07: force=true — the user explicitly asked for a restart; a
        // cached "converged" result must not suppress it.
        scheduleAnalyzeModeRestart(/*force=*/true);
    } else {
        // Q7: stop the current search, leave the process running. Orthogonal to
        // ENG-02 — deliberately NO revertEnginePlaysToOff() here.
        controller_.stopAnalysis();
    }
}

void AnalysisCoordinator::revertEnginePlaysToOff()
{
    MatchConfig mc = gameState_.matchConfig();
    if (mc.enginePlays == EnginePlaysSide::Off) return;  // nothing armed
    mc.enginePlays = EnginePlaysSide::Off;
    gameState_.setMatchConfig(mc);
    signal_engine_plays_reverted.emit();
    // Deliberately NO SettingsStorage::save — transient session action
    // (ENG-02); the persisted, user-chosen side is restored on next launch.
}

void AnalysisCoordinator::revertEnginePlaysIfEnginesTurn()
{
    if (isEnginesTurn(gameState_.matchConfig().enginePlays, gameState_.board().sideToMove()))
        revertEnginePlaysToOff();
}

void AnalysisCoordinator::stopAnalysis()
{
    revertEnginePlaysToOff();
    controller_.stopAnalysis();
}
