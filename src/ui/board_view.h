#pragma once

#include "board_renderer.h"
#include "model/board_view_model.h"

#include <gtkmm.h>

/// GTK widget wrapping the board DrawingArea.
/// Handles mouse input and delegates rendering to BoardRenderer.
class BoardView : public Gtk::DrawingArea {
public:
    explicit BoardView(BoardViewModel &viewModel);

    /// Request a redraw of the board.
    void queueRedraw();

    /// Signal emitted when the user clicks a valid board coordinate.
    sigc::signal<void(Coord)> signal_move_clicked;

private:
    void onDraw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height);

    // ── Input ───────────────────────────────────────────────────────────────
    Coord pixelToCoord(double px, double py, int width, int height) const;

    BoardViewModel &vm_;
    BoardRenderer   renderer_;
};
