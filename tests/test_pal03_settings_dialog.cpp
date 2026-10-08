// PAL-03 — widget-level: every settings_registry row is built by SettingsDialog,
// and showSetting() jumps to the right notebook tab and focuses the control.
// Links gtkmm; self-skips with no display server.

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "model/game_state.h"
#include "ui/settings_dialog.h"
#include "ui/settings_registry.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

bool gtkReady() { return gtk_init_check(); }

GtkNotebook *findNotebook(GtkWidget *w)
{
    if (!w) return nullptr;
    if (GTK_IS_NOTEBOOK(w)) return GTK_NOTEBOOK(w);
    for (GtkWidget *c = gtk_widget_get_first_child(w); c; c = gtk_widget_get_next_sibling(c))
        if (auto *n = findNotebook(c)) return n;
    return nullptr;
}

}  // namespace

TEST_CASE("PAL-03 dialog builds exactly the registry's rows")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Gtk::Window parent;
    SettingsDialog dlg(parent, EngineConfig{}, ViewConfig{});

    std::vector<std::string> expected;
    for (const auto &e : settings_registry::kSettings)
        expected.emplace_back(e.id);
    std::sort(expected.begin(), expected.end());
    CHECK(dlg.registeredSettingIds() == expected);
}

TEST_CASE("PAL-03 showSetting selects the registry tab")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Gtk::Window parent;
    SettingsDialog dlg(parent, EngineConfig{}, ViewConfig{});
    auto *nb = findNotebook(GTK_WIDGET(dlg.gobj()));
    REQUIRE(nb != nullptr);

    for (const auto &e : settings_registry::kSettings) {
        CHECK_MESSAGE(dlg.showSetting(std::string(e.id)), e.id);
        CHECK_MESSAGE(gtk_notebook_get_current_page(nb) == e.tab, e.id);
    }
    gtk_notebook_set_current_page(nb, 0);
    CHECK_FALSE(dlg.showSetting("set.nonexistent"));
    CHECK(gtk_notebook_get_current_page(nb) == 0);
}
