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
        /// The GtkListBoxRow actually parented to `listBox_`. UI-07: PVView
        /// used to append the bare `row` Box and let GTK auto-wrap it in an
        /// implicit GtkListBoxRow — but then `listBox_.remove(*row)` was
        /// passed a *grandchild* of the list box, which GTK 4 rejects with
        /// "Tried to remove non-child" and silently ignores, so shrinking
        /// left the row on screen forever. Keep the wrapper explicitly so
        /// removal targets a real child.
        Gtk::ListBoxRow *listRow = nullptr;
        Gtk::Box   *row         = nullptr;
        Gtk::Label *idxLabel    = nullptr;
        Gtk::Label *scoreLabel  = nullptr;
        Gtk::Label *depthLabel  = nullptr;
        Gtk::Label *movesLabel  = nullptr;
        std::vector<Coord> moves; // kept in sync with the row's current content
    };

    Gtk::ListBox listBox_;
    std::vector<RowWidgets> rows_;

    /// UI-08: thin passthrough wrapper around listBox_ (UX-01 used it to show
    /// an idle-state placeholder message; that text was removed — an empty PV
    /// list now renders clean).
    EmptyStateOverlay overlay_{""};
};
