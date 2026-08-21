#pragma once

#include "model/game_state.h"
#include "empty_state.h"

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
    /// Widgets for one PV row, kept alive across update() calls so its
    /// Gtk::EventControllerMotion (and any in-progress hover) survives a
    /// content refresh instead of being destroyed and recreated. See RT-03.
    struct RowWidgets {
        Gtk::Box   *row         = nullptr;
        Gtk::Label *idxLabel    = nullptr;
        Gtk::Label *scoreLabel  = nullptr;
        Gtk::Label *depthLabel  = nullptr;
        Gtk::Label *movesLabel  = nullptr;
        std::vector<Coord> moves; // kept in sync with the row's current content
    };

    Gtk::ListBox listBox_;
    std::vector<RowWidgets> rows_;

    /// UX-01: shows "No analysis yet…" over listBox_ while it has no rows,
    /// and hides it as soon as update() gets a non-empty pvLines (or after
    /// New Game clears back to empty via STATE-01).
    EmptyStateOverlay overlay_{"No analysis yet — press Analyze (F5) to see principal variations"};
};
