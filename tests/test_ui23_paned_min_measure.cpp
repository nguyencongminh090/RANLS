// UI-23: the board | analysis Gtk::Paned must not log
//   "Trying to measure gtkmm__GtkBox ... for width of N, but it needs at least N+1"
//
// Root cause: UI-23 made the analysis panel height-for-width (the engine
// stats now wrap in a Gtk::FlowBox). When the window re-measures its content
// at its minimum width (on every PV update that changes the panel's natural
// width), GtkPaned (GTK 4.22) hands a height-for-width end child 1px less than
// its minimum *if the start child's minimum width is 0* -- and BoardView had
// no minimum size at all. The fix gives BoardView a real minimum size, which
// is also the UX intent (the board can never be squeezed to nothing).
//
// Links gtkmm; skips cleanly with no display server (see tests/CMakeLists.txt).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "gtk_log_capture.h"
#include "model/board_view_model.h"
#include "model/game_state.h"
#include "ui/analysis_panel.h"
#include "ui/board_geometry.h"
#include "ui/board_view.h"

namespace {
bool gtkReady() { return gtk_init_check(); }
}  // namespace

TEST_CASE("UI-23: BoardView has a non-zero minimum size")
{
    if (!gtkReady()) return;
    GameState gs(15);
    BoardViewModel vm(gs);
    BoardView board(vm);

    int minW = 0, natW = 0, minH = 0, natH = 0, b1 = 0, b2 = 0;
    board.measure(Gtk::Orientation::HORIZONTAL, -1, minW, natW, b1, b2);
    board.measure(Gtk::Orientation::VERTICAL, -1, minH, natH, b1, b2);
    // Room for the coordinate margins on both sides plus a usable grid.
    CHECK(minW > 2 * kCoordMargin);
    CHECK(minH > 2 * kCoordMargin);
}

TEST_CASE("UI-23: board|analysis paned measured at its minimum width logs no size warning")
{
    if (!gtkReady()) return;
    GameState gs(15);
    BoardViewModel vm(gs);

    Gtk::Window window;
    Gtk::Paned paned(Gtk::Orientation::HORIZONTAL);
    auto *board    = Gtk::make_managed<BoardView>(vm);
    auto *analysis = Gtk::make_managed<AnalysisPanel>(gs);
    // Same configuration as MainWindow's mainHPaned_.
    paned.set_start_child(*board);
    paned.set_end_child(*analysis);
    paned.set_resize_start_child(true);
    paned.set_shrink_start_child(false);
    paned.set_resize_end_child(true);
    paned.set_shrink_end_child(false);
    paned.set_position(640);
    window.set_child(paned);

    WarningCounter warnings;
    int minW = 0, natW = 0, minH = 0, natH = 0, b1 = 0, b2 = 0;
    paned.measure(Gtk::Orientation::HORIZONTAL, -1, minW, natW, b1, b2);
    REQUIRE(minW > 0);
    paned.measure(Gtk::Orientation::VERTICAL, minW, minH, natH, b1, b2);

    const auto bad = WarningCounter::containing("Trying to measure");
    for (const auto &m : bad) MESSAGE("warning: " << m);
    CHECK(bad.empty());
}
