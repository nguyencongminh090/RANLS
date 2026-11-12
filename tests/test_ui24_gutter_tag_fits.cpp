// UI-24: the Engine Log gutter must show its longest tag ("MESSAGE") in full,
// with a visible gap before the log text.
//
// Root cause: drawGutter() sized the gutter with
// set_size_request(width("MESSAGE") + 12) and drew the tag at x = 6. In GTK4 a
// size request includes the widget's CSS padding + border, and `.engine-gutter`
// has `padding: 0 4px; border-right: 1px`, so the draw area was 9px narrower
// than requested (measured: request 64, content 55, tag 52 wide drawn at x=6
// -> ends at 58, clipped by 3px).
//
// The app's style.css is loaded here (from the source tree; the test binary
// does not link the GResource) because the bug only exists with that CSS.
//
// Links gtkmm; skips cleanly with no display server (see tests/CMakeLists.txt).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "ui/bottom_panel.h"

namespace {

bool gtkReady() { return gtk_init_check(); }

template <typename Pred>
GtkWidget *findWidget(GtkWidget *root, Pred pred)
{
    if (!root) return nullptr;
    if (pred(root)) return root;
    for (GtkWidget *c = gtk_widget_get_first_child(root); c; c = gtk_widget_get_next_sibling(c))
        if (GtkWidget *hit = findWidget(c, pred)) return hit;
    return nullptr;
}

void pump(int ms)
{
    for (int elapsed = 0; elapsed < ms; elapsed += 10) {
        while (g_main_context_iteration(nullptr, FALSE)) {
        }
        g_usleep(10 * 1000);
    }
    while (g_main_context_iteration(nullptr, FALSE)) {
    }
}

/// Installs the app stylesheet for the test's lifetime only, so other test
/// cases keep running against the default theme.
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

}  // namespace

TEST_CASE("UI-24: Engine Log gutter fits the longest tag plus a gap")
{
    if (!gtkReady()) return;
    AppCss css;

    auto window = Gtk::make_managed<Gtk::Window>();
    auto panel  = Gtk::make_managed<BottomPanel>();
    window->set_child(*panel);
    window->set_default_size(720, 260);
    window->set_visible(true);
    panel->set_current_page(1);  // Engine Log must be the visible page to be allocated
    pump(150);

    for (int i = 0; i < 5; ++i)
        panel->appendRecv("Message", Glib::ustring::compose("REALTIME LOST %1,%1", i));
    pump(300);  // flush tick + first gutter draw (sizes it) + relayout + redraw

    GtkWidget *gutter = findWidget(GTK_WIDGET(panel->gobj()), [](GtkWidget *w) {
        return GTK_IS_DRAWING_AREA(w) && gtk_widget_has_css_class(w, "engine-gutter");
    });
    GtkWidget *log = findWidget(GTK_WIDGET(panel->gobj()), [](GtkWidget *w) {
        return GTK_IS_TEXT_VIEW(w) && gtk_widget_has_css_class(w, "monospace");
    });
    REQUIRE(gutter != nullptr);
    REQUIRE(log != nullptr);

    // Width of the longest tag in the exact font the gutter paints with.
    PangoLayout *layout = gtk_widget_create_pango_layout(log, "MESSAGE");
    int tagW = 0, tagH = 0;
    pango_layout_get_pixel_size(layout, &tagW, &tagH);
    g_object_unref(layout);

    const int contentW = gtk_widget_get_width(gutter);  // draw area, excludes CSS padding/border
    constexpr int kTagX = 6, kMinGap = 4;
    MESSAGE("gutter content width " << contentW << ", tag width " << tagW);
    CHECK(tagW > 0);
    CHECK(contentW >= kTagX + tagW + kMinGap);
}
