// UI-11: widget-level regression guard for the custom About dialog.
//
// Asserts that AboutDialog (src/ui/about_dialog.*) — which replaced the stock
// Gtk::AboutDialog in MainWindow::onAbout() — builds a widget tree containing
// the app name "RANLS", the "Developer: Nguyen Minh" credit, and the version
// string sourced from APP_VERSION (version.h / CMake project(VERSION), REL-02).
//
// Links gtkmm; self-skips with no display server (see tests/CMakeLists.txt and
// the main() in test_ui07_pv_view_rows.cpp).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "ui/about_dialog.h"
#include "version.h"

#include <string>
#include <vector>

namespace {

bool gtkReady() { return gtk_init_check(); }

// Every GTK_IS_LABEL text in `root`'s subtree (visible or not — the dialog is
// never shown in this test).
std::vector<std::string> labelTexts(GtkWidget *root)
{
    std::vector<std::string> out;
    if (!root) return out;
    if (GTK_IS_LABEL(root)) {
        if (const char *t = gtk_label_get_text(GTK_LABEL(root)))
            out.emplace_back(t);
    }
    for (GtkWidget *c = gtk_widget_get_first_child(root); c;
         c = gtk_widget_get_next_sibling(c)) {
        auto sub = labelTexts(c);
        out.insert(out.end(), sub.begin(), sub.end());
    }
    return out;
}

bool anyContains(const std::vector<std::string> &hay, const std::string &needle)
{
    for (const auto &s : hay)
        if (s.find(needle) != std::string::npos) return true;
    return false;
}

}  // namespace

TEST_CASE("UI-11: AboutDialog shows name, developer credit and single-sourced version")
{
    if (!gtkReady()) return;

    Gtk::Window parent;
    AboutDialog dialog{parent};

    auto labels = labelTexts(GTK_WIDGET(dialog.gobj()));

    CHECK(anyContains(labels, "RANLS"));
    CHECK(anyContains(labels, "Developer: Nguyen Minh"));
    CHECK(anyContains(labels, "Nguyen Minh"));
    // Version must come from APP_VERSION, never a hard-coded literal (REL-02).
    CHECK(anyContains(labels, std::string(APP_VERSION)));
    // Old stock-dialog / wrong-name strings must be gone.
    CHECK_FALSE(anyContains(labels, "Rapfi Analysis"));

    // Tech/build-info and links blocks are present.
    CHECK(anyContains(labels, "GTK"));
    CHECK(anyContains(labels, "Build date"));
    CHECK(anyContains(labels, "github.com/nguyencongminh090/RANLS"));
    CHECK(anyContains(labels, "Rapfi, Yixin"));
}

TEST_CASE("UI-11: AboutDialog is modal and transient for its parent")
{
    if (!gtkReady()) return;

    Gtk::Window parent;
    AboutDialog dialog{parent};

    CHECK(dialog.get_modal());
    CHECK(dialog.get_transient_for() == &parent);
}

TEST_CASE("UI-11: AboutDialog builds under both the light and dark GTK theme")
{
    if (!gtkReady()) return;

    auto settings = Gtk::Settings::get_default();
    REQUIRE(settings);
    const Glib::ustring saved = settings->property_gtk_theme_name();

    for (const char *theme : {"Adwaita", "Adwaita-dark"}) {
        settings->property_gtk_theme_name() = theme;
        Gtk::Window parent;
        AboutDialog dialog{parent};             // must not crash / assert
        auto labels = labelTexts(GTK_WIDGET(dialog.gobj()));
        CHECK(anyContains(labels, "RANLS"));
    }

    settings->property_gtk_theme_name() = saved;
}
