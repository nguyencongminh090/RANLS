// Widget-level regression guard for ANLZ-01: the "Analyze Mode" toggle.
//
// Asserts, against a REAL MainWindow, that:
//  - the "analyze-mode" action exists and is a checkable (boolean-stateful)
//    action, like "engine-plays" is a radio-string one;
//  - activating it toggles its state, and the state round-trips (turning it
//    on then off leaves it off) — this exercises onToggleAnalyzeMode() ->
//    ViewConfig write -> syncAnalyzeModeMenu() -> action state, the same
//    two-way sync pattern as syncEnginePlaysMenu().
//
// Links gtkmm; self-skips with no display server (main() lives in
// test_ui07_pv_view_rows.cpp and probes gtk_init_check()).

#include "vendor/doctest.h"

#include <gtkmm.h>
#include <giomm.h>

#include "main_window.h"
#include "model/settings_storage.h"

#include <cstdio>

namespace {
bool gtkReady() { return gtk_init_check(); }
}  // namespace

TEST_CASE("ANLZ-01: the analyze-mode action exists and is a checkable boolean action")
{
    if (!gtkReady()) return;

    // Start from a clean settings file so the toggle's initial state is the
    // ViewConfig default (off), and clean up whatever the activate() writes.
    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    MainWindow window;

    auto action = std::dynamic_pointer_cast<Gio::SimpleAction>(
        window.lookup_action("analyze-mode"));
    REQUIRE(action);

    // Checkable => has boolean state.
    auto stateType = action->get_state_type();
    REQUIRE(stateType.gobj() != nullptr);
    CHECK(std::string(stateType.get_string()) == "b");

    bool state = true;
    action->get_state(state);
    CHECK(state == false);

    // Request the state change the way the menu checkbox does. This drives
    // signal_change_state -> onToggleAnalyzeMode(true) -> ViewConfig.analyzeMode
    // = true -> syncAnalyzeModeMenu() -> action state = true.
    action->change_state(Glib::Variant<bool>::create(true));
    action->get_state(state);
    CHECK(state == true);

    // ...and back off — the state round-trips.
    action->change_state(Glib::Variant<bool>::create(false));
    action->get_state(state);
    CHECK(state == false);

    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}
