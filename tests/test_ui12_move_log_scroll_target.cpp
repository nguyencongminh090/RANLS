// UI-12: the Move Log must auto-scroll so the newest move stays visible.
//
// Root cause (identical to the UI-10 Engine Log bug): `BottomPanel` put the
// Move Log TextView inside an `EmptyStateOverlay` (a plain `Gtk::Overlay`, a
// no-op passthrough since UI-08) and made THAT the `Gtk::ScrolledWindow`'s
// child. `Gtk::Overlay` does not implement `Gtk::Scrollable`, so
// `Gtk::ScrolledWindow` silently wraps it in an implicit `GtkViewport`. The
// Viewport then does the scrolling while the TextView is laid out at full
// height inside it — so `moveLogView_.scroll_to(...)` in `scrollToEnd` moved
// nothing and every append left the newest move off the bottom.
//
// The fix makes the TextView the ScrolledWindow's DIRECT child (it implements
// GtkScrollable). These cases pin that: no GtkViewport between the Move Log's
// ScrolledWindow and its TextView; and N appended moves leave the view pinned
// to the bottom.
//
// The "don't yank a user who scrolled up" behaviour is deliberately out of
// scope for UI-12 (see docs/todo/UI-12-*.md) — the Move Log always follows the
// newest move — so there is no test case for it here.
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

/// The Move Log's ScrolledWindow is the one that does NOT share a row with the
/// drawn gutter (a GtkDrawingArea) — that sibling belongs to the Engine Log.
GtkWidget *moveLogScroller(GtkWidget *root)
{
    return findWidget(root, [](GtkWidget *w) {
        return GTK_IS_SCROLLED_WINDOW(w) && !hasSiblingDrawingArea(w);
    });
}

/// Spin the main loop for roughly `ms` so GTK settles layout/scroll. The Move
/// Log has no batch-flush timer (unlike the Engine Log) — this is purely to let
/// realize/allocate and the deferred scroll pass run.
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

TEST_CASE("UI-12: Move Log TextView is the ScrolledWindow's direct child")
{
    if (!gtkReady()) return;

    BottomPanel panel;
    GtkWidget  *root = GTK_WIDGET(panel.gobj());

    GtkWidget *moveScroller = moveLogScroller(root);
    REQUIRE(moveScroller != nullptr);

    GtkWidget *child =
        gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(moveScroller));
    REQUIRE(child != nullptr);

    // The regression: a GtkViewport (or the old GtkOverlay) here means
    // TextView::scroll_to can't move the visible area.
    CHECK_FALSE(GTK_IS_VIEWPORT(child));
    CHECK(GTK_IS_TEXT_VIEW(child));
}

TEST_CASE("UI-12: appended moves keep the Move Log scrolled to the bottom")
{
    if (!gtkReady()) return;

    auto window = Gtk::make_managed<Gtk::Window>();
    auto panel  = Gtk::make_managed<BottomPanel>();
    window->set_child(*panel);
    window->set_default_size(720, 260);
    window->set_visible(true);
    pump(200);  // realize + first allocation

    GtkWidget *sw = moveLogScroller(GTK_WIDGET(panel->gobj()));
    REQUIRE(sw != nullptr);
    GtkAdjustment *vadj =
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw));
    REQUIRE(vadj != nullptr);

    // Many moves, well past one viewport. Moves arrive one at a time with the
    // event loop running between them (a game is played move by move), so pump
    // GTK every few appends rather than dumping 400 inserts under one layout.
    for (int i = 0; i < 400; ++i) {
        panel->appendMoveLog(Glib::ustring::compose("%1. move on the board\n", i));
        if (i % 20 == 0)
            pump(10);
    }
    pump(300);  // let layout + the deferred scroll pass settle

    const double value    = gtk_adjustment_get_value(vadj);
    const double page     = gtk_adjustment_get_page_size(vadj);
    const double upper    = gtk_adjustment_get_upper(vadj);
    const double maxValue = upper - page;

    REQUIRE(maxValue > 10.0);              // content really does overflow now
    CHECK(value >= maxValue - 4.0);        // …and we are pinned to its bottom

    window->set_visible(false);
}
