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

#include "i18n/i18n.h"
#include "ui/about_dialog.h"
#include "version.h"

#include <fstream>
#include <sstream>
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

// UI-22: the "Links & protocol" heading rendered blank (and GTK warned)
// because makeSection() built Pango markup from the raw title, so the bare
// '&' was parsed as an entity start. Every section heading must keep its
// visible text, in en and vi, with no markup-parse warning.
namespace {

// Captures GLib log output (GTK4 logs through the structured-logging writer,
// which g_log_set_default_handler does not see). The writer can be installed
// only once per process, so it stays installed and records while `active`.
struct WarningCounter {
    static inline std::vector<std::string> messages;
    static inline bool active = false;
    static inline bool installed = false;
    WarningCounter()
    {
        if (!installed) {
            installed = true;
            g_log_set_writer_func(
                [](GLogLevelFlags level, const GLogField *fields, gsize n, gpointer) {
                    if (active)
                        for (gsize i = 0; i < n; ++i)
                            if (std::string(fields[i].key) == "MESSAGE" && fields[i].value)
                                messages.emplace_back(static_cast<const char *>(fields[i].value));
                    return g_log_writer_default(level, fields, n, nullptr);
                },
                nullptr, nullptr);
        }
        messages.clear();
        active = true;
    }
    ~WarningCounter() { active = false; }
    // Only Pango/GTK markup problems are in scope; unrelated host noise
    // (gvfs, EGL) is ignored.
    static std::vector<std::string> markupErrors()
    {
        std::vector<std::string> out;
        for (const auto &m : messages)
            if (m.find("markup") != std::string::npos) out.push_back(m);
        return out;
    }
};

struct UiLanguageGuard {
    ~UiLanguageGuard()
    {
        i18n::setCatalogLoader(nullptr);
        i18n::setLanguage("en");
    }
};

}  // namespace

TEST_CASE("UI-22: every About section heading keeps its visible text (en and vi)")
{
    if (!gtkReady()) return;
    UiLanguageGuard guard;

    i18n::setCatalogLoader([](std::string_view code) {
        std::ifstream f(I18N_VI_PATH, std::ios::binary);
        std::stringstream ss;
        if (code == "vi") ss << f.rdbuf();
        return ss.str();
    });

    for (const std::string lang : {"en", "vi"}) {
        REQUIRE(i18n::setLanguage(lang));
        std::vector<std::string> expected;
        for (const char *k : {"Developer", "Tech / build info", "Links & protocol"})
            expected.push_back(i18n::tr(k));

        WarningCounter warnings;
        Gtk::Window parent;
        AboutDialog dialog{parent};
        auto labels = labelTexts(GTK_WIDGET(dialog.gobj()));

        for (const auto &h : expected) {
            INFO("lang=" << lang << " heading=" << h);
            bool found = false;
            for (const auto &l : labels) found = found || (l == h);
            CHECK(found);
        }
        for (const auto &m : WarningCounter::markupErrors()) MESSAGE("warning: " << m);
        CHECK(WarningCounter::markupErrors().empty());
    }
}

// I18N-05: the two markup=true value labels ("Repository: %s", "Engine
// protocol: %s") build Pango markup from a translated prefix. A prefix with a
// bare '&' or '<' must not blank the label, and the <a href> link must stay live.
namespace {

// First GtkLabel in `root`'s subtree whose text contains `needle`.
GtkLabel *findLabelContaining(GtkWidget *root, const std::string &needle)
{
    if (!root) return nullptr;
    if (GTK_IS_LABEL(root)) {
        const char *t = gtk_label_get_text(GTK_LABEL(root));
        if (t && std::string(t).find(needle) != std::string::npos) return GTK_LABEL(root);
    }
    for (GtkWidget *c = gtk_widget_get_first_child(root); c;
         c = gtk_widget_get_next_sibling(c))
        if (auto *l = findLabelContaining(c, needle)) return l;
    return nullptr;
}

}  // namespace

TEST_CASE("I18N-05: a translated link prefix with '&' and '<' still renders and keeps its link")
{
    if (!gtkReady()) return;
    UiLanguageGuard guard;

    i18n::setCatalogLoader([](std::string_view code) {
        return code == "vi" ? std::string("Repository: %s\tKho & <ma nguon>: %s\n"
                                          "Engine protocol: %s\tGiao thuc & <engine>: %s\n")
                            : std::string();
    });
    REQUIRE(i18n::setLanguage("vi"));

    WarningCounter warnings;
    Gtk::Window parent;
    AboutDialog dialog{parent};
    GtkWidget *root = GTK_WIDGET(dialog.gobj());

    struct Case { const char *prefix; const char *linkText; };
    for (const Case &c : {Case{"Kho & <ma nguon>: ", "github.com/nguyencongminh090/RANLS"},
                          Case{"Giao thuc & <engine>: ", "Gomocup / Yixin protocol"}}) {
        INFO("prefix=" << c.prefix);
        GtkLabel *lbl = findLabelContaining(root, c.linkText);
        REQUIRE(lbl != nullptr);
        CHECK(std::string(gtk_label_get_text(lbl)) == std::string(c.prefix) + c.linkText);
        CHECK(gtk_label_get_use_markup(lbl));
        // The anchor survived: the label carries a link.
        const char *markup = gtk_label_get_label(lbl);
        CHECK(std::string(markup ? markup : "").find("<a href=") != std::string::npos);
    }
    for (const auto &m : WarningCounter::markupErrors()) MESSAGE("warning: " << m);
    CHECK(WarningCounter::markupErrors().empty());
}
