#pragma once

#include "model/board_state.h" // For Coord
#include <cstdint>
#include <map>
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

/// PROTO-06: per-empty-cell live search overlay, mirroring the original
/// Yixin-Board `board*[y][x]` arrays (main.c). Fed by the engine's REALTIME
/// feed (`pos`/`lost`/`best`) and by `INFO PV DONE` (a winrate/mate text `tag`
/// plus the root depth it was written at). Live-only — rendered only while
/// GameState::isAnalyzing().
struct AnalysisOverlayCell {
    std::string tag;              ///< "62%", "+M7", "-M3", "D"; empty = no tag
    int         tagDepth   = 0;   ///< root depth the tag was written at (stale-tag cleanup key)
    double      tagWinrate = 0.5; ///< winrate [0,1] backing `tag`, for the HSV heat colour
    int         pos  = 0;         ///< 0 none, 1 examined (REALTIME DONE), 2 examining (REALTIME POS)
    bool        lost = false;     ///< REALTIME LOST — a losing root move
};

struct AnalysisOverlay {
    std::map<Coord, AnalysisOverlayCell> cells;
    Coord bestMove;               ///< REALTIME BEST — current best root move (own highlight)

    bool empty() const { return cells.empty() && bestMove == Coord {}; }
    void clear() { cells.clear(); bestMove = Coord {}; }
    /// REALTIME REFRESH — clear only the examining/examined marks (new depth).
    void clearPos() { for (auto &kv : cells) kv.second.pos = 0; }
};
