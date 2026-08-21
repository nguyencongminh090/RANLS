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

    // Computed during draw():
    double cellSize_   = 0.0;
    double marginLeft_ = 0.0;
    double marginTop_  = 0.0;
    int    boardPx_    = 0;
};
