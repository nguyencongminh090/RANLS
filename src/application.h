#pragma once

#include <gtkmm.h>

/// Top-level GTK Application.
/// Owns the CSS provider and creates the MainWindow.
class RapfiApplication : public Gtk::Application {
public:
    static Glib::RefPtr<RapfiApplication> create();

protected:
    RapfiApplication();

    void on_activate() override;

private:
    void loadStylesheet();
};
