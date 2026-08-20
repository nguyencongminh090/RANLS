#pragma once

#include "model/board_state.h" // For Coord
#include <cstdint>
#include <string>
#include <vector>

/// Category of an engine output message.
enum class EngineMessageType {
    Output,
    Coord,
    Message,
    Error,
    Debug
};

/// Engine analysis data for a single PV line.
struct PVLine {
    int                pvIndex = 0;    ///< 1-based MultiPV slot from the engine.
    int                depth  = 0;
    int                selDepth = 0;
    int64_t            nodes  = 0;
    double             score  = 0.5;   ///< Win rate [0.0, 1.0]
    int                mateStep = 0;   ///< Positive = winning in N, negative = losing in N
    std::string        evalText;
    std::vector<Coord> moves;
};

/// Snapshot of engine status.
struct EngineStatus {
    int     depth    = 0;
    int     selDepth = 0;
    int64_t nodes    = 0;
    int64_t nps      = 0;
    int64_t timeMs   = 0;
    double  winrate  = 0.5;   ///< Win rate [0.0, 1.0]
    int     mateStep = 0;     ///< Positive = winning in N, negative = losing in N
    std::string evalText;
    Coord   bestMove;
};

/// Entry from the engine's internal database for a specific coordinate.
struct DatabaseEntry {
    Coord       pos;
    std::string label;       ///< e.g. "W", "L", "D", "VCF"
    int         value = 0;   ///< Engine-internal evaluation
    int         depth = 0;   ///< Depth of stored evaluation
    int         bound = 0;   ///< 0=Exact, 1=Alpha, 2=Beta (matches engine)
    bool        hasComment = false;
    std::string boardText;   ///< Extra text to display on the board (e.g. VCF step count)
};
