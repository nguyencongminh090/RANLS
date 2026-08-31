#pragma once

#include <gtkmm.h>

/// UI-11: custom "About" dialog for RANLS.
///
/// Replaces the stock `Gtk::AboutDialog` used by `MainWindow::onAbout()` with a
/// deliberately laid-out, two-pane window: app icon on the left, an information
/// column (name + version, developer credit, tech/build info, links & protocol)
/// on the right, and a single bottom-right "Close" button.
///
/// Self-contained and purely static app metadata — it has no dependency on
/// `GameState` or the engine. The version string comes from `APP_VERSION`
/// (version.h, single-sourced from CMake `project(VERSION)` — REL-02); build
/// date and git commit come from build_info.h (UI-11).
///
/// Lifetime: constructed on the heap by `onAbout()` with the CLEAN-01
/// delete-on-hide convention (`set_hide_on_close(true)` + `signal_hide`).
class AboutDialog : public Gtk::Window {
public:
    explicit AboutDialog(Gtk::Window &parent);
};
