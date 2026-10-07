#include "board_renderer.h"
#include "board_geometry.h"

#include <algorithm>
#include <cmath>

// ── Board colors ─────────────────────────────────────────────────────────────
static constexpr double kBoardR = 0.87, kBoardG = 0.72, kBoardB = 0.53; // Wood
static constexpr double kGridR  = 0.20, kGridG  = 0.20, kGridB  = 0.18;
// UX-06: the coordinate labels are drawn in the margin *outside* the wood
// board (on the widget background), so their colour is not a fixed constant
// any more -- BoardView feeds BoardRenderer the themed foreground colour via
// setCoordinateColor() each frame (see BoardRenderer::coordR_/G_/B_). The
// earlier UX-03 fix assumed the labels sat on the wood; they never did.
static constexpr double kLastR  = 0.85, kLastG  = 0.20, kLastB  = 0.20;
// UX-08: hover crosshair + emphasised margin labels.
static constexpr double kCrossR = 0.10, kCrossG = 0.25, kCrossB = 0.55, kCrossAlpha = 0.28;
static constexpr double kLabelHotR = 0.15, kLabelHotG = 0.50, kLabelHotB = 0.90;
static constexpr double kHoverAlpha  = 0.4;
static constexpr double kGhostAlpha   = 0.35;
static constexpr double kMarkerAlpha  = 0.7;
static constexpr double kVariantR     = 0.90, kVariantG   = 0.65, kVariantB   = 0.15;
// UX-07: below this cell size (px) corner badges and branch counts are hidden
// (22x22 boards have small cells; the badges would just be noise).
static constexpr double kMinBadgeCell = 20.0;
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

// ═════════════════════════════════════════════════════════════════════════════
BoardRenderer::BoardRenderer(const BoardViewModel &viewModel)
    : vm_(viewModel)
{
}

BoardRenderer::Geometry BoardRenderer::computeGeometry(int width, int height) const
{
    Geometry g;
    int bs = vm_.boardSize;
    if (bs <= 0) return g;

    double usableW = width  - 2.0 * kCoordMargin;
    double usableH = height - 2.0 * kCoordMargin;
    g.cellSize   = std::min(usableW, usableH) / bs;
    g.boardPx    = static_cast<int>(g.cellSize * bs);
    g.marginLeft = (width  - g.boardPx) / 2.0;
    g.marginTop  = (height - g.boardPx) / 2.0;
    return g;
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
    Geometry geo = computeGeometry(width, height);
    cellSize_   = geo.cellSize;
    boardPx_    = geo.boardPx;
    marginLeft_ = geo.marginLeft;
    marginTop_  = geo.marginTop;

    // ── Layer pipeline ──────────────────────────────────────────────────────
    // UI-17: every layer runs inside its own save()/restore() so font face,
    // font size, line width and source colour set by one layer (e.g. the BOLD
    // "X" of drawForbiddenPoints) can never leak into the next one.
    // The base face is the one the text layers used to inherit from the
    // coordinate labels (sans-serif, normal), now set once so it survives restore().
    cr->select_font_face("sans-serif", Cairo::ToyFontFace::Slant::NORMAL,
                         Cairo::ToyFontFace::Weight::NORMAL);
    auto layer = [&](void (BoardRenderer::*draw)(const Cairo::RefPtr<Cairo::Context> &)) {
        cr->save();
        // save()/restore() does NOT cover the current path: a text layer
        // (show_text) leaves a current point, and the next layer's first
        // arc() would then draw a stray connecting line from it.
        cr->begin_new_path();
        (this->*draw)(cr);
        cr->restore();
    };
    layer(&BoardRenderer::drawGrid);
    layer(&BoardRenderer::drawCrosshair);
    layer(&BoardRenderer::drawStones);
    layer(&BoardRenderer::drawLastMove);
    // UX-07 order: variant -> database -> engine overlay (incl. best ring) ->
    // lost/forbidden -> PV ghost -> hover, so the lower-priority marks stay
    // visible under (or beside) the engine's.
    layer(&BoardRenderer::drawVariantMarkers);
    layer(&BoardRenderer::drawDatabaseMarkers);
    layer(&BoardRenderer::drawSearchOverlay);
    layer(&BoardRenderer::drawForbiddenPoints);
    layer(&BoardRenderer::drawPVHighlight);
    layer(&BoardRenderer::drawHover);
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
        cr->set_source_rgb(coordR_, coordG_, coordB_);
        cr->select_font_face("sans-serif", Cairo::ToyFontFace::Slant::NORMAL,
                             Cairo::ToyFontFace::Weight::NORMAL);
        // UX-04 (confirmed defect): with only a floor and no ceiling, this
        // scaled unboundedly with cellSize_ -- on a 5x5 board (large cells)
        // it grew past the fixed kCoordMargin reserve and the labels were
        // visibly clipped by the menu bar / left edge of the drawing area.
        // Cap it so the label always fits the fixed-size margin regardless
        // of board size.
        cr->set_font_size(std::clamp(cellSize_ * 0.35, 9.0, 16.0));

        // UX-08: the hovered cell's column letter / row number are emphasised
        // (bold + accent colour). Only for a valid empty cell, like the crosshair.
        const Coord hov = vm_.hoverMove;
        const bool hot  = hov.isValid(bs) && !vm_.isOccupied(hov);
        auto setLabelStyle = [&](bool emphasised) {
            if (emphasised) {
                cr->set_source_rgb(kLabelHotR, kLabelHotG, kLabelHotB);
                cr->select_font_face("sans-serif", Cairo::ToyFontFace::Slant::NORMAL,
                                     Cairo::ToyFontFace::Weight::BOLD);
            } else {
                cr->set_source_rgb(coordR_, coordG_, coordB_);
                cr->select_font_face("sans-serif", Cairo::ToyFontFace::Slant::NORMAL,
                                     Cairo::ToyFontFace::Weight::NORMAL);
            }
        };

        Cairo::TextExtents ext;
        for (int i = 0; i < bs; ++i) {
            // Column labels (A-O) at top.
            char col = 'A' + i;
            std::string label(1, col);
            setLabelStyle(hot && hov.x == i);
            cr->get_text_extents(label, ext);
            cr->move_to(cellCenterX(i) - ext.width / 2.0, marginTop_ - 6.0);
            cr->show_text(label);

            // Row labels (15..1 top to bottom) at left.
            std::string row = std::to_string(bs - i);
            setLabelStyle(hot && hov.y == i);
            cr->get_text_extents(row, ext);
            cr->move_to(marginLeft_ - ext.width - 6.0, cellCenterY(i) + ext.height / 2.0);
            cr->show_text(row);
        }
    }
}

// ── 1b. CrosshairLayer (UX-08) ───────────────────────────────────────────────
void BoardRenderer::drawCrosshair(const Cairo::RefPtr<Cairo::Context> &cr)
{
    const Coord h = vm_.hoverMove;
    if (!h.isValid(vm_.boardSize)) return;
    // Occupied cells follow the UI-16 rule (no hover feedback on a real stone):
    // no crosshair there either.
    if (vm_.isOccupied(h)) return;

    const double x0 = cellCenterX(0), x1 = cellCenterX(vm_.boardSize - 1);
    const double y0 = cellCenterY(0), y1 = cellCenterY(vm_.boardSize - 1);
    cr->set_source_rgba(kCrossR, kCrossG, kCrossB, kCrossAlpha);
    cr->set_line_width(std::max(2.0, cellSize_ * 0.12));
    cr->move_to(x0, cellCenterY(h.y));
    cr->line_to(x1, cellCenterY(h.y));
    cr->move_to(cellCenterX(h.x), y0);
    cr->line_to(cellCenterX(h.x), y1);
    cr->stroke();
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
            // UX-07: O(1) lookup from the view model's per-update index map.
            const int moveIndex = vm_.moveNumberAt(pos);
            if (moveIndex > 0) {
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
                
                // Bold digits read better on a stone than the thin default face.
                cr->select_font_face("sans-serif", Cairo::ToyFontFace::Slant::NORMAL, Cairo::ToyFontFace::Weight::BOLD);
                cr->set_font_size(std::max(8.0, cellSize_ * 0.45));
                Cairo::TextExtents ext;
                cr->get_text_extents(num, ext);

                // UX-04 (confirmed defect): on a 22x22 board with small cells,
                // 3-digit move numbers (100+) measured visibly wider than the
                // stone -- the fixed cellSize_*0.45 formula has no ceiling
                // relative to how many digits it must fit. Shrink the font so
                // the label stays inside the stone's diameter regardless of
                // digit count, instead of adding a general font-scaling
                // system (out of this fix's scope).
                double maxTextWidth = 2.0 * r * 0.85;
                if (ext.width > maxTextWidth && ext.width > 0.0) {
                    double scale = maxTextWidth / ext.width;
                    cr->set_font_size(std::max(6.0, cellSize_ * 0.45 * scale));
                    cr->get_text_extents(num, ext);
                }

                // Centre the ink box: x/y_bearing are the offset of the glyph
                // bounding box from the origin, so ignoring them shifted the
                // number off-centre (digit-dependent).
                cr->move_to(cx - ext.width / 2.0 - ext.x_bearing,
                            cy - ext.height / 2.0 - ext.y_bearing);
                cr->show_text(num);
                cr->begin_new_path();
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

// ── 3b. ForbiddenPointLayer (UI-03) ──────────────────────────────────────────
// UI-03 / UX-03: indication only -- these points remain fully clickable (see
// GameState::makeMove, which never consults RenjuRule). The marker uses both
// a distinct shape (a ring with a diagonal cross through it, like a "no
// entry" sign) AND a text glyph, per UX-03's "don't rely on colour alone".
void BoardRenderer::drawForbiddenPoints(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (vm_.forbiddenPoints.empty()) return;

    static constexpr double kForbidR = 0.75, kForbidG = 0.10, kForbidB = 0.10;
    double r = stoneRadius() * 0.55;

    for (const auto &pos : vm_.forbiddenPoints) {
        if (!pos.isValid(vm_.boardSize)) continue;
        cr->begin_new_path();   // UX-07: see drawDatabaseMarkers
        double cx = cellCenterX(pos.x);
        double cy = cellCenterY(pos.y);

        // Ring (the "no entry" shape).
        cr->set_source_rgba(kForbidR, kForbidG, kForbidB, 0.85);
        cr->set_line_width(std::max(1.5, cellSize_ * 0.045));
        cr->arc(cx, cy, r, 0, 2 * M_PI);
        cr->stroke();

        // Diagonal cross through the ring.
        double dr = r * 0.75;
        cr->move_to(cx - dr, cy - dr);
        cr->line_to(cx + dr, cy + dr);
        cr->move_to(cx - dr, cy + dr);
        cr->line_to(cx + dr, cy - dr);
        cr->stroke();

        // "X" text glyph below the point -- UX-03: shape alone (the ring +
        // cross) already avoids colour-only meaning, but a text glyph makes
        // the "forbidden" meaning legible even at a glance/low zoom.
        cr->set_source_rgba(kForbidR, kForbidG, kForbidB, 0.9);
        cr->select_font_face("sans-serif", Cairo::ToyFontFace::Slant::NORMAL,
                             Cairo::ToyFontFace::Weight::BOLD);
        cr->set_font_size(std::max(7.0, cellSize_ * 0.24));
        Cairo::TextExtents ext;
        cr->get_text_extents("X", ext);
        cr->move_to(cx - ext.width / 2.0, cy + r + ext.height + 1.0);
        cr->show_text("X");
    }
}

// ── 4. DatabaseMarkerLayer ───────────────────────────────────────────────────
// UX-07: database entries are OUTLINED (stroke-only) diamonds so they read as
// "book knowledge" next to the engine's filled discs. The best entry gets a
// thicker outline. On a cell that also carries an engine mark the diamond
// shrinks to a corner badge (no label); below kMinBadgeCell the badge is
// dropped entirely (the engine mark wins).
bool BoardRenderer::hasEngineMark(Coord c) const
{
    for (const auto &m : vm_.searchOverlay)
        if (m.pos == c) return true;
    return false;
}

void BoardRenderer::drawDatabaseMarkers(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (vm_.databaseMarkers.empty()) return;

    const double r = stoneRadius() * 0.55;

    for (const auto &m : vm_.databaseMarkers) {
        if (!m.pos.isValid(vm_.boardSize)) continue;
        double cx = cellCenterX(m.pos.x);
        double cy = cellCenterY(m.pos.y);

        // A previous marker's show_text leaves a current point; without this
        // the next path would start with a stray line from the old label.
        cr->begin_new_path();
        const bool badge = hasEngineMark(m.pos);
        if (badge && cellSize_ < kMinBadgeCell) continue;

        double dr = r;
        if (badge) {
            dr = cellSize_ * 0.14;
            cx += cellSize_ * 0.30;
            cy -= cellSize_ * 0.30;
        }

        // Heat-coloured outline (HSV ramp unchanged).
        if (m.eval >= 0) {
            set_source_from_winrate(cr, m.eval, 0.95);
        } else {
            cr->set_source_rgba(kDatabaseR, kDatabaseG, kDatabaseB, 0.95);
        }
        const double thin  = std::max(1.5, cellSize_ * 0.05);
        const double thick = std::max(2.5, cellSize_ * 0.08);
        cr->set_line_width(m.isBest ? thick : thin);
        cr->move_to(cx, cy - dr);
        cr->line_to(cx + dr, cy);
        cr->line_to(cx, cy + dr);
        cr->line_to(cx - dr, cy);
        cr->close_path();
        cr->stroke();

        // Label text (eval text only; no bound/comment glyphs). UX-03: white
        // text with a dark shadow stays legible across the heat hue range and
        // now also over the unfilled diamond / bare wood. Not drawn on badges.
        if (!badge && !m.label.empty()) {
            cr->set_font_size(std::max(8.0, cellSize_ * 0.28));
            Cairo::TextExtents ext;
            cr->get_text_extents(m.label, ext);
            double tx = cx - ext.width / 2.0;
            double ty = cy + ext.height / 2.0;

            cr->set_source_rgba(0.0, 0.0, 0.0, 0.6);
            cr->move_to(tx + 1, ty + 1);
            cr->show_text(m.label);

            cr->set_source_rgba(1.0, 1.0, 1.0, 0.95);
            cr->move_to(tx, ty);
            cr->show_text(m.label);
        }
    }
}

// ── 5. VariantMarkerLayer ────────────────────────────────────────────────────
// UX-07: ring + centre dot (the dot alone was easy to lose under other
// marks). The ring is larger than the engine tag disc / best ring so it stays
// visible around them. With > 1 branches a count badge sits at the bottom-right
// corner (hidden below kMinBadgeCell).
void BoardRenderer::drawVariantMarkers(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (vm_.variantMarkers.empty()) return;

    const double rRing = stoneRadius() * 0.80;
    const double rDot  = stoneRadius() * 0.22;

    for (const auto &m : vm_.variantMarkers) {
        if (!m.pos.isValid(vm_.boardSize)) continue;
        double cx = cellCenterX(m.pos.x);
        double cy = cellCenterY(m.pos.y);

        cr->begin_new_path();   // see drawDatabaseMarkers
        cr->set_source_rgba(kVariantR, kVariantG, kVariantB, 0.9);
        cr->set_line_width(std::max(1.5, cellSize_ * 0.06));
        cr->arc(cx, cy, rRing, 0, 2 * M_PI);
        cr->stroke();
        cr->arc(cx, cy, rDot, 0, 2 * M_PI);
        cr->fill();

        if (m.branchCount > 1 && cellSize_ >= kMinBadgeCell) {
            const double bx = cx + cellSize_ * 0.30;
            const double by = cy + cellSize_ * 0.30;
            const double br = cellSize_ * 0.17;
            cr->set_source_rgba(kVariantR, kVariantG, kVariantB, 1.0);
            cr->arc(bx, by, br, 0, 2 * M_PI);
            cr->fill();

            const std::string num = std::to_string(m.branchCount);
            cr->set_font_size(std::max(7.0, cellSize_ * 0.24));
            Cairo::TextExtents ext;
            cr->get_text_extents(num, ext);
            cr->set_source_rgba(0.10, 0.07, 0.0, 1.0);
            cr->move_to(bx - ext.width / 2.0 - ext.x_bearing, by + ext.height / 2.0);
            cr->show_text(num);
        }
    }
}

// ── 6. SearchOverlayLayer (PROTO-06 — live per-cell search feedback) ─────────
// One mark per empty cell, already resolved single-winner by
// BoardViewModel::update() (priority tag > lost > best > examined > examining).
// The winrate tag keeps the HSV heat colour (hue red→cyan by win%, like the
// reference winrate2colorstr); the other marks use fixed, shape-distinct
// glyphs so they read without relying on colour alone.
void BoardRenderer::drawSearchOverlay(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (vm_.searchOverlay.empty()) return;

    using Kind = BoardViewModel::SearchOverlayMark::Kind;
    const double rMark = stoneRadius() * 0.42;
    const double labelFont = std::max(8.0, cellSize_ * 0.36);
    cr->set_font_size(labelFont);

    // Sabaki heatmap (Shudan `.shudan-heat_N`): each analysed move is a soft,
    // blurred blob -- not a hard disc -- whose colour/size/opacity come from a
    // strength 1..9 (red < purple < blue < green; 9 = best, widest glow).
    // Sabaki derives strength from visits*winrate relative to the best move;
    // we have no visits here, so it is the winrate gap to the best tag.
    struct Heat { double r, g, b, spread, blur, alpha; };
    static const Heat kHeat[9] = {
        {0.941, 0.137, 0.067, 0.40, 0.75, 0.7},   // 1 #F02311
        {0.941, 0.137, 0.067, 0.40, 0.75, 0.8},   // 2
        {0.573, 0.153, 0.561, 0.45, 0.80, 0.7},   // 3 #92278F
        {0.573, 0.153, 0.561, 0.50, 0.85, 0.8},   // 4
        {0.282, 0.525, 0.835, 0.55, 0.90, 0.7},   // 5 #4886D5
        {0.282, 0.525, 0.835, 0.60, 1.00, 0.8},   // 6
        {0.282, 0.525, 0.835, 0.75, 1.00, 0.8},   // 7
        {0.349, 0.659, 0.059, 0.90, 1.00, 0.7},   // 8 #59A80F
        {0.349, 0.659, 0.059, 1.00, 1.00, 0.8},   // 9
    };
    double bestWinrate = 0.0;
    for (const auto &m : vm_.searchOverlay)
        if (m.kind == Kind::Tag) bestWinrate = std::max(bestWinrate, m.winrate);
    // Steeper than Sabaki's ratio: one strength step per 4 winrate points below
    // the best tag (7 pts -> blue, 20 pts -> purple, 32+ pts -> red).
    constexpr double kPointsPerStep = 0.04;
    auto strengthOf = [&](const BoardViewModel::SearchOverlayMark &m) {
        if (m.isBest) return 9;
        const int steps = static_cast<int>(std::lround((bestWinrate - m.winrate) / kPointsPerStep));
        return std::clamp(9 - steps, 1, 9);
    };
    // Sabaki's blobs reach ~2 cells across; gomoku tags sit on adjacent cells,
    // so the whole glow is scaled down to stay readable.
    constexpr double kGlowScale = 0.6;
    auto drawHeat = [&](double gx, double gy, int strength) {
        const Heat &h = kHeat[strength - 1];
        // box-shadow `0 0 blur spread`: solid out to spread - blur/2, gone at spread + blur/2.
        const double rIn  = std::max(0.0, h.spread - h.blur / 2.0) * kGlowScale * cellSize_;
        const double rOut = (h.spread + h.blur / 2.0) * kGlowScale * cellSize_;
        auto g = Cairo::RadialGradient::create(gx, gy, rIn, gx, gy, rOut);
        g->add_color_stop_rgba(0.0, h.r, h.g, h.b, h.alpha);
        g->add_color_stop_rgba(1.0, h.r, h.g, h.b, 0.0);
        cr->set_source(g);
        cr->arc(gx, gy, rOut, 0, 2 * M_PI);
        cr->fill();
        cr->begin_new_path();
    };

    for (const auto &m : vm_.searchOverlay) {
        if (!m.pos.isValid(vm_.boardSize)) continue;
        cr->begin_new_path();   // see drawDatabaseMarkers (tag text -> next arc)
        double cx = cellCenterX(m.pos.x);
        double cy = cellCenterY(m.pos.y);

        switch (m.kind) {
        case Kind::Tag: {
            drawHeat(cx, cy, strengthOf(m));
            if (!m.label.empty()) {
                // Sabaki label: bold, white, centred, soft dark shadow.
                cr->select_font_face("sans-serif", Cairo::ToyFontFace::Slant::NORMAL,
                                     Cairo::ToyFontFace::Weight::BOLD);
                Cairo::TextExtents ext;
                cr->get_text_extents(m.label, ext);
                const double tx = cx - ext.width / 2.0 - ext.x_bearing;
                const double ty = cy - ext.height / 2.0 - ext.y_bearing;
                cr->set_source_rgba(0.0, 0.0, 0.0, 0.55);
                cr->move_to(tx + 1, ty + 1);
                cr->show_text(m.label);
                cr->set_source_rgba(1.0, 1.0, 1.0, 0.95);
                cr->move_to(tx, ty);
                cr->show_text(m.label);
                cr->begin_new_path();
            }
            break;
        }
        case Kind::Lost: {
            // Losing root move — red ring + diagonal cross.
            cr->set_source_rgba(0.80, 0.12, 0.12, 0.85);
            cr->set_line_width(std::max(1.5, cellSize_ * 0.05));
            cr->arc(cx, cy, rMark, 0, 2 * M_PI);
            cr->stroke();
            double d = rMark * 0.72;
            cr->move_to(cx - d, cy - d);
            cr->line_to(cx + d, cy + d);
            cr->move_to(cx - d, cy + d);
            cr->line_to(cx + d, cy - d);
            cr->stroke();
            break;
        }
        case Kind::Best: {
            // Current best root move — filled cyan disc + ring.
            cr->set_source_rgba(0.15, 0.75, 0.85, 0.45);
            cr->arc(cx, cy, rMark, 0, 2 * M_PI);
            cr->fill();
            cr->set_source_rgba(0.10, 0.55, 0.70, 0.95);
            cr->set_line_width(std::max(1.5, cellSize_ * 0.05));
            cr->arc(cx, cy, rMark, 0, 2 * M_PI);
            cr->stroke();
            break;
        }
        case Kind::Examined: {
            cr->set_source_rgba(0.55, 0.55, 0.55, 0.35);
            cr->arc(cx, cy, rMark * 0.5, 0, 2 * M_PI);
            cr->fill();
            break;
        }
        case Kind::Examining: {
            cr->set_source_rgba(0.95, 0.85, 0.30, 0.55);
            cr->arc(cx, cy, rMark * 0.6, 0, 2 * M_PI);
            cr->fill();
            break;
        }
        }

        // UI-18: a best move that also carries a non-Tag mark (lost/examined/
        // examining) still gets the glow; Kind::Tag drew its own above and
        // Kind::Best is its own cyan disc.
        if (m.isBest && m.kind != Kind::Best && m.kind != Kind::Tag)
            drawHeat(cx, cy, 9);
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
        if (vm_.isOccupied(pos)) continue;   // UI-16: never paint a ghost over a real stone

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
    if (vm_.isOccupied(vm_.hoverMove)) return;   // UI-16: no hover stone over a real stone

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
