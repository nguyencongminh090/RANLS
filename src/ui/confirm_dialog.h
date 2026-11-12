#pragma once

#include <gtkmm.h>

/// I18N-07: construction of the "discard the current game?" confirmation dialog,
/// split out of `MainWindow::confirmDiscardGame` so a test can inspect its buttons.
///
/// Returns a heap-allocated, modal, not-yet-shown dialog. The caller owns it
/// (connect `signal_response`, show it, `delete` it on response). Buttons are
/// labelled through `i18n::tr` (not GTK's stock YES_NO labels, which follow
/// GTK's own locale rather than the app's UI language). No default response is
/// set, so Enter never confirms a destructive action.
Gtk::MessageDialog *makeDiscardConfirmDialog(Gtk::Window &parent, const Glib::ustring &action);
