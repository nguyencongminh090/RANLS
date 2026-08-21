#include "board_renderer.h"

#include <algorithm>
#include <cmath>

// ── Board colors ─────────────────────────────────────────────────────────────
static constexpr double kBoardR = 0.87, kBoardG = 0.72, kBoardB = 0.53; // Wood
static constexpr double kGridR  = 0.20, kGridG  = 0.20, kGridB  = 0.18;
static constexpr double kCoordR = 0.60, kCoordG = 0.60, kCoordB = 0.55;
static constexpr double kLastR  = 0.85, kLastG  = 0.20, kLastB  = 0.20;
static constexpr double kHoverAlpha   = 0.4;
static constexpr double kGhostAlpha   = 0.35;
static constexpr double kMarkerAlpha  = 0.7;
static constexpr double kVariantR     = 0.90, kVariantG   = 0.65, kVariantB   = 0.15;
static constexpr double kDatabaseR    = 0.40, kDatabaseG  = 0.75, kDatabaseB  = 0.40;

static void set_source_from_winrate(const Cairo::RefPtr<Cairo::Context>& cr, double winrate, double alpha) {
    // HSV color from winrate: hue = (100-wr)*1.8 → 0°=green, 180°=red.
    double wrPct = std::clamp(winrate, 0.0, 1.0) * 100.0;
    double hue   = (100.0 - wrPct) * 1.8;  // degrees
    double sat   = 0.75;
    double val   = 0.85;

    // HSV → RGB conversion.
    double c = val * sat;
    double x2 = c * (1.0 - std::fabs(std::fmod(hue / 60.0, 2.0) - 1.0));
    double m2 = val - c;
    double rr, gg, bb;
    if      (hue < 60)  { rr = c;  gg = x2; bb = 0; }
    else if (hue < 120) { rr = x2; gg = c;  bb = 0; }
    else if (hue < 180) { rr = 0;  gg = c;  bb = x2; }
    else if (hue < 240) { rr = 0;  gg = x2; bb = c; }
    else if (hue < 300) { rr = x2; gg = 0;  bb = c; }
    else                { rr = c;  gg = 0;  bb = x2; }
    rr += m2; gg += m2; bb += m2;

    cr->set_source_rgba(rr, gg, bb, alpha);
}

static constexpr double kCoordMargin  = 24.0; // px reserved for coordinate labels

// ═════════════════════════════════════════════════════════════════════════════
BoardRenderer::BoardRenderer(const BoardViewModel &viewModel)
    : vm_(viewModel)
{
}

// ── Coordinate helpers ──────────────────────────────────────────────────────
double BoardRenderer::cellCenterX(int x) const
{
    return marginLeft_ + cellSize_ * (x + 0.5);
}

double BoardRenderer::cellCenterY(int y) const
{
    return marginTop_ + cellSize_ * (y + 0.5);
}

double BoardRenderer::stoneRadius() const
{
    return cellSize_ * 0.44;
}

// ═════════════════════════════════════════════════════════════════════════════
void BoardRenderer::draw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height)
{
    int bs = vm_.boardSize;
    if (bs <= 0) return;

    // Compute cell size to fit the available area (leaving coord margins).
    double usableW = width  - 2.0 * kCoordMargin;
    double usableH = height - 2.0 * kCoordMargin;
    cellSize_   = std::min(usableW, usableH) / bs;
    boardPx_    = static_cast<int>(cellSize_ * bs);
    marginLeft_ = (width  - boardPx_) / 2.0;
    marginTop_  = (height - boardPx_) / 2.0;

    // ── Layer pipeline ──────────────────────────────────────────────────────
    drawGrid(cr);
    drawStones(cr);
    drawLastMove(cr);
    drawDatabaseMarkers(cr);
    drawVariantMarkers(cr);
    drawCandidateMoves(cr);
    drawPVHighlight(cr);
    drawHover(cr);
}

// ── 1. GridLayer ─────────────────────────────────────────────────────────────
void BoardRenderer::drawGrid(const Cairo::RefPtr<Cairo::Context> &cr)
{
    int bs = vm_.boardSize;

    // Board background (wood color).
    cr->set_source_rgb(kBoardR, kBoardG, kBoardB);
    cr->rectangle(marginLeft_, marginTop_, boardPx_, boardPx_);
    cr->fill();

    // Grid lines.
    cr->set_source_rgb(kGridR, kGridG, kGridB);
    cr->set_line_width(1.0);
    for (int i = 0; i < bs; ++i) {
        double cx = cellCenterX(i);
        double cy = cellCenterY(i);

        // Vertical line.
        cr->move_to(cx, cellCenterY(0));
        cr->line_to(cx, cellCenterY(bs - 1));

        // Horizontal line.
        cr->move_to(cellCenterX(0), cy);
        cr->line_to(cellCenterX(bs - 1), cy);
    }
    cr->stroke();

    // Star points (tengen, corners, and edge midpoints), generalized from the
    // traditional 15×15 layout: offset-3-from-edge points plus the true
    // center. PROTO-02: this used to be hardcoded to `bs == 15`, so every
    // other board size silently lost its star points. Only odd sizes have a
    // single-intersection center, and sizes below 9 have no room for corner
    // and center star points to stay distinct -- both are cleanly omitted
    // rather than drawing something misleading.
    if (bs % 2 == 1 && bs >= 9) {
        double dotR = cellSize_ * 0.08;
        int offset = 3;
        int center = bs / 2;
        int far    = bs - 1 - offset;
        int stars[][2] = {
            {offset, offset}, {offset, far}, {far, offset}, {far, far},
            {center, center},
            {offset, center}, {far, center}, {center, offset}, {center, far},
        };
        cr->set_source_rgb(kGridR, kGridG, kGridB);
        for (auto &s : stars) {
            cr->arc(cellCenterX(s[0]), cellCenterY(s[1]), dotR, 0, 2 * M_PI);
            cr->fill();
        }
    }

    // Coordinate labels.
    if (vm_.viewConfig.showCoordinates) {
        cr->set_source_rgb(kCoordR, kCoordG, kCoordB);
        cr->select_font_face("sans-serif", Cairo::ToyFontFace::Slant::NORMAL,
                             Cairo::ToyFontFace::Weight::NORMAL);
        cr->set_font_size(std::max(9.0, cellSize_ * 0.35));

        Cairo::TextExtents ext;
        for (int i = 0; i < bs; ++i) {
            // Column labels (A-O) at top.
            char col = 'A' + i;
            std::string label(1, col);
            cr->get_text_extents(label, ext);
            cr->move_to(cellCenterX(i) - ext.width / 2.0, marginTop_ - 6.0);
            cr->show_text(label);

            // Row labels (15..1 top to bottom) at left.
            std::string row = std::to_string(bs - i);
            cr->get_text_extents(row, ext);
            cr->move_to(marginLeft_ - ext.width - 6.0, cellCenterY(i) + ext.height / 2.0);
            cr->show_text(row);
        }
    }
}

// ── 2. StoneLayer ────────────────────────────────────────────────────────────
void BoardRenderer::drawStones(const Cairo::RefPtr<Cairo::Context> &cr)
{
    double r = stoneRadius();
    bool hasLast = vm_.lastMove.isValid(vm_.boardSize);
    for (auto &[pos, stone] : vm_.stones) {
        double cx = cellCenterX(pos.x);
        double cy = cellCenterY(pos.y);

        if (stone == Stone::Black) {
            cr->set_source_rgb(0.1, 0.1, 0.1);
        } else {
            cr->set_source_rgb(0.95, 0.95, 0.95);
        }
        cr->arc(cx, cy, r, 0, 2 * M_PI);
        cr->fill();

        // Subtle border.
        cr->set_source_rgba(0.0, 0.0, 0.0, 0.3);
        cr->set_line_width(1.0);
        cr->arc(cx, cy, r, 0, 2 * M_PI);
        cr->stroke();

        if (vm_.viewConfig.showMoveNumbers) {
            auto it = std::find(vm_.moveHistory.begin(), vm_.moveHistory.end(), pos);
            if (it != vm_.moveHistory.end()) {
                int moveIndex = std::distance(vm_.moveHistory.begin(), it) + 1;
                std::string num = std::to_string(moveIndex);

                bool isLast = hasLast && pos == vm_.lastMove;
                if (isLast) {
                    // When move numbers are enabled, highlight the last move using red text
                    // instead of drawing an extra ring marker.
                    cr->set_source_rgb(kLastR, kLastG, kLastB);
                } else {
                    cr->set_source_rgb((stone == Stone::Black) ? 0.9 : 0.1,
                                       (stone == Stone::Black) ? 0.9 : 0.1,
                                       (stone == Stone::Black) ? 0.9 : 0.1);
                }
                
                cr->select_font_face("sans-serif", Cairo::ToyFontFace::Slant::NORMAL, Cairo::ToyFontFace::Weight::NORMAL);
                cr->set_font_size(std::max(8.0, cellSize_ * 0.45));
                Cairo::TextExtents ext;
                cr->get_text_extents(num, ext);
                cr->move_to(cx - ext.width / 2.0, cy + ext.height / 2.0);
                cr->show_text(num);
            }
        }
    }
}

// ── 3. LastMoveLayer ─────────────────────────────────────────────────────────
void BoardRenderer::drawLastMove(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (!vm_.lastMove.isValid(vm_.boardSize))
        return;
    if (vm_.viewConfig.showMoveNumbers) {
        // Last move is already highlighted as red move number.
        return;
    }

    double cx = cellCenterX(vm_.lastMove.x);
    double cy = cellCenterY(vm_.lastMove.y);
    double mr = stoneRadius() * 0.35;

    cr->set_source_rgb(kLastR, kLastG, kLastB);
    cr->set_line_width(2.0);
    cr->arc(cx, cy, mr, 0, 2 * M_PI);
    cr->stroke();
}

// ── 4. DatabaseMarkerLayer ───────────────────────────────────────────────────
void BoardRenderer::drawDatabaseMarkers(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (vm_.databaseMarkers.empty()) return;

    double r = stoneRadius() * 0.55;

    for (const auto &m : vm_.databaseMarkers) {
        if (!m.pos.isValid(vm_.boardSize)) continue;
        double cx = cellCenterX(m.pos.x);
        double cy = cellCenterY(m.pos.y);

        // Draw heat map diamond marker.
        if (m.eval >= 0) {
            set_source_from_winrate(cr, m.eval, kMarkerAlpha);
        } else {
            cr->set_source_rgba(kDatabaseR, kDatabaseG, kDatabaseB, kMarkerAlpha);
        }

        cr->move_to(cx, cy - r);
        cr->line_to(cx + r, cy);
        cr->line_to(cx, cy + r);
        cr->line_to(cx - r, cy);
        cr->close_path();
        cr->fill();

        // Label text.
        if (!m.label.empty()) {
            cr->set_source_rgba(1.0, 1.0, 1.0, 0.9);
            cr->set_font_size(std::max(8.0, cellSize_ * 0.28));
            Cairo::TextExtents ext;
            cr->get_text_extents(m.label, ext);
            cr->move_to(cx - ext.width / 2.0, cy + ext.height / 2.0);
            cr->show_text(m.label);
        }
    }
}

// ── 5. VariantMarkerLayer ────────────────────────────────────────────────────
void BoardRenderer::drawVariantMarkers(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (vm_.variantMarkers.empty()) return;

    double r = stoneRadius() * 0.30;
    cr->set_source_rgba(kVariantR, kVariantG, kVariantB, kMarkerAlpha);

    for (const auto &m : vm_.variantMarkers) {
        if (!m.pos.isValid(vm_.boardSize)) continue;
        double cx = cellCenterX(m.pos.x);
        double cy = cellCenterY(m.pos.y);
        cr->arc(cx, cy, r, 0, 2 * M_PI);
        cr->fill();
    }
}

// ── 6. CandidateMoveLayer (MultiPV with HSV winrate coloring) ────────────────
void BoardRenderer::drawCandidateMoves(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (vm_.candidateMoves.empty()) return;

    double r = stoneRadius() * 0.55;
    cr->set_font_size(std::max(8.0, cellSize_ * 0.32));

    for (const auto &m : vm_.candidateMoves) {
        if (!m.pos.isValid(vm_.boardSize)) continue;
        double cx = cellCenterX(m.pos.x);
        double cy = cellCenterY(m.pos.y);

        // Draw translucent circle with winrate color (heat map).
        set_source_from_winrate(cr, m.eval, 0.55);
        cr->arc(cx, cy, r, 0, 2 * M_PI);
        cr->fill();

        // Circle outline.
        set_source_from_winrate(cr, m.eval, 0.8);
        cr->set_line_width(1.5);
        cr->arc(cx, cy, r, 0, 2 * M_PI);
        cr->stroke();

        // Label text (white with shadow for readability).
        if (!m.label.empty()) {
            Cairo::TextExtents ext;
            cr->get_text_extents(m.label, ext);
            double tx = cx - ext.width / 2.0;
            double ty = cy + ext.height / 2.0;

            // Shadow.
            cr->set_source_rgba(0.0, 0.0, 0.0, 0.6);
            cr->move_to(tx + 1, ty + 1);
            cr->show_text(m.label);

            // Foreground.
            cr->set_source_rgba(1.0, 1.0, 1.0, 0.95);
            cr->move_to(tx, ty);
            cr->show_text(m.label);
        }
    }
}

// ── 7. PVHighlightLayer (ghost stones) ───────────────────────────────────────
void BoardRenderer::drawPVHighlight(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (vm_.pvPreview.empty()) return;

    double r     = stoneRadius();
    Stone  side  = vm_.hoverStone;  // Side to move determines first ghost color.

    for (size_t i = 0; i < vm_.pvPreview.size(); ++i) {
        const auto &pos = vm_.pvPreview[i];
        if (!pos.isValid(vm_.boardSize)) continue;

        double cx = cellCenterX(pos.x);
        double cy = cellCenterY(pos.y);

        // Alternate colors: first move = side to move, then alternating.
        Stone ghostColor = (i % 2 == 0) ? side
                           : (side == Stone::Black ? Stone::White : Stone::Black);

        if (ghostColor == Stone::Black)
            cr->set_source_rgba(0.1, 0.1, 0.1, kGhostAlpha);
        else
            cr->set_source_rgba(0.95, 0.95, 0.95, kGhostAlpha);

        cr->arc(cx, cy, r, 0, 2 * M_PI);
        cr->fill();

        // Draw move number on ghost stone.
        cr->set_source_rgba(ghostColor == Stone::Black ? 1.0 : 0.0,
                            ghostColor == Stone::Black ? 1.0 : 0.0,
                            ghostColor == Stone::Black ? 1.0 : 0.0,
                            0.7);
        cr->set_font_size(std::max(8.0, cellSize_ * 0.30));
        std::string num = std::to_string(i + 1);
        Cairo::TextExtents ext;
        cr->get_text_extents(num, ext);
        cr->move_to(cx - ext.width / 2.0, cy + ext.height / 2.0);
        cr->show_text(num);
    }
}

// ── 8. HoverLayer ────────────────────────────────────────────────────────────
void BoardRenderer::drawHover(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (!vm_.hoverMove.isValid(vm_.boardSize)) return;

    double cx = cellCenterX(vm_.hoverMove.x);
    double cy = cellCenterY(vm_.hoverMove.y);
    double r  = stoneRadius();

    if (vm_.hoverStone == Stone::Black)
        cr->set_source_rgba(0.1, 0.1, 0.1, kHoverAlpha);
    else
        cr->set_source_rgba(0.95, 0.95, 0.95, kHoverAlpha);

    cr->arc(cx, cy, r, 0, 2 * M_PI);
    cr->fill();
}
