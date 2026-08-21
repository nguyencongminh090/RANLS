#include "engine_controller.h"
#include "gomocup_protocol.h"
#include <iostream>

EngineController::EngineController(GameState &gameState, EngineProcess &engine)
    : gameState_(gameState)
    , engine_(engine)
{
    // Default to Gomocup protocol. In the future this could be configurable.
    protocol_ = std::make_unique<GomocupProtocol>(gameState_.boardSize());
    connectProtocolSignals();

    engine_.signal_line_received.connect(
        sigc::mem_fun(*this, &EngineController::onEngineLine));

    engine_.signal_process_died.connect([this]() {
        // Only a real transition: dying while NotStarted (nothing to crash)
        // or already Crashed/Stopping-to-completion is not a fresh event.
        if (state_ == EngineState::NotStarted || state_ == EngineState::Crashed) return;
        gameState_.setAnalyzing(false);
        setState(EngineState::Crashed);
    });
}

EngineController::~EngineController()
{
    // No GLib main loop is guaranteed to be running by the time this runs, so
    // we cannot rely on stopEngine()'s async completion callback ever firing.
    // Force-kill synchronously instead (EngineProcess::stop() — immediate,
    // no g_usleep, no g_main_context_iteration). This is deliberately not
    // stopEngine(): see docs/instruction/ENG-01-engine-state-honesty-and-blocking-stop.md.
    engine_.stop();
}

void EngineController::connectProtocolSignals() {
    protocol_->signal_log.connect([this](EngineMessageType type, std::string line) {
        std::string typeStr;
        switch (type) {
            case EngineMessageType::Message: typeStr = "Message"; break;
            case EngineMessageType::Coord:   typeStr = "Coord"; break;
            case EngineMessageType::Error:   typeStr = "Error"; break;
            case EngineMessageType::Debug:   typeStr = "Debug"; break;
            default:                         typeStr = "Output"; break;
        }
        signal_engine_output.emit(typeStr, line);
    });

    protocol_->signal_move.connect([this](Coord move) {
        if (state_ == EngineState::Analyzing) {
            gameState_.setAnalyzing(false);
            setState(EngineState::Idle);
            // RT-01: search completion must not wait for the next throttle
            // tick — flush any coalesced analysis update immediately so the
            // final PV/status is never delayed or dropped.
            gameState_.flush();
        }
        signal_engine_move.emit(move);
    });

    protocol_->signal_analysis.connect([this](const std::vector<PVLine>& pvs, const EngineStatus& status) {
        gameState_.setAnalysisData(pvs, status);
    });

    protocol_->signal_database_entry.connect([this](const DatabaseEntry& entry) {
        signal_database_entry.emit(entry);
    });
    protocol_->signal_database_refresh.connect([this]() {
        signal_database_refresh.emit();
    });
    protocol_->signal_database_done.connect([this]() {
        signal_database_done.emit();
    });
}

void EngineController::setState(EngineState next)
{
    if (state_ == next) return;
    state_ = next;
    signal_state_changed.emit(state_);
}

bool EngineController::isUsable() const
{
    return state_ == EngineState::Starting || state_ == EngineState::Idle
        || state_ == EngineState::Analyzing;
}

// ─── Engine lifecycle ────────────────────────────────────────────────────────
void EngineController::startEngine()
{
    // Already starting/running: no-op. Crashed is a legitimate restart point.
    if (state_ != EngineState::NotStarted && state_ != EngineState::Crashed) return;

    const auto &cfg = gameState_.engineConfig();
    if (cfg.enginePath.empty()) return;

    setState(EngineState::Starting);

    bool ok = engine_.start(cfg.enginePath);
    if (!ok) {
        // ENG-01: honor EngineProcess::start()'s return value. Do not claim
        // a running state, and surface a visible error naming the path that
        // failed (routed to the bottom-panel console via signal_engine_output,
        // the same path used for all other engine-originated messages).
        signal_engine_output.emit("Error", "Failed to start engine: " + cfg.enginePath);
        setState(EngineState::NotStarted);
        return;
    }

    setState(EngineState::Idle);
}

void EngineController::stopEngine(std::function<void()> onComplete)
{
    if (state_ == EngineState::NotStarted) {
        if (onComplete) onComplete();
        return;
    }

    if (state_ == EngineState::Stopping) {
        // Already stopping — chain this caller's completion onto the
        // in-flight shutdown instead of firing it early. Firing early would
        // let a caller (e.g. MainWindow::onQuit()) close/destroy `this`
        // before the original async stop has actually settled.
        if (onComplete) {
            auto prev = std::move(pendingStopComplete_);
            pendingStopComplete_ = [prev, onComplete]() {
                if (prev) prev();
                onComplete();
            };
        }
        return;
    }

    if (engine_.isRunning()) {
        sendRawCommand("YXSAVEDATABASE");
        sendRawCommand("END");
    }

    gameState_.setAnalyzing(false);
    setState(EngineState::Stopping);

    // Non-blocking: no g_usleep, no g_main_context_iteration (ENG-01). The UI
    // stays responsive and shows "Stopping" until this completes, either when
    // the process exits or when EngineProcess's grace-period timeout forces it.
    //
    // This completion callback may run long after this call returns (see
    // EngineProcess::stopAsync()) — possibly after `this` has been destroyed.
    // Guard with a weak_ptr to lifetimeGuard_ rather than touching `this`
    // unconditionally.
    std::weak_ptr<void> guard = lifetimeGuard_;
    engine_.stopAsync([this, guard, onComplete]() {
        if (guard.expired()) return; // EngineController was destroyed meanwhile.
        setState(EngineState::NotStarted);
        auto pending = std::move(pendingStopComplete_);
        if (onComplete) onComplete();
        if (pending) pending();
    });
}

void EngineController::reloadEngine()
{
    stopEngine([this]() {
        startEngine();
        if (state_ == EngineState::Idle) sendConfig();
    });
}

void EngineController::sendConfig()
{
    if (!isUsable()) return;

    const auto &cfg = gameState_.engineConfig();

    for (const auto& cmd : protocol_->generateStart(gameState_.boardSize())) {
        engine_.sendLine(cmd);
    }

    for (const auto& cmd : protocol_->generateConfig(cfg)) {
        engine_.sendLine(cmd);
    }

    for (const auto& cmd : protocol_->generateRule(gameState_.rule())) {
        engine_.sendLine(cmd);
    }
}

// ─── Analysis ────────────────────────────────────────────────────────────────
void EngineController::analyze()
{
    if (state_ != EngineState::Idle) return;

    auto path = gameState_.currentPath();
    const auto &cfg = gameState_.engineConfig();

    for (const auto& cmd : protocol_->generateAnalyzeRequest(path, cfg.multiPV)) {
        engine_.sendLine(cmd);
    }

    gameState_.setAnalyzing(true);
    setState(EngineState::Analyzing);
}

void EngineController::stopAnalysis()
{
    // Always forward STOP when the engine is usable.
    if (!isUsable()) return;

    engine_.sendLine(protocol_->generateStop());

    if (state_ == EngineState::Analyzing) {
        gameState_.setAnalyzing(false);
        setState(EngineState::Idle);
        // RT-01: analysis-stopped must emit immediately, not wait for the
        // next throttle tick.
        gameState_.flush();
    }
}

void EngineController::queryDatabase() {
    if (!isUsable()) return;
    auto cmds = protocol_->generateDatabaseQuery(gameState_.currentPath());
    for (const auto& cmd : cmds) {
        engine_.sendLine(cmd);
    }
}

void EngineController::sendRawCommand(const std::string &command)
{
    if (!isUsable()) return;
    engine_.sendLine(command);
}

// ─── Line parsing ────────────────────────────────────────────────────────────
void EngineController::onEngineLine(const std::string &line)
{
    // Simply pipe to protocol parser. The protocol emits domain signals.
    protocol_->parseLine(line);
}
