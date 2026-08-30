// UI-07 (second pass): widget-level regression guard for the PV panel.
//
// The first UI-07 fix (60640c6) made AnalysisPanel's signal_board_changed
// handler call pvView_.update(gameState_.pvLines(), ...) on every position
// change. That is correct and necessary, but it did NOT fix the reported bug,
// because the defect lives one layer further down, inside PVView::update()'s
// own widget bookkeeping:
//
//   PVView appended each PV row as a bare Gtk::Box (`listBox_.append(*row)`),
//   which GTK auto-wraps in an implicitly-created GtkListBoxRow. The shrink
//   path then called `listBox_.remove(*rows_.back().row)` with that same
//   Gtk::Box — a *grandchild* of the list box, not a child. GTK 4 answers
//   that with `Gtk-WARNING: Tried to remove non-child` and removes nothing.
//   So `rows_` shrank while the row widget stayed on screen forever, and the
//   next analysis appended its row *below* the orphan.
//
// That is exactly the reported signature: row 1 = the previous position's
// line, row 2 = the current position's line, both labelled "PV #1" (they each
// came from commitPV(0), so both carry pvIndex == 1) — and ~12 stale rows
// after a dozen position changes, one orphan leaked per clear.
//
// Every existing UI-07 test asserts GameState::pvLines(), which was always
// genuinely correct; nothing asserted what the widget tree actually holds.
// These cases do, by driving the REAL PVView / AnalysisPanel widgets.
//
// This binary links gtkmm (unlike rapfi-gui-tests, which deliberately does
// not — see tests/CMakeLists.txt and docs/audit/2026-08-21-test-framework-choice.md).
// It skips itself cleanly when no display server is available.

#define DOCTEST_CONFIG_IMPLEMENT
#include "vendor/doctest.h"

#include <gtkmm.h>

#include "engine/gomocup_protocol.h"
#include "model/game_state.h"
#include "ui/analysis_panel.h"
#include "ui/pv_view.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool                            g_gtkReady = false;
Glib::RefPtr<Gtk::Application>  g_app;

/// First GtkListBox in `root`'s widget subtree (depth-first). PVView keeps its
/// Gtk::ListBox private and wraps it in an EmptyStateOverlay inside a
/// ScrolledWindow, so the test reaches it through the widget tree rather than
/// by widening the production API for tests.
GtkWidget *findListBox(GtkWidget *root)
{
    if (!root) return nullptr;
    if (GTK_IS_LIST_BOX(root)) return root;
    for (GtkWidget *c = gtk_widget_get_first_child(root); c;
         c = gtk_widget_get_next_sibling(c)) {
        if (GtkWidget *found = findListBox(c)) return found;
    }
    return nullptr;
}

/// One string per GtkListBoxRow actually present in the list box, built by
/// concatenating that row's label texts. This is what the user sees.
std::vector<std::string> renderedRows(Gtk::Widget &pvViewWidget)
{
    std::vector<std::string> out;
    GtkWidget *lb = findListBox(GTK_WIDGET(pvViewWidget.gobj()));
    if (!lb) return out;

    for (GtkWidget *row = gtk_widget_get_first_child(lb); row;
         row = gtk_widget_get_next_sibling(row)) {
        std::string text;
        GtkWidget *content = GTK_IS_LIST_BOX_ROW(row)
                                 ? gtk_list_box_row_get_child(GTK_LIST_BOX_ROW(row))
                                 : row;
        if (!content) { out.push_back(text); continue; }
        for (GtkWidget *l = gtk_widget_get_first_child(content); l;
             l = gtk_widget_get_next_sibling(l)) {
            if (GTK_IS_LABEL(l)) {
                text += gtk_label_get_text(GTK_LABEL(l));
                text += " ";
            }
        }
        out.push_back(text);
    }
    return out;
}

PVLine makePV(std::vector<Coord> moves, int depth, int pvIndex = 1)
{
    PVLine pv;
    pv.moves    = std::move(moves);
    pv.depth    = depth;
    pv.pvIndex  = pvIndex;
    pv.score    = 0.5;
    return pv;
}

bool anyRowContains(const std::vector<std::string> &rows, const std::string &needle)
{
    for (const auto &r : rows)
        if (r.find(needle) != std::string::npos) return true;
    return false;
}

}  // namespace

TEST_CASE("UI-07: PVView drops its row widgets when the PV list clears")
{
    if (!g_gtkReady) return;

    PVView pv;

    pv.update({makePV({Coord {10, 10}, Coord {11, 11}}, 21)}, 15);
    REQUIRE(renderedRows(pv).size() == 1);

    // Position change: the model clears, the panel refreshes with an empty
    // vector (this is what 60640c6 wired up). The widget tree must follow.
    pv.update({}, 15);
    CHECK(renderedRows(pv).size() == 0);

    // ... and a fresh analysis must land in a list that holds exactly its own
    // row, not one appended under a leftover.
    pv.update({makePV({Coord {9, 11}}, 22)}, 15);
    auto rows = renderedRows(pv);
    REQUIRE(rows.size() == 1);
    CHECK(anyRowContains(rows, "d22"));
}

TEST_CASE("UI-07: repeated clear/refill cycles never accumulate PV rows")
{
    if (!g_gtkReady) return;

    PVView pv;

    // The user saw ~12 stale "PV #1" rows in one session — one leaked per
    // position change. Twelve cycles must still end at one row.
    for (int i = 0; i < 12; ++i) {
        pv.update({makePV({Coord {7, 7}, Coord {8, 8}}, i)}, 15);
        REQUIRE(renderedRows(pv).size() == 1);
        pv.update({}, 15);
        REQUIRE(renderedRows(pv).size() == 0);
    }
    pv.update({makePV({Coord {9, 11}}, 99)}, 15);
    CHECK(renderedRows(pv).size() == 1);
}

TEST_CASE("UI-07: MultiPV shrink from N to M leaves exactly M rendered rows")
{
    if (!g_gtkReady) return;

    PVView pv;

    pv.update({makePV({Coord {1, 1}}, 5, 1),
               makePV({Coord {2, 2}}, 5, 2),
               makePV({Coord {3, 3}}, 5, 3),
               makePV({Coord {4, 4}}, 5, 4)},
              15);
    REQUIRE(renderedRows(pv).size() == 4);

    pv.update({makePV({Coord {1, 1}}, 6, 1), makePV({Coord {2, 2}}, 6, 2)}, 15);
    CHECK(renderedRows(pv).size() == 2);

    pv.update({makePV({Coord {1, 1}}, 7, 1)}, 15);
    auto rows = renderedRows(pv);
    REQUIRE(rows.size() == 1);
    CHECK(anyRowContains(rows, "PV #1"));
    CHECK_FALSE(anyRowContains(rows, "PV #2"));
}

TEST_CASE("UI-07: real AnalysisPanel, real MESSAGE-depth log, two positions -> one row")
{
    if (!g_gtkReady) return;

    GameState       gs(15);
    GomocupProtocol proto(15);
    AnalysisPanel   panel(gs);  // connects its own handlers, incl. 60640c6's

    // Mirror EngineController::connectProtocolSignals + MainWindow's
    // signal_engine_move -> GameState::makeMove wiring.
    proto.signal_analysis.connect(
        [&](const std::vector<PVLine> &pvs, const EngineStatus &status) {
            if (gs.isAnalyzing()) gs.setAnalysisData(pvs, status);
        });
    gs.signal_board_changed.connect([&]() { proto.clearAnalysisState(); });
    proto.signal_move.connect([&](Coord c) { gs.makeMove(c); });

    auto feed = [&](const std::vector<std::string> &lines) {
        for (const auto &l : lines) proto.parseLine(l);
        gs.flush();  // RT-01 coalescing: deliver to the view
    };

    // ---- Position 1: the three stones from the reported session ----
    REQUIRE(gs.makeMove(Coord {13, 12}));
    REQUIRE(gs.makeMove(Coord {13, 13}));
    REQUIRE(gs.makeMove(Coord {10, 13}));

    gs.setAnalyzing(true);
    for (const auto &cmd : proto.generateAnalyzeRequest(gs.currentPath(), 1)) (void)cmd;
    feed({
        "MESSAGE depth 2-3 ev -5 n 498 n/ms 498 tm 0 pv L3 K5",
        "MESSAGE depth 10-15 ev -9 n 42K n/ms 1425 tm 30 pv K5 M4 L4 J6 M3 O1 L5 J5 J4 L6",
        "MESSAGE depth 21-38 ev 6 n 5117K n/ms 1674 tm 3056 pv K5 L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5",
    });

    {
        auto rows = renderedRows(panel.pvView());
        REQUIRE(rows.size() == 1);
        CHECK(anyRowContains(rows, "d21/38"));
    }

    // ---- STOP -> RT-01 flush -> engine plays its own move 10,10 ----
    gs.setAnalyzing(false);
    gs.flush();
    proto.parseLine("STOP");
    proto.parseLine("10,10");  // signal_move -> makeMove -> signal_board_changed

    REQUIRE(gs.pvLines().empty());
    CHECK(renderedRows(panel.pvView()).size() == 0);

    // ---- Position 2: four stones, fresh analysis ----
    gs.setAnalyzing(true);
    for (const auto &cmd : proto.generateAnalyzeRequest(gs.currentPath(), 1)) (void)cmd;
    feed({
        "MESSAGE depth 2-4 ev 20 n 394 n/ms 394 tm 0 pv L4 J4 L6",
        "MESSAGE depth 15-26 ev -3 n 133K n/ms 1587 tm 84 pv L4 J4 L6 L5 J5 I6 M5 K3 K7 J8 I5 H6",
        "MESSAGE depth 22-44 ev -12 n 16M n/ms 1741 tm 9663 pv J4 K6 K3 K7 K4 L4 M3 L3 I5 L2 L5 H6",
    });

    auto rows = renderedRows(panel.pvView());

    // The reported bug rendered TWO rows here: the 3-stone position's
    // d21/38 "K5 → L4 → …" line on top, the 4-stone position's d22/44
    // "J4 → K6 → …" line below it.
    REQUIRE(rows.size() == 1);
    CHECK(anyRowContains(rows, "d22/44"));
    CHECK_FALSE(anyRowContains(rows, "d21/38"));
    CHECK(rows[0].rfind("PV #1", 0) == 0);
}

int main(int argc, char **argv)
{
    Gio::init();

    // GTK 4 aborts on a missing display; probe first so this binary is a
    // clean no-op on a headless machine rather than a spurious failure.
    if (gtk_init_check()) {
        g_gtkReady = true;
        // Gtk::Application::create() performs gtkmm's internal wrapper
        // registration (the same side effect src/application.cpp relies on);
        // without it Gtk::Object teardown asserts. NON_UNIQUE avoids touching
        // the session bus / an already-running instance.
        g_app = Gtk::Application::create("org.rapfi.gui.tests.ui07",
                                         Gio::Application::Flags::NON_UNIQUE);
    } else {
        std::cout << "[ui-tests] no display server available - GTK widget cases skipped\n";
    }

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int rc = context.run();

    g_app.reset();
    return rc;
}
