// UI-10 (second pass): the Engine Log must actually follow the newest line
// while analysis streams in. The first UI-10 fix added a persistent
// end-of-buffer mark and `engineLogView_.scroll_to(mark, …)` on every flush,
// but the log still sat pinned to the FIRST line during analysis.
//
// Root cause: `BottomPanel` put the Engine Log TextView inside an
// `EmptyStateOverlay` (a plain `Gtk::Overlay`, a no-op passthrough since
// UI-08) and made THAT the `Gtk::ScrolledWindow`'s child. `Gtk::Overlay` does
// not implement `Gtk::Scrollable`, so `Gtk::ScrolledWindow` silently wraps it
// in an implicit `GtkViewport`. The Viewport then does the scrolling while the
// TextView is laid out at full height inside it — so `scroll_to` on the
// TextView moves nothing. Every auto-scroll was a no-op.
//
// The fix makes the TextView the ScrolledWindow's DIRECT child (it implements
// GtkScrollable). This case pins that: no GtkViewport between the Engine Log's
// ScrolledWindow and its TextView.
//
// Links gtkmm; skips cleanly with no display server (see tests/CMakeLists.txt).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "ui/bottom_panel.h"

namespace {

/// main() (in test_ui07_pv_view_rows.cpp) has already run gtk_init_check();
/// calling it again is cheap and true iff a display server is usable.
bool gtkReady() { return gtk_init_check(); }

template <typename Pred>
GtkWidget *findWidget(GtkWidget *root, Pred pred)
{
    if (!root) return nullptr;
    if (pred(root)) return root;
    for (GtkWidget *c = gtk_widget_get_first_child(root); c;
         c = gtk_widget_get_next_sibling(c)) {
        if (GtkWidget *hit = findWidget(c, pred)) return hit;
    }
    return nullptr;
}

bool hasSiblingDrawingArea(GtkWidget *w)
{
    GtkWidget *parent = gtk_widget_get_parent(w);
    if (!parent) return false;
    for (GtkWidget *c = gtk_widget_get_first_child(parent); c;
         c = gtk_widget_get_next_sibling(c)) {
        if (c != w && GTK_IS_DRAWING_AREA(c)) return true;
    }
    return false;
}

GtkWidget *engineLogScroller(GtkWidget *root)
{
    return findWidget(root, [](GtkWidget *w) {
        return GTK_IS_SCROLLED_WINDOW(w) && hasSiblingDrawingArea(w);
    });
}

/// Spin the main loop for roughly `ms` so the 50 ms batch-flush timer in
/// BottomPanel fires several times and GTK settles layout/scroll in between.
void pump(int ms)
{
    for (int elapsed = 0; elapsed < ms; elapsed += 10) {
        while (g_main_context_iteration(nullptr, /*may_block=*/FALSE)) {
        }
        g_usleep(10 * 1000);
    }
    while (g_main_context_iteration(nullptr, FALSE)) {
    }
}

}  // namespace

TEST_CASE("UI-10: Engine Log TextView is the ScrolledWindow's direct child")
{
    if (!gtkReady()) return;

    BottomPanel panel;
    GtkWidget  *root = GTK_WIDGET(panel.gobj());

    // The Engine Log's ScrolledWindow is the one sharing a row with the drawn
    // gutter (a GtkDrawingArea); the Move Log's ScrolledWindow stands alone.
    GtkWidget *engineScroller = engineLogScroller(root);
    REQUIRE(engineScroller != nullptr);

    GtkWidget *child =
        gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(engineScroller));
    REQUIRE(child != nullptr);

    // The regression: a GtkViewport (or the old GtkOverlay) here means
    // TextView::scroll_to can't move the visible area.
    CHECK_FALSE(GTK_IS_VIEWPORT(child));
    CHECK(GTK_IS_TEXT_VIEW(child));
}

TEST_CASE("UI-10: streamed lines keep the Engine Log scrolled to the bottom")
{
    if (!gtkReady()) return;

    auto window = Gtk::make_managed<Gtk::Window>();
    auto panel  = Gtk::make_managed<BottomPanel>();
    window->set_child(*panel);
    window->set_default_size(720, 260);
    window->set_visible(true);
    pump(200);  // realize + first allocation

    GtkWidget *sw = engineLogScroller(GTK_WIDGET(panel->gobj()));
    REQUIRE(sw != nullptr);
    GtkAdjustment *vadj =
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw));
    REQUIRE(vadj != nullptr);

    // Simulate an analysis stream: many lines, well past one viewport.
    for (int i = 0; i < 400; ++i)
        panel->appendSend(Glib::ustring::compose("depth %1 line of streamed output", i));
    pump(600);  // let the 50 ms flush timer drain the queue + auto-scroll

    const double value    = gtk_adjustment_get_value(vadj);
    const double page     = gtk_adjustment_get_page_size(vadj);
    const double upper    = gtk_adjustment_get_upper(vadj);
    const double maxValue = upper - page;

    REQUIRE(maxValue > 10.0);              // content really does overflow now
    CHECK(value >= maxValue - 4.0);        // …and we are pinned to its bottom

    window->set_visible(false);
}

TEST_CASE("UI-10: a user who scrolled up is not yanked back down by new lines")
{
    if (!gtkReady()) return;

    auto window = Gtk::make_managed<Gtk::Window>();
    auto panel  = Gtk::make_managed<BottomPanel>();
    window->set_child(*panel);
    window->set_default_size(720, 260);
    window->set_visible(true);
    pump(200);

    GtkWidget *sw = engineLogScroller(GTK_WIDGET(panel->gobj()));
    REQUIRE(sw != nullptr);
    GtkAdjustment *vadj =
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw));
    REQUIRE(vadj != nullptr);

    for (int i = 0; i < 200; ++i)
        panel->appendSend(Glib::ustring::compose("depth %1 first burst", i));
    pump(400);

    // User drags the view up to read history (simulate via the adjustment,
    // exactly what the scrollbar does — this fires value_changed unguarded).
    gtk_adjustment_set_value(vadj, 0.0);
    pump(120);
    const double parkedUpper = gtk_adjustment_get_upper(vadj);

    // More stream arrives.
    for (int i = 0; i < 200; ++i)
        panel->appendSend(Glib::ustring::compose("depth %1 second burst", i));
    pump(400);

    // The view must have stayed near where the user parked it, NOT snapped to
    // the (now much larger) bottom.
    const double value = gtk_adjustment_get_value(vadj);
    CHECK(value < parkedUpper);

    window->set_visible(false);
}
