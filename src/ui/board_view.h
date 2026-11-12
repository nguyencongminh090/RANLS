#pragma once

#include "board_renderer.h"
#include "model/board_view_model.h"
#include "model/search_overlay_animator.h"

#include <gtkmm.h>

/// GTK widget wrapping the board DrawingArea.
/// Handles mouse input and delegates rendering to BoardRenderer.
class BoardView : public Gtk::DrawingArea {
public:
    explicit BoardView(BoardViewModel &viewModel);

    /// Request a redraw of the board. Call after every BoardViewModel::update():
    /// it is also where the eased search overlay picks up its new target.
    void queueRedraw();

    /// Signal emitted when the user clicks a valid board coordinate.
    sigc::signal<void(Coord)> signal_move_clicked;

private:
    void onDraw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height);

    // ── Input ───────────────────────────────────────────────────────────────
    Coord pixelToCoord(double px, double py, int width, int height) const;

    /// PROTO-09: frame-clock tick that advances overlayAnim_; runs only while
    /// a mark is still fading or a winrate is still counting.
    bool onAnimationTick(const Glib::RefPtr<Gdk::FrameClock> &clock);
    void stopAnimationTick();

    BoardViewModel &vm_;
    BoardRenderer   renderer_;

    SearchOverlayAnimator overlayAnim_;
    guint                 animTickId_    = 0;
    gint64                animLastFrame_ = 0;   ///< frame time (µs) of the previous tick, 0 = none
};
