#pragma once

#include "model/config.h"
#include "model/board_state.h" // For Coord
#include "model/game_state.h"  // For GameRule
#include "engine_types.h"

#include <sigc++/sigc++.h>
#include <string>
#include <vector>

/// Abstract interface for bridging GUI interactions with engine subprocess streams.
/// Implementations of this class translate domain models into raw string commands,
/// and parse raw engine stdout strings into strongly-typed domain events.
class IEngineProtocol {
public:
    virtual ~IEngineProtocol() = default;

    // ─── GUI -> Engine Generators ───────────────────────────────────────────
    /// Generate the command to start a new game (e.g. "START 15").
    virtual std::vector<std::string> generateStart(int boardSize) = 0;

    /// Generate the command to set the rule (e.g. "INFO rule 0").
    virtual std::vector<std::string> generateRule(GameRule rule) = 0;

    /// Generate commands to send configuration parameters (threads, hash, timeouts).
    virtual std::vector<std::string> generateConfig(const EngineConfig& cfg) = 0;

    /// Generate commands to send the current board state and start analysis.
    virtual std::vector<std::string> generateAnalyzeRequest(const std::vector<Coord>& path, int multiPV) = 0;

    /// Generate the command to stop the current analysis.
    virtual std::string generateStop() = 0;

    /// Generate the command to quit the engine.
    virtual std::string generateQuit() = 0;

    /// Generate the command to query the internal database for the current position.
    virtual std::vector<std::string> generateDatabaseQuery(const std::vector<Coord>& path) = 0;

    // ─── Engine -> GUI Parsers ──────────────────────────────────────────────
    /// Feed a raw string received from the engine's stdout into the protocol state machine.
    /// The protocol will emit signals as complete messages/events are formed.
    virtual void parseLine(const std::string& line) = 0;

    // ─── Signals ────────────────────────────────────────────────────────────
    /// Emitted when a human-readable log message (or explicit debug/error) is produced.
    sigc::signal<void(EngineMessageType, std::string)> signal_log;

    /// Emitted when the engine proposes a final move.
    sigc::signal<void(Coord)> signal_move;

    /// Emitted when the engine provides ongoing analysis updates (PVs, Depth, Eval, NPS).
    sigc::signal<void(const std::vector<PVLine>&, const EngineStatus&)> signal_analysis;

    /// Emitted when a single database record is parsed.
    sigc::signal<void(const DatabaseEntry&)> signal_database_entry;

    /// Emitted when a database query starts (clearing previous results).
    sigc::signal<void()> signal_database_refresh;

    /// Emitted when a database refresh is completed.
    sigc::signal<void()> signal_database_done;
};
