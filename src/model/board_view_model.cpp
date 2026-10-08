#include "board_view_model.h"
#include "game_state.h"
#include "renju_rule.h"
#include "i18n/i18n.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

/// Heat-colour winrate [0,1] of a database marker, read from the engine's own
/// display label like the original Yixin-Board (main.c: "W.." -> 100, "L.." -> 0,
/// "NN%" -> NN); -1 when the label carries no score ("D", empty, ...). The raw
/// record value is deliberately not converted: the engine's cp -> winrate scale
/// and sign are its own (label uses valueToWinRate(-value), scale != 200 for
/// yixin-net), so any local sigmoid disagrees with the "NN%" drawn on the marker.
static double databaseLabelWinrate(const std::string &label) {
    if (label.empty()) return -1.0;
    if (label[0] == 'W' || label[0] == 'w') return 1.0;
    if (label[0] == 'L' || label[0] == 'l') return 0.0;
    if (label.back() == '%') return std::clamp(std::atoi(label.c_str()), 0, 100) / 100.0;
    return -1.0;
}

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
            std::string line = i18n::format(i18n::tr("Engine: %s"), m.label);
            const auto &cells = state_.analysisOverlay().cells;
            auto it = cells.find(c);
            if (it != cells.end() && it->second.tagDepth > 0)
                line += " " + i18n::format(i18n::tr("(depth %d)"), it->second.tagDepth);
            addLine(line);
        } else if (m.kind == SearchOverlayMark::Kind::Lost) {
            addLine(i18n::tr("Losing move"));
        }
        if (m.isBest) addLine(i18n::tr("Best move"));
        break;
    }

    // Database entry (databaseMarkers is empty when showDatabase is off).
    for (const auto &m : databaseMarkers) {
        if (m.pos != c) continue;
        const auto &db = state_.database();
        auto it = db.find(c);
        if (it == db.end()) break;
        const DatabaseEntry &e = it->second;
        const std::string bound = e.bound == 0 ? i18n::tr("Exact")
                                  : e.bound == 1 ? i18n::tr("Alpha")
                                  : e.bound == 2 ? i18n::tr("Beta")
                                                 : i18n::tr("Unknown");
        const std::string &text = e.boardText.empty() ? e.label : e.boardText;
        addLine(text.empty() ? i18n::tr("Database") : i18n::format(i18n::tr("Database: %s"), text));
        addLine(i18n::format(i18n::tr("Value %d, depth %d, bound %s"), static_cast<int>(e.value),
                             static_cast<int>(e.depth), bound));
        if (m.isBest) addLine(i18n::tr("Best database move"));
        if (e.hasComment) addLine(i18n::tr("Has comment"));
        break;
    }

    // Variant marker.
    for (const auto &m : variantMarkers) {
        if (m.pos != c) continue;
        addLine(m.branchCount == 1 ? i18n::format(i18n::tr("%d variation"), m.branchCount)
                                   : i18n::format(i18n::tr("%d variations"), m.branchCount));
        break;
    }

    // Renju forbidden point -- indication only (UI-03), still playable.
    if (std::find(forbiddenPoints.begin(), forbiddenPoints.end(), c) != forbiddenPoints.end())
        addLine(i18n::tr("Forbidden for Black (still playable)"));

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
            m.eval  = databaseLabelWinrate(entry.label);
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
