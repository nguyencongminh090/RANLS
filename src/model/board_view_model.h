#pragma once

#include "board_state.h"
#include "config.h"
#include "engine/engine_types.h"   // AnalysisOverlay (PROTO-06)

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
    std::vector<Coord>           pvPreview;             ///< Ghost stone path for PV hover
    Coord                        hoverMove;             ///< Current mouse hover position
    Stone                        hoverStone = Stone::Empty;

    /// UI-03: Black's currently-forbidden points under Free Renju, computed by
    /// RenjuRule::forbiddenPoints() (src/model/renju_rule.h) -- domain logic,
    /// not BoardRenderer's job. Populated only when the active rule is Renju
    /// AND it is currently Black's turn to move (forbidden points are a
    /// Black-only, to-move concept; showing them on White's turn would be
    /// meaningless). Indication only -- BoardRenderer draws these, it never
    /// blocks a click on them (see docs/todo/UI-03-rule-not-visible-on-board.md).
    std::vector<Coord>           forbiddenPoints;

    /// PROTO-06: resolved single-winner-per-cell live search overlay. Replaces
    /// `candidateMoves`. Populated by update() ONLY while GameState::isAnalyzing()
    /// (live-only, matches the original Yixin-Board) AND
    /// ViewConfig::showSearchOverlay is on. Per-cell priority already resolved
    /// here: tag > lost > best > examined (pos==1) > examining (pos==2).
    struct SearchOverlayMark {
        enum class Kind { Tag, Lost, Best, Examined, Examining };
        Coord       pos;
        Kind        kind    = Kind::Tag;
        std::string label;             ///< winrate/mate text — Tag only
        double      winrate = 0.5;     ///< backs the HSV heat colour — Tag only
    };
    std::vector<SearchOverlayMark> searchOverlay;

private:
    GameState &state_;
};
