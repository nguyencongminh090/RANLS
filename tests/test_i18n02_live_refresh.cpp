// I18N-02 — widget-level: with the real bundled vi.tsv installed, a language
// switch (MainWindow::applyLanguage / i18n::setLanguage) re-texts the live
// window (hamburger menu model, header tooltips and rule chip, panel tabs,
// status label) and dialogs built afterwards (Settings, About); switching
// back to "en" restores the English text. Links gtkmm; self-skips with no
// display server (main() lives in test_ui07_pv_view_rows.cpp).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "i18n/i18n.h"
#include "main_window.h"
#include "ui/about_dialog.h"
#include "ui/settings_dialog.h"

#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Test-only accessor — MainWindow declares `friend struct RanlsI18n02Probe`.
struct RanlsI18n02Probe {
    MainWindow &w;
    GMenuModel *menu() { return G_MENU_MODEL(w.menuButton_.get_menu_model()->gobj()); }
    std::string ruleChip() { return w.ruleLabel_.get_text(); }
    std::string newTooltip() { return w.btnNew_->get_tooltip_text(); }
    std::string analyzeLabel() { return w.lblStart_->get_text(); }
};

namespace {

bool gtkReady() { return gtk_init_check(); }

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

void collectMenuLabels(GMenuModel *m, std::set<std::string> &out)
{
    const int n = g_menu_model_get_n_items(m);
    for (int i = 0; i < n; ++i) {
        if (GVariant *v = g_menu_model_get_item_attribute_value(m, i, "label", G_VARIANT_TYPE_STRING)) {
            out.insert(g_variant_get_string(v, nullptr));
            g_variant_unref(v);
        }
        for (const char *link : {"submenu", "section"})
            if (GMenuModel *sub = g_menu_model_get_item_link(m, i, link)) {
                collectMenuLabels(sub, out);
                g_object_unref(sub);
            }
    }
}

void collectLabels(GtkWidget *w, std::vector<std::string> &out)
{
    if (!w) return;
    if (GTK_IS_LABEL(w))
        if (const char *t = gtk_label_get_text(GTK_LABEL(w))) out.emplace_back(t);
    for (GtkWidget *c = gtk_widget_get_first_child(w); c; c = gtk_widget_get_next_sibling(c))
        collectLabels(c, out);
}

bool has(const std::vector<std::string> &v, const std::string &s)
{
    for (const auto &x : v)
        if (x == s) return true;
    return false;
}

}  // namespace

TEST_CASE("I18N-02 applyLanguage re-texts the live window and returns to English")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    LanguageGuard guard;
    installBundledLoader();

    MainWindow window;
    RanlsI18n02Probe probe{window};

    std::set<std::string> en;
    collectMenuLabels(probe.menu(), en);
    CHECK(en.count("Game"));
    CHECK(en.count("Settings…"));
    CHECK(en.count("Freestyle Gomoku"));  // rule names are never translated
    CHECK(probe.newTooltip() == "New game");
    CHECK(probe.analyzeLabel() == "Analyze");
    CHECK(probe.ruleChip() == "Rule: Freestyle Gomoku");

    CHECK_FALSE(window.applyLanguage("fr"));  // unsupported: nothing changes
    CHECK(probe.newTooltip() == "New game");

    REQUIRE(window.applyLanguage("vi"));
    std::set<std::string> vi;
    collectMenuLabels(probe.menu(), vi);
    CHECK(vi.count("Ván"));
    CHECK(vi.count("Cài đặt…"));
    CHECK_FALSE(vi.count("Game"));
    CHECK(vi.count("Freestyle Gomoku"));
    CHECK(vi.count("Free Renju"));
    CHECK(probe.newTooltip() == "Ván mới");
    CHECK(probe.analyzeLabel() == "Phân tích");
    CHECK(probe.ruleChip() == "Luật: Freestyle Gomoku");

    std::vector<std::string> labels;
    collectLabels(GTK_WIDGET(window.gobj()), labels);
    CHECK(has(labels, "Nhật ký nước đi"));  // bottom panel tab
    CHECK(has(labels, "● TẮT"));            // engine status
    CHECK(has(labels, "Sơ đồ (mọi nhánh)"));

    // A plain i18n::setLanguage (no MainWindow call) refreshes too.
    REQUIRE(i18n::setLanguage("en"));
    CHECK(probe.newTooltip() == "New game");
    CHECK(probe.ruleChip() == "Rule: Freestyle Gomoku");
    std::set<std::string> back;
    collectMenuLabels(probe.menu(), back);
    CHECK(back == en);
    labels.clear();
    collectLabels(GTK_WIDGET(window.gobj()), labels);
    CHECK(has(labels, "Move Log"));
    CHECK(has(labels, "● OFF"));
}

TEST_CASE("I18N-02 dialogs built after a switch use the new language")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    LanguageGuard guard;
    installBundledLoader();
    Gtk::Window parent;

    {
        SettingsDialog en(parent, EngineConfig{}, ViewConfig{});
        std::vector<std::string> labels;
        collectLabels(GTK_WIDGET(en.gobj()), labels);
        CHECK(has(labels, "Engine Path"));
        CHECK(has(labels, "Hotkeys"));
        CHECK(std::string(en.get_title()) == "Settings");
    }
    REQUIRE(i18n::setLanguage("vi"));
    {
        SettingsDialog vi(parent, EngineConfig{}, ViewConfig{});
        std::vector<std::string> labels;
        collectLabels(GTK_WIDGET(vi.gobj()), labels);
        CHECK(has(labels, "Đường dẫn engine"));
        CHECK(has(labels, "Phím tắt"));
        CHECK(has(labels, "Số luồng"));
        CHECK_FALSE(has(labels, "Engine Path"));
        CHECK(std::string(vi.get_title()) == "Cài đặt");
    }
    {
        AboutDialog about(parent);
        std::vector<std::string> labels;
        collectLabels(GTK_WIDGET(about.gobj()), labels);
        bool version = false;
        for (const auto &l : labels)
            if (l.rfind("Phiên bản ", 0) == 0) version = true;
        CHECK(version);
        CHECK(std::string(about.get_title()) == "Giới thiệu RANLS");
    }
}
