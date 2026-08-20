#include "application.h"
#include "main_window.h"

#include <filesystem>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
RapfiApplication::RapfiApplication()
    : Gtk::Application("com.rapfi.analysis", Gio::Application::Flags::DEFAULT_FLAGS)
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

    // Try to find style.css next to the executable first, then in source tree.
    std::filesystem::path cssPath = "style.css";
    if (!std::filesystem::exists(cssPath)) {
        // Fallback: look relative to the source directory (dev mode).
        cssPath = std::filesystem::path(__FILE__).parent_path() / "resources" / "style.css";
    }

    if (std::filesystem::exists(cssPath)) {
        cssProvider->load_from_path(cssPath.string());
        Gtk::StyleContext::add_provider_for_display(
            Gdk::Display::get_default(),
            cssProvider,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    else {
        std::cerr << "[Rapfi GUI] Warning: style.css not found.\n";
    }
}
