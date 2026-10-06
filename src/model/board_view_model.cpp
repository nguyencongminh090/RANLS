#include "board_view_model.h"
#include "game_state.h"
#include "renju_rule.h"
#include <cmath>
#include <algorithm>

BoardViewModel::BoardViewModel(GameState &state)
    : state_(state)
{
}

bool BoardViewModel::isOccupied(Coord c) const
{
    for (const auto &entry : stones)
        if (entry.first == c) return true;
    return false;
}

int BoardViewModel::moveNumberAt(Coord c) const
{
    if (!c.isValid(boardSize)) return 0;
    const size_t i = static_cast<size_t>(c.y) * boardSize + c.x;
    return i < moveNumber_.size() ? moveNumber_[i] : 0;
}

std::string BoardViewModel::tooltipFor(Coord c) const
{
    if (!c.isValid(boardSize) || isOccupied(c)) return {};

    std::string out;
    auto addLine = [&out](const std::string &line) {
        if (!out.empty()) out += '\n';
        out += line;
    };

    // Engine mark (searchOverlay is only populated while analysing with
    // showSearchOverlay on). Examined/Examining dots add no text: a tooltip on
    // every dot of a running search would be noise.
    for (const auto &m : searchOverlay) {
        if (m.pos != c) continue;
        if (m.kind == SearchOverlayMark::Kind::Tag) {
            std::string line = "Engine: " + m.label;
            const auto &cells = state_.analysisOverlay().cells;
            auto it = cells.find(c);
            if (it != cells.end() && it->second.tagDepth > 0)
                line += " (depth " + std::to_string(it->second.tagDepth) + ")";
            addLine(line);
        } else if (m.kind == SearchOverlayMark::Kind::Lost) {
            addLine("Losing move");
        }
        if (m.isBest) addLine("Best move");
        break;
    }

    // Database entry (databaseMarkers is empty when showDatabase is off).
    for (const auto &m : databaseMarkers) {
        if (m.pos != c) continue;
        const auto &db = state_.database();
        auto it = db.find(c);
        if (it == db.end()) break;
        const DatabaseEntry &e = it->second;
        const char *bound = e.bound == 0 ? "Exact" : e.bound == 1 ? "Alpha" : e.bound == 2 ? "Beta" : "Unknown";
        std::string head = "Database";
        const std::string &text = e.boardText.empty() ? e.label : e.boardText;
        if (!text.empty()) head += ": " + text;
        addLine(head);
        addLine("Value " + std::to_string(e.value) + ", depth " + std::to_string(e.depth) +
                ", bound " + bound);
        if (m.isBest) addLine("Best database move");
        if (e.hasComment) addLine("Has comment");
        break;
    }

    // Variant marker.
    for (const auto &m : variantMarkers) {
        if (m.pos != c) continue;
        addLine(std::to_string(m.branchCount) + (m.branchCount == 1 ? " variation" : " variations"));
        break;
    }

    // Renju forbidden point -- indication only (UI-03), still playable.
    if (std::find(forbiddenPoints.begin(), forbiddenPoints.end(), c) != forbiddenPoints.end())
        addLine("Forbidden for Black (still playable)");

    return out;
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
    // UX-07: Coord -> move number, built once per update(). First occurrence
    // wins (same as the std::find it replaces).
    moveNumber_.assign(static_cast<size_t>(boardSize) * boardSize, 0);
    for (size_t i = 0; i < moveHistory.size(); ++i) {
        const Coord &c = moveHistory[i];
        if (!c.isValid(boardSize)) continue;
        int &slot = moveNumber_[static_cast<size_t>(c.y) * boardSize + c.x];
        if (slot == 0) slot = static_cast<int>(i) + 1;
    }

    // ── Variant markers from the variation tree ─────────────────────────────
    variantMarkers.clear();
    auto path     = state_.currentPath();
    auto branches = state_.tree().getBranchCoords(path);
    for (const auto &coord : branches) {
        // UX-07: branchCount = children of the current node. getBranchCoords()
        // lists those children in insertion order (index 0 = main line).
        Marker m;
        m.pos         = coord;
        m.branchCount = static_cast<int>(branches.size());
        variantMarkers.push_back(m);
    }

    // ── PROTO-06: live per-cell search overlay (replaces candidateMoves) ────
    // Live-only: copied into render state ONLY while the engine is analyzing
    // (matches the original Yixin-Board — the overlay vanishes on search end).
    // Single-winner priority per empty cell: tag > lost > best > examined >
    // examining. `showSearchOverlay` is the master switch; `showSearchWinrate`
    // drops only the text tags, letting the mark below it win instead.
    searchOverlay.clear();
    if (state_.isAnalyzing() && viewConfig.showSearchOverlay) {
        const AnalysisOverlay &ov = state_.analysisOverlay();
        const bool showTags = viewConfig.showSearchWinrate;
        auto emptyCell = [&](const Coord &c) {
            return c.isValid(boardSize) && state_.board().stoneAt(c) == Stone::Empty;
        };

        for (const auto &[c, cell] : ov.cells) {
            if (!emptyCell(c)) continue;
            SearchOverlayMark m;
            m.pos    = c;
            // UI-18: best is a flag, not a rank -- a tagged best move keeps both.
            m.isBest = (c == ov.bestMove);
            if (showTags && !cell.tag.empty()) {
                m.kind    = SearchOverlayMark::Kind::Tag;
                m.label   = cell.tag;
                m.winrate = cell.tagWinrate;
            } else if (cell.lost) {
                m.kind = SearchOverlayMark::Kind::Lost;
            } else if (c == ov.bestMove) {
                m.kind = SearchOverlayMark::Kind::Best;
            } else if (cell.pos == 1) {
                m.kind = SearchOverlayMark::Kind::Examined;
            } else if (cell.pos == 2) {
                m.kind = SearchOverlayMark::Kind::Examining;
            } else {
                continue;   // cell carries no currently-visible state
            }
            searchOverlay.push_back(m);
        }

        // BEST may have arrived before any POS/tag for that cell — no map entry.
        if (emptyCell(ov.bestMove) && ov.cells.find(ov.bestMove) == ov.cells.end()) {
            SearchOverlayMark m;
            m.pos    = ov.bestMove;
            m.kind   = SearchOverlayMark::Kind::Best;
            m.isBest = true;
            searchOverlay.push_back(m);
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
        // UX-07: flag the best entry (highest value; ties -> all tied flagged).
        if (!databaseMarkers.empty()) {
            int bestValue = state_.database().begin()->second.value;
            for (const auto &kv : state_.database())
                bestValue = std::max(bestValue, kv.second.value);
            size_t i = 0;
            for (const auto &kv : state_.database())
                databaseMarkers[i++].isBest = (kv.second.value == bestValue);
        }
    }

    // ── Renju forbidden points (UI-03) ──────────────────────────────────────
    // Domain logic lives in RenjuRule (src/model/renju_rule.h) -- this just
    // pulls the already-computed coordinates for BoardRenderer to draw.
    // Black-only, and only meaningful on Black's turn to move.
    forbiddenPoints.clear();
    if (state_.rule() == GameRule::Renju && state_.board().sideToMove() == Stone::Black) {
        forbiddenPoints = RenjuRule::forbiddenPoints(state_.board());
    }

    // pvPreview is set externally (by UI interactions).
}
