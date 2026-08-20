#pragma once

#include "model/game_state.h"

#include <gtkmm.h>

/// Scrollable list of Principal Variation lines.
class PVView : public Gtk::ScrolledWindow {
public:
    PVView();

    /// Update PV lines display.
    void update(const std::vector<PVLine> &pvLines, int boardSize);

    /// Signal emitted when the user hovers a PV line (sends the move path).
    sigc::signal<void(std::vector<Coord>)> signal_pv_hovered;

    /// Signal emitted when the hover leaves all PV rows.
    sigc::signal<void()> signal_pv_hover_left;

private:
    Gtk::ListBox listBox_;
};
