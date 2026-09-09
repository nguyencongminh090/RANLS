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

    /// PROTO-07: generate commands that set `path` as the position and start a
    /// `YXANALZ` search restricted to an explicit allow-list of root moves —
    /// the inverse of `YXBLOCK`. Same `YXBOARD … DONE` position block as
    /// generateAnalyzeRequest(), followed by `YXANALZ` / one coordinate per
    /// line / `DONE`. The engine searches exactly `moves` (one PV line each)
    /// and finishes with a bare best-move coordinate, which the caller treats
    /// as search-completion only (the ANLZ-06 SearchIntent gate). The whitelist
    /// is one-shot engine-side — no client mirror state.
    virtual std::vector<std::string> generateAnalyzeMovesRequest(const std::vector<Coord>& path,
                                                                 const std::vector<Coord>& moves) = 0;

    /// Generate commands that set the given position and ask the engine to
    /// produce (and commit to) a single move for the side to move — as opposed
    /// to generateAnalyzeRequest(), which only asks for analysis. UI-06 uses
    /// this for the "Engine plays <side>" auto-move feature. The engine replies
    /// with one coordinate line, surfaced via signal_move.
    virtual std::vector<std::string> generateMoveRequest(const std::vector<Coord>& path) = 0;

    /// Generate the command to stop the current analysis.
    virtual std::string generateStop() = 0;

    /// Discard any analysis the protocol is still accumulating for the current
    /// think (PV lines, per-PV state machine, cached status). UI-04: a position
    /// change invalidates in-flight analysis, and a late async engine message
    /// for the previous position must not be able to repopulate stale PV rows.
    /// generateAnalyzeRequest() also performs this clear at the start of a fresh
    /// analysis.
    virtual void clearAnalysisState() = 0;

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

    /// PROTO-06: emitted when the live per-cell search overlay changes — a
    /// REALTIME pos/lost/best/refresh line, or an `INFO PV DONE` winrate tag.
    /// Coalesced downstream exactly like signal_analysis (GameState dirty flag
    /// + tick). Independent of signal_analysis: it never routes through
    /// GameState::setAnalysisData (no tree-node / eval-history writes).
    sigc::signal<void(const AnalysisOverlay&)> signal_analysis_overlay;

    /// Emitted when a single database record is parsed.
    sigc::signal<void(const DatabaseEntry&)> signal_database_entry;

    /// Emitted when a database query starts (clearing previous results).
    sigc::signal<void()> signal_database_refresh;

    /// Emitted when a database refresh is completed.
    sigc::signal<void()> signal_database_done;
};
