// UX-09 — Settings "Apply" must stay available while the engine path is
// invalid: it applies every other setting and keeps the previously saved engine
// path (the invalid entry text is never stored). Links gtkmm; self-skips with
// no display server.

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "model/game_state.h"
#include "ui/settings_dialog.h"

#include <string>

// Test seam (declared a friend in settings_dialog.h): reaches the Apply button,
// the language dropdown and the engine-path entry without widget-tree walking.
struct RanlsUx09Probe {
    SettingsDialog &d;
    explicit RanlsUx09Probe(SettingsDialog &dialog) : d(dialog) {}
    bool applySensitive() { return d.btnApply_->get_sensitive(); }
    std::string applyTooltip() { return d.btnApply_->get_tooltip_text(); }
    void setEnginePath(const std::string &p) { d.entryEnginePath_.set_text(p); }
    void selectLanguage(guint i) { d.dropLanguage_.set_selected(i); }
    void clickApply() { g_signal_emit_by_name(d.btnApply_->gobj(), "clicked"); }
};

namespace {

bool gtkReady() { return gtk_init_check(); }

struct Applied {
    int count = 0;
    EngineConfig e;
    ViewConfig v;
};

void connect(SettingsDialog &dlg, Applied &out)
{
    dlg.signal_applied.connect([&out](EngineConfig e, ViewConfig v) {
        ++out.count;
        out.e = e;
        out.v = v;
    });
}

}  // namespace

TEST_CASE("UX-09 invalid saved engine path: Apply applies other settings, keeps the saved path")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Gtk::Window parent;
    EngineConfig ec;
    ec.enginePath = "/nonexistent/engine";
    ViewConfig vc;
    vc.language = "en";
    SettingsDialog dlg(parent, ec, vc);
    RanlsUx09Probe probe(dlg);
    Applied got;
    connect(dlg, got);

    CHECK(probe.applySensitive());
    CHECK_FALSE(probe.applyTooltip().empty());

    probe.selectLanguage(2);  // Tiếng Việt (0 = System, 1 = English)
    probe.clickApply();

    REQUIRE(got.count == 1);
    CHECK(got.v.language == "vi");
    CHECK(got.e.enginePath == "/nonexistent/engine");
}

TEST_CASE("UX-09 valid saved path, invalid entry text: base path kept, not the typed text")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Gtk::Window parent;
    EngineConfig ec;
    ec.enginePath = MOCK_ENGINE_PATH;
    ViewConfig vc;
    vc.language = "en";
    SettingsDialog dlg(parent, ec, vc);
    RanlsUx09Probe probe(dlg);
    Applied got;
    connect(dlg, got);

    CHECK(probe.applySensitive());
    CHECK(probe.applyTooltip().empty());  // valid: no explanation needed

    probe.setEnginePath("/definitely/not/an/engine");
    CHECK(probe.applySensitive());
    CHECK_FALSE(probe.applyTooltip().empty());

    probe.selectLanguage(2);
    probe.clickApply();

    REQUIRE(got.count == 1);
    CHECK(got.v.language == "vi");
    CHECK(got.e.enginePath == MOCK_ENGINE_PATH);
}

TEST_CASE("UX-09 valid typed path is applied and clears the tooltip")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Gtk::Window parent;
    EngineConfig ec;
    ec.enginePath = "/nonexistent/engine";
    SettingsDialog dlg(parent, ec, ViewConfig{});
    RanlsUx09Probe probe(dlg);
    Applied got;
    connect(dlg, got);

    CHECK_FALSE(probe.applyTooltip().empty());
    probe.setEnginePath(MOCK_ENGINE_PATH);
    CHECK(probe.applyTooltip().empty());
    probe.clickApply();
    REQUIRE(got.count == 1);
    CHECK(got.e.enginePath == MOCK_ENGINE_PATH);
}
