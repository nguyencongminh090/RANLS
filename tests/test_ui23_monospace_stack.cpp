// UI-23 follow-up: widgets with the `.monospace` class (Engine Log, console
// suggestion rows) must get the UI-23 monospace font stack from style.css.
//
// Root cause: a pre-UI-23 rule `.monospace { font-family: monospace; }` was
// left further down style.css. Same specificity, later in the file, so it
// overrode the new `.monospace, textview { font-family: "JetBrains Mono", ... }`
// rule: plain TextViews (Move Log) got the stack, `.monospace` widgets did not
// (measured: "monospace 12.267px" vs "JetBrains Mono,...,monospace 12.267px").
//
// Links gtkmm; skips cleanly with no display server (see tests/CMakeLists.txt).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include <string>

namespace {

bool gtkReady() { return gtk_init_check(); }

struct AppCss {
    Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
    AppCss()
    {
        provider->load_from_path(STYLE_CSS_PATH);
        Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(), provider,
                                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    ~AppCss() { Gtk::StyleContext::remove_provider_for_display(Gdk::Display::get_default(), provider); }
};

std::string family(Gtk::Widget &w)
{
    return w.get_pango_context()->get_font_description().get_family();
}

}  // namespace

TEST_CASE("UI-23: .monospace widgets and text views share the monospace font stack")
{
    if (!gtkReady()) return;
    AppCss css;

    Gtk::Window window;
    Gtk::Box box(Gtk::Orientation::VERTICAL);
    Gtk::TextView plainView;          // Move Log
    Gtk::TextView monoView;           // Engine Log
    monoView.add_css_class("monospace");
    Gtk::Label monoLabel("x");        // console suggestion rows
    monoLabel.add_css_class("monospace");
    box.append(plainView);
    box.append(monoView);
    box.append(monoLabel);
    window.set_child(box);

    for (Gtk::Widget *w : {static_cast<Gtk::Widget *>(&plainView), static_cast<Gtk::Widget *>(&monoView),
                           static_cast<Gtk::Widget *>(&monoLabel)}) {
        const std::string fam = family(*w);
        MESSAGE("resolved family: " << fam);
        CHECK(fam.find("JetBrains Mono") != std::string::npos);
    }
}
