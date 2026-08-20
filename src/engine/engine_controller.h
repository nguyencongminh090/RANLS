#pragma once

#include "engine_process.h"
#include "i_engine_protocol.h"
#include "model/game_state.h"

#include <memory>
#include <sigc++/sigc++.h>
#include <string>

/// High-level controller bridging GameState ↔ EngineProcess via IEngineProtocol.
class EngineController {
public:
    EngineController(GameState &gameState, EngineProcess &engine);
    ~EngineController();

    /// Start/restart the engine with current config.
    void startEngine();

    /// Stop the engine process.
    void stopEngine();

    /// Stop then restart the engine.
    void reloadEngine();

    /// Send the current board position and request analysis.
    void analyze();

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

    /// Signal for engine state changes (true = running, false = stopped).
    sigc::signal<void(bool)> signal_engine_state;

    /// Signal for analysis state changes (true = analyzing, false = stopped).
    sigc::signal<void(bool)> signal_analyzing_state;

    /// Signal when a database entry is received.
    sigc::signal<void(const DatabaseEntry&)> signal_database_entry;

    /// Signal when a database query starts (clearing previous results).
    sigc::signal<void()> signal_database_refresh;
    
    /// Signal when a database refresh is done.
    sigc::signal<void()> signal_database_done;

    bool isAnalyzing() const { return analyzing_; }
    bool isStarted()   const { return started_; }

private:
    void onEngineLine(const std::string &line);
    void connectProtocolSignals();

    GameState     &gameState_;
    EngineProcess &engine_;
    std::unique_ptr<IEngineProtocol> protocol_;

    bool analyzing_ = false;
    bool started_   = false;
};
