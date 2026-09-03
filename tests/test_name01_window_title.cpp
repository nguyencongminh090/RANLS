// NAME-01: widget-level regression guard for the app-wide rename
// "Rapfi Analysis" -> RANLS.
//
// The window title is set in MainWindow's constructor (src/main_window.cpp:
// `set_title(kAppDisplayName)`). This asserts, against a REAL MainWindow
// widget, that the WM-visible title is "RANLS" and never the old
// "Rapfi Analysis" string. Per the Sprint 9 lesson, title/identity assertions
// belong in the gtkmm-linked ranls-gui-ui-tests target, not the model-only one.
//
// Links gtkmm; self-skips with no display server (main() lives in
// test_ui07_pv_view_rows.cpp and probes gtk_init_check()).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "main_window.h"

#include <string>

namespace {
bool gtkReady() { return gtk_init_check(); }
}  // namespace

TEST_CASE("NAME-01: the app display-name constant is RANLS, not \"Rapfi Analysis\"")
{
    CHECK(std::string(kAppDisplayName) == "RANLS");
    CHECK(std::string(kAppDisplayName) != "Rapfi Analysis");
}

TEST_CASE("NAME-01: a real MainWindow is titled \"RANLS\"")
{
    if (!gtkReady()) return;

    MainWindow window;

    CHECK(std::string(window.get_title()) == "RANLS");
    CHECK(std::string(window.get_title()) != "Rapfi Analysis");
}
