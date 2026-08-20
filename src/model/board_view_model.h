#pragma once

#include "board_state.h"
#include "config.h"

#include <string>
#include <vector>

class GameState;

/// Render-ready data consumed by BoardRenderer.
/// Acts as the ViewModel (MVVM) — BoardRenderer never touches GameState directly.
class BoardViewModel {
public:
    /// A marker to draw on the board (variant branch, database entry, etc.).
    struct Marker {
        Coord       pos;
        std::string label;          ///< Short text label (e.g., "W5", "L3", "65%")
        double      eval = -1.0;    ///< Win rate [0,1], or -1 if N/A
    };

    explicit BoardViewModel(GameState &state);

    /// Rebuild all render-ready fields from the current GameState.
    void update();

    // ── Fields consumed by BoardRenderer ────────────────────────────────────
    int                          boardSize    = 15;
    ViewConfig                   viewConfig;                            ///< View settings for rendering
    std::vector<std::pair<Coord, Stone>> stones;                        ///< All placed stones
    std::vector<Coord>           moveHistory;                           ///< Ordered sequence of played stones
    Coord                        lastMove;              ///< Last move highlight
    std::vector<Marker>          variantMarkers;        ///< Variation branch markers
    std::vector<Marker>          databaseMarkers;       ///< Database move markers
    std::vector<Marker>          candidateMoves;        ///< Engine MultiPV candidates (optional)
    std::vector<Coord>           pvPreview;             ///< Ghost stone path for PV hover
    Coord                        hoverMove;             ///< Current mouse hover position
    Stone                        hoverStone = Stone::Empty;

private:
    GameState &state_;
};
