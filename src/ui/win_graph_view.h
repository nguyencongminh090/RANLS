#pragma once

#include "model/config.h"

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <vector>

/// Custom DrawingArea rendering a win-rate line graph.
/// X-axis = move number, Y-axis = win probability.
class WinGraphView : public Gtk::DrawingArea {
public:
    WinGraphView();

    /// Update the graph data.
    void setData(const std::vector<double> &blackSeries,
                 const std::vector<double> &whiteSeries,
                 int currentMoveIndex,
                 WinGraphMode mode);

    /// Signal emitted when the user clicks on the graph to jump to a move.
    sigc::signal<void(int)> signal_move_jumped;

private:
    void onDraw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height);

    std::vector<double> blackData_;
    std::vector<double> whiteData_;
    int                 currentIndex_ = -1;
    int                 hoverIndex_   = -1;
    WinGraphMode        mode_         = WinGraphMode::BothSide;
};
