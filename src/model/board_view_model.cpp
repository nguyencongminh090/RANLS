#include "board_view_model.h"
#include "game_state.h"
#include <cmath>
#include <algorithm>

BoardViewModel::BoardViewModel(GameState &state)
    : state_(state)
{
}

void BoardViewModel::update()
{
    boardSize  = state_.boardSize();
    viewConfig = state_.viewConfig();

    // ── Stones ──────────────────────────────────────────────────────────────
    stones.clear();
    for (int y = 0; y < boardSize; ++y) {
        for (int x = 0; x < boardSize; ++x) {
            Stone s = state_.board().stoneAt({x, y});
            if (s != Stone::Empty) {
                stones.push_back({{x, y}, s});
            }
        }
    }

    // ── Last move ───────────────────────────────────────────────────────────
    lastMove = state_.lastMove();

    // ── Move history for move-number rendering ──────────────────────────────
    moveHistory.clear();
    const auto &hist = state_.history();
    for (int i = 0; i < hist.moveCount(); ++i) {
        moveHistory.push_back(hist.moves()[i]);
    }

    // ── Variant markers from the variation tree ─────────────────────────────
    variantMarkers.clear();
    auto path     = state_.currentPath();
    auto branches = state_.tree().getBranchCoords(path);
    for (const auto &coord : branches) {
        variantMarkers.push_back({coord, "", -1.0});
    }

    // ── Candidate moves from engine PV lines (optional, MultiPV) ────────────
    candidateMoves.clear();
    for (const auto &pv : state_.pvLines()) {
        if (!pv.moves.empty()) {
            Marker m;
            m.pos  = pv.moves[0];
            m.eval = pv.score;

            // Short label: win rate as percentage.
            if (pv.mateStep > 0) {
                m.label = "+M" + std::to_string(pv.mateStep);
            } else if (pv.mateStep < 0) {
                m.label = "-M" + std::to_string(-pv.mateStep);
            } else {
                int pct = static_cast<int>(pv.score * 100.0);
                m.label = std::to_string(pct) + "%";
            }
            candidateMoves.push_back(m);
        }
    }

    // ── Hover stone color ───────────────────────────────────────────────────
    hoverStone = state_.board().sideToMove();

    // ── Database markers ────────────────────────────────────────────────────
    databaseMarkers.clear();
    if (viewConfig.showDatabase) {
        for (const auto &[coord, entry] : state_.database()) {
            Marker m;
            m.pos   = coord;
            m.label = entry.boardText.empty() ? entry.label : entry.boardText;
            // Normalize centipawns to [0, 1] winrate using the same sigmoid as the engine.
            m.eval  = 1.0 / (1.0 + std::exp(-static_cast<double>(entry.value) / 200.0));
            databaseMarkers.push_back(m);
        }
    }

    // pvPreview is set externally (by UI interactions).
}
