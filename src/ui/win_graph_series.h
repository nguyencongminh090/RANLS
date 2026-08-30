#pragma once

// UX-06: pure win-graph series construction, split out of analysis_panel.cpp
// so it can be unit-tested without gtkmm (see tests/test_ux06_wingraph_series.cpp).
//
// Input `raw[i]` is the eval of the position AFTER move i, scored from the
// side to move IN THAT position (UI-01 attribution — not re-derived here).
// Move 0 is Black's move, so Black is to move only when i is odd. `raw[i]`
// may be NaN — an explicit "unevaluated" sentinel (GameState::evalHistory);
// it is propagated as a gap, never clamped to a false 50%.
//
//  - SingleSide: ONE perspective-correct line in `black`; `white` is left
//    empty. Perspective is Black by default, or White when the engine plays
//    White (MatchConfig::enginePlays); Off / Black → Black perspective.
//  - BothSide: `black` = Black's per-move win-rate, `white` = White's
//    (its complement) — each colour shown in its own perspective.

#include "model/config.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

struct WinGraphSeries {
    std::vector<double> black;
    std::vector<double> white;
};

inline WinGraphSeries buildWinGraphSeries(const std::vector<double> &raw,
                                          WinGraphMode mode,
                                          EnginePlaysSide enginePlays)
{
    WinGraphSeries out;
    out.black.reserve(raw.size());
    if (mode == WinGraphMode::BothSide)
        out.white.reserve(raw.size());

    const bool whitePerspective =
        (mode == WinGraphMode::SingleSide && enginePlays == EnginePlaysSide::White);

    for (size_t i = 0; i < raw.size(); ++i) {
        bool blackToMove = (i % 2 == 1);

        if (std::isnan(raw[i])) {
            double nan = std::numeric_limits<double>::quiet_NaN();
            out.black.push_back(nan);
            if (mode == WinGraphMode::BothSide)
                out.white.push_back(nan);
            continue;
        }

        double sideToMove = std::clamp(raw[i], 0.0, 1.0);
        double blackWin    = blackToMove ? sideToMove : (1.0 - sideToMove);

        if (mode == WinGraphMode::BothSide) {
            out.black.push_back(blackWin);
            out.white.push_back(1.0 - blackWin);
        } else {
            out.black.push_back(whitePerspective ? (1.0 - blackWin) : blackWin);
        }
    }
    return out;
}
