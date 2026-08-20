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
        if (started_) {
            started_ = false;
            analyzing_ = false;
            signal_engine_state.emit(false);
            signal_analyzing_state.emit(false);
        }
    });
}

EngineController::~EngineController()
{
    stopEngine();
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
        if (analyzing_) {
            analyzing_ = false;
            gameState_.setAnalyzing(false);
            signal_analyzing_state.emit(false);
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

// ─── Engine lifecycle ────────────────────────────────────────────────────────
void EngineController::startEngine()
{
    if (started_) return;

    const auto &cfg = gameState_.engineConfig();
    if (cfg.enginePath.empty()) return;

    engine_.start(cfg.enginePath);
    started_ = true;
    signal_engine_state.emit(true);
}

void EngineController::stopEngine()
{
    if (started_ && engine_.isRunning()) {
        sendRawCommand("YXSAVEDATABASE");
        sendRawCommand("END");
        // We wait for the engine to flush its buffers.
        g_usleep(500000); // 500ms
    }
    engine_.stop();
    started_ = false;
    analyzing_ = false;
    signal_engine_state.emit(false);
    signal_analyzing_state.emit(false);
}

void EngineController::reloadEngine()
{
    stopEngine();
    startEngine();
    if (started_) sendConfig();
}

void EngineController::sendConfig()
{
    if (!started_) return;

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
    if (!started_ || analyzing_) return;

    analyzing_ = true;
    
    auto path = gameState_.currentPath();
    const auto &cfg = gameState_.engineConfig();

    for (const auto& cmd : protocol_->generateAnalyzeRequest(path, cfg.multiPV)) {
        engine_.sendLine(cmd);
    }
    
    gameState_.setAnalyzing(true);
    signal_analyzing_state.emit(true);
}

void EngineController::stopAnalysis()
{
    // Always forward STOP when the engine is running.
    if (!started_) return;

    engine_.sendLine(protocol_->generateStop());

    if (analyzing_) {
        analyzing_ = false;
        gameState_.setAnalyzing(false);
        signal_analyzing_state.emit(false);
    }
}

void EngineController::queryDatabase() {
    if (!started_) return;
    auto cmds = protocol_->generateDatabaseQuery(gameState_.currentPath());
    for (const auto& cmd : cmds) {
        engine_.sendLine(cmd);
    }
}

void EngineController::sendRawCommand(const std::string &command)
{
    if (!started_) return;
    engine_.sendLine(command);
}

// ─── Line parsing ────────────────────────────────────────────────────────────
void EngineController::onEngineLine(const std::string &line)
{
    // Simply pipe to protocol parser. The protocol emits domain signals.
    protocol_->parseLine(line);
}
