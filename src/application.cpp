#include "application.h"
#include "main_window.h"

#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
RapfiApplication::RapfiApplication()
    : Gtk::Application("com.ranls.gui", Gio::Application::Flags::DEFAULT_FLAGS)
{
}

Glib::RefPtr<RapfiApplication> RapfiApplication::create()
{
    return Glib::make_refptr_for_instance<RapfiApplication>(new RapfiApplication());
}

// ─────────────────────────────────────────────────────────────────────────────
void RapfiApplication::on_activate()
{
    loadStylesheet();

    auto *window = new MainWindow();
    add_window(*window);
    window->present();
}

// ─────────────────────────────────────────────────────────────────────────────
void RapfiApplication::loadStylesheet()
{
    auto cssProvider = Gtk::CssProvider::create();

    // style.css is bundled into the binary as a GResource (see
    // src/resources/ranls.gresource.xml + CMakeLists.txt). This is independent
    // of the launch directory and cannot bake a build-host path into the binary.
    try {
        cssProvider->load_from_resource("/org/ranls/style.css");
        Gtk::StyleContext::add_provider_for_display(
            Gdk::Display::get_default(),
            cssProvider,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    catch (const Glib::Error &e) {
        std::cerr << "[RANLS] Warning: failed to load bundled style.css: "
                  << e.what() << "\n";
    }
}
