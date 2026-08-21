// Custom main (instead of DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN) so we can call
// Gio::init() first.
//
// ENG-01 added engine_process.cpp/engine_controller.cpp (giomm/Gio::Subprocess)
// to this test target. Production code never needs an explicit init call
// because Gtk::Application::create() (src/application.cpp) performs it as a
// side effect. This test binary never touches gtkmm, so without an explicit
// Gio::init() here, glibmm's custom-GObject wrapper registration
// (Glib::wrap_register) never runs and every giomm object crossing a C GIO
// callback (e.g. the GUnixInputStream behind Gio::Subprocess's stdout/stderr
// pipes) fails to wrap with a GLib-GIO-CRITICAL and silently never completes
// its async read — which was previously masked because those tests didn't
// exist yet.
#define DOCTEST_CONFIG_IMPLEMENT
#include "vendor/doctest.h"

#include <giomm.h>

int main(int argc, char **argv)
{
    Gio::init();

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
