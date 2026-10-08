// I18N-03 — widget-level: the Settings dialog's Language dropdown (System /
// English / Tiếng Việt) feeds ViewConfig::language on Apply, the startup glue
// language_setting::applySetting() resolves "system" against a locale list, and the
// live window re-texts when the language is switched through it. Links gtkmm;
// self-skips with no display server (main() lives in test_ui07_pv_view_rows.cpp).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "i18n/i18n.h"
#include "main_window.h"
#include "ui/language_setting.h"
#include "ui/settings_dialog.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Test-only accessors (SettingsDialog / MainWindow declare these as friends).
struct RanlsI18n03Probe {
    SettingsDialog &d;
    explicit RanlsI18n03Probe(SettingsDialog &dialog) : d(dialog) {}
    guint selected() { return d.dropLanguage_.get_selected(); }
    void select(guint i) { d.dropLanguage_.set_selected(i); }
    std::string item(guint i)
    {
        auto list = std::dynamic_pointer_cast<Gtk::StringList>(d.dropLanguage_.get_model());
        return list ? std::string(list->get_string(i)) : std::string();
    }
    guint count() { return d.dropLanguage_.get_model()->get_n_items(); }
    void apply() { d.enginePathValid_ = true; d.onApply(); }
};

namespace {

bool gtkReady() { return gtk_init_check(); }

void collectLabels(GtkWidget *w, std::vector<std::string> &out)
{
    if (!w) return;
    if (GTK_IS_LABEL(w))
        if (const char *t = gtk_label_get_text(GTK_LABEL(w))) out.emplace_back(t);
    for (GtkWidget *c = gtk_widget_get_first_child(w); c; c = gtk_widget_get_next_sibling(c))
        collectLabels(c, out);
}

// Does any label in the window read `text`? (toolbar "Analyze" button label)
bool windowShows(MainWindow &window, const std::string &text)
{
    std::vector<std::string> labels;
    collectLabels(GTK_WIDGET(window.gobj()), labels);
    for (const auto &l : labels)
        if (l == text) return true;
    return false;
}

struct LanguageGuard {
    ~LanguageGuard()
    {
        i18n::setCatalogLoader(nullptr);
        i18n::setLanguage("en");
    }
};

void installBundledLoader()
{
    i18n::setCatalogLoader([](std::string_view code) {
        std::ifstream f(I18N_VI_PATH, std::ios::binary);
        std::stringstream ss;
        if (code == "vi") ss << f.rdbuf();
        return ss.str();
    });
}

}  // namespace

TEST_CASE("I18N-03 startup glue: system resolves vi_VN -> vi and fr_FR -> en; explicit codes win")
{
    LanguageGuard guard;
    installBundledLoader();

    CHECK(language_setting::applySetting("system", {"vi_VN.UTF-8", "vi_VN", "vi", "C"}) == "vi");
    CHECK(i18n::currentLanguage() == "vi");
    CHECK(i18n::tr("New Game") == "Ván mới");

    CHECK(language_setting::applySetting("system", {"fr_FR.UTF-8", "fr_FR", "fr", "C"}) == "en");
    CHECK(i18n::currentLanguage() == "en");
    CHECK(i18n::tr("New Game") == "New Game");

    CHECK(language_setting::applySetting("vi", {"fr_FR"}) == "vi");
    CHECK(language_setting::applySetting("bogus", {"vi_VN"}) == "vi");  // unknown acts as system
    CHECK(language_setting::applySetting("en", {"vi_VN"}) == "en");

    CHECK_FALSE(language_setting::systemLocaleNames().empty());  // GLib always yields at least "C"
}

TEST_CASE("I18N-03 dropdown choices, preselection, and Apply writes ViewConfig::language")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    LanguageGuard guard;
    Gtk::Window parent;

    ViewConfig view;
    view.language = "vi";
    SettingsDialog dlg(parent, EngineConfig{}, view);
    RanlsI18n03Probe probe(dlg);

    REQUIRE(probe.count() == 3);
    CHECK(probe.item(0) == "System");
    CHECK(probe.item(1) == "English");
    CHECK(probe.item(2) == "Tiếng Việt");
    CHECK(probe.selected() == 2);  // preselects the stored setting

    std::vector<std::string> applied;
    dlg.signal_applied.connect([&](EngineConfig, ViewConfig v) { applied.push_back(v.language); });

    probe.select(1);
    probe.apply();
    probe.select(2);
    probe.apply();
    probe.select(0);
    probe.apply();
    CHECK(applied == std::vector<std::string>{"en", "vi", "system"});

    ViewConfig unknown;
    unknown.language = "klingon";  // a corrupt stored value shows as System
    SettingsDialog dlg2(parent, EngineConfig{}, unknown);
    CHECK(RanlsI18n03Probe(dlg2).selected() == 0);
}

TEST_CASE("I18N-03 choosing Tiếng Việt in the dropdown switches the live UI; English switches back")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    LanguageGuard guard;
    installBundledLoader();

    MainWindow window;
    CHECK(windowShows(window, "Analyze"));
    CHECK_FALSE(windowShows(window, "Phân tích"));

    // The dialog's Apply output is what MainWindow::openSettings feeds to
    // language_setting::applySetting().
    Gtk::Window parent;
    SettingsDialog dlg(parent, EngineConfig{}, ViewConfig{});
    RanlsI18n03Probe probe(dlg);
    dlg.signal_applied.connect([&](EngineConfig, ViewConfig v) { language_setting::applySetting(v.language, {"C"}); });

    probe.select(2);  // Tiếng Việt
    probe.apply();
    CHECK(windowShows(window, "Phân tích"));
    CHECK_FALSE(windowShows(window, "Analyze"));

    probe.select(1);  // English
    probe.apply();
    CHECK(windowShows(window, "Analyze"));
    CHECK_FALSE(windowShows(window, "Phân tích"));

    // A Settings dialog opened afterwards is built in the active language.
    probe.select(2);
    probe.apply();
    SettingsDialog later(parent, EngineConfig{}, ViewConfig{});
    CHECK(RanlsI18n03Probe(later).item(0) == "Hệ thống");
}
