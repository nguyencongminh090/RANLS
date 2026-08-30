#pragma once

#include "model/board_view_model.h"

#include <cairomm/cairomm.h>

/// Renders the Gomoku board using layered Cairo drawing.
/// Reads only from BoardViewModel — never touches GameState directly.
class BoardRenderer {
public:
    explicit BoardRenderer(const BoardViewModel &viewModel);

    /// Draw the full board into the given Cairo context.
    /// @param cr      Cairo context.
    /// @param width   Available pixel width.
    /// @param height  Available pixel height.
    void draw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height);

    /// Layout geometry for a given widget size and the view model's current
    /// board size: cell size, board-area margins, and board pixel extent.
    struct Geometry {
        double cellSize   = 0.0;
        double marginLeft = 0.0;
        double marginTop  = 0.0;
        int    boardPx    = 0;
    };

    /// Compute the board layout geometry for (width, height). UX-04: this is
    /// the single source of truth for board geometry -- draw() and
    /// BoardView::pixelToCoord() both call this instead of each keeping an
    /// independent copy of the cell-size/margin formula (previously these
    /// diverged silently since board_view.cpp:6 duplicated
    /// board_renderer.cpp's kCoordMargin constant and formula).
    Geometry computeGeometry(int width, int height) const;

    /// UX-06: the coordinate labels (A–O / 1–N) are drawn in the margin
    /// *outside* the wood board, i.e. on the widget/app background — not on
    /// the wood. A fixed dark-brown (tuned for wood by UX-03) is invisible
    /// there under a dark theme. BoardView passes its themed foreground
    /// colour (get_color()) here each frame so the labels track the theme
    /// and stay legible in both light and dark.
    void setCoordinateColor(double r, double g, double b)
    {
        coordR_ = r;
        coordG_ = g;
        coordB_ = b;
    }

private:
    // ── Layer methods (called in order) ─────────────────────────────────────
    void drawGrid(const Cairo::RefPtr<Cairo::Context> &cr);
    void drawStones(const Cairo::RefPtr<Cairo::Context> &cr);
    void drawLastMove(const Cairo::RefPtr<Cairo::Context> &cr);
    void drawDatabaseMarkers(const Cairo::RefPtr<Cairo::Context> &cr);
    void drawVariantMarkers(const Cairo::RefPtr<Cairo::Context> &cr);
    void drawCandidateMoves(const Cairo::RefPtr<Cairo::Context> &cr);
    void drawPVHighlight(const Cairo::RefPtr<Cairo::Context> &cr);
    void drawHover(const Cairo::RefPtr<Cairo::Context> &cr);
    /// UI-03: draws vm_.forbiddenPoints (already computed by the model --
    /// RenjuRule, src/model/renju_rule.h). Indication only: a marked point is
    /// still fully clickable/playable, this layer never blocks input.
    void drawForbiddenPoints(const Cairo::RefPtr<Cairo::Context> &cr);

    // ── Coordinate helpers ──────────────────────────────────────────────────
    /// Convert board coordinate (x, y) to pixel center.
    double cellCenterX(int x) const;
    double cellCenterY(int y) const;

    /// Pixel radius for a stone.
    double stoneRadius() const;

    const BoardViewModel &vm_;

    // UX-06: coordinate-label colour, updated per-frame by BoardView from the
    // widget's themed foreground. Defaults to the old wood-tuned dark brown.
    double coordR_ = 0.20, coordG_ = 0.14, coordB_ = 0.08;

    // Computed during draw():
    double cellSize_   = 0.0;
    double marginLeft_ = 0.0;
    double marginTop_  = 0.0;
    int    boardPx_    = 0;
};
