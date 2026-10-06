// UX-08: BoardView query-tooltip wiring. Constructs a real BoardView inside a
// window and emits GTK's "query-tooltip" signal at widget-relative pixels, so
// the pixel -> cell -> BoardViewModel::tooltipFor chain is exercised end to
// end. Self-skips with no display (main() in test_ui07_pv_view_rows.cpp has
// already run gtk_init_check()).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "model/board_view_model.h"
#include "model/game_state.h"
#include "ui/board_view.h"

namespace {

bool gtkReady() { return gtk_init_check(); }

/// Emit query-tooltip on `w` at (x, y); returns the handler's result.
bool queryTooltip(Gtk::Widget &w, int x, int y) {
    GtkTooltip *tip = GTK_TOOLTIP(g_object_new(GTK_TYPE_TOOLTIP, nullptr));
    gboolean ret = FALSE;
    g_signal_emit_by_name(w.gobj(), "query-tooltip", x, y, FALSE, tip, &ret);
    g_object_unref(tip);
    return ret != FALSE;
}

} // namespace

TEST_CASE("UX-08: BoardView enables tooltips and reports has_tooltip") {
    if (!gtkReady()) return;
    GameState gs(15);
    BoardViewModel vm(gs);
    BoardView view(vm);
    CHECK(view.get_has_tooltip());
}

TEST_CASE("UX-08: BoardView query-tooltip returns true only over a cell with tooltip text") {
    if (!gtkReady()) return;
    GameState gs(15);
    DatabaseEntry e; e.pos = {3, 3}; e.value = 120; e.depth = 8; e.label = "w3";
    gs.makeMove({7, 7});
    gs.addDatabaseEntry(e);
    BoardViewModel vm(gs);
    vm.update();

    BoardView view(vm);
    Gtk::Window win;
    win.set_default_size(420, 420);
    win.set_child(view);
    win.present();
    auto ctx = Glib::MainContext::get_default();
    for (int i = 0; i < 200 && view.get_width() < 100; ++i)
        while (ctx->iteration(false)) {}
    if (view.get_width() < 100) return;   // compositor never mapped the window: skip

    BoardRenderer r(vm);
    auto g = r.computeGeometry(view.get_width(), view.get_height());
    auto at = [&](Coord c, int &x, int &y) {
        x = static_cast<int>(g.marginLeft + g.cellSize * (c.x + 0.5));
        y = static_cast<int>(g.marginTop + g.cellSize * (c.y + 0.5));
    };
    int x, y;
    at({3, 3}, x, y);
    CHECK(queryTooltip(view, x, y));      // database cell -> tooltip shown
    at({10, 10}, x, y);
    CHECK_FALSE(queryTooltip(view, x, y));  // plain empty cell
    at({7, 7}, x, y);
    CHECK_FALSE(queryTooltip(view, x, y));  // occupied cell
    CHECK_FALSE(queryTooltip(view, 1, 1));  // margin / off-board
}
