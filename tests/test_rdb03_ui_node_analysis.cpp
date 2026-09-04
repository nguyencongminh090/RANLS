// RDB-03 widget-level regression guard (ranls-gui-ui-tests): the ANLZ-03 goal
// exercised through the real gtkmm-linked stack — set evals on a couple of tree
// nodes directly, save a `.rdb`, newGame(), open it, and assert
// GameState::evalHistory() comes back restored WITHOUT any engine call.
//
// The detailed model-layer coverage is in tests/test_rdb03_node_analysis.cpp
// (ranls-gui-tests). This case only adds what the gtkmm target can prove:
// nothing in the archive/apply path regressed once linked into MainWindow's
// world, and a real on-disk `.rdb` file round-trips the win-rate graph.
//
// Links gtkmm; self-skips with a clean exit 0 when no display server is
// available (main() lives in test_ui07_pv_view_rows.cpp and probes
// gtk_init_check()).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "model/game_state.h"
#include "model/variation_tree.h"
#include "model/rdb/game_archive.h"
#include "model/rdb/game_graph_convert.h"

#include <cmath>
#include <cstdio>
#include <filesystem>

namespace {
bool gtkReady() { return gtk_init_check(); }
} // namespace

TEST_CASE("RDB-03: save a game with a populated win-rate graph, reopen -> graph restored, no re-analysis")
{
    if (!gtkReady()) return;

    GameState gs(15);
    gs.newGame(15);
    gs.setRule(GameRule::Freestyle);
    const Coord moves[] = {{7, 7}, {7, 8}, {8, 8}, {8, 7}};
    for (const auto &m : moves)
        REQUIRE(gs.makeMove(m));

    // "Analyse-stub" a couple of nodes directly (no engine).
    TreeNode *n0 = gs.tree().getNode({moves[0]});
    REQUIRE(n0);
    n0->eval = 0.66; n0->depth = 13; n0->nodes = 250000;
    TreeNode *n2 = gs.tree().getNode({moves[0], moves[1], moves[2]});
    REQUIRE(n2);
    n2->eval = 0.34; n2->depth = 10; n2->nodes = 120000;

    const auto orig = gs.evalHistory();
    REQUIRE(orig.size() == 4);
    REQUIRE_FALSE(std::isnan(orig[0]));
    REQUIRE(std::isnan(orig[1]));
    REQUIRE_FALSE(std::isnan(orig[2]));

    auto path   = std::filesystem::temp_directory_path() / "ranls-rdb03-ui.rdb";
    auto graph  = rdb::toGameGraph(gs.tree(), gs.boardSize(), gs.rule(),
                                   rdb::GraphMeta{"RANLS", {}, {}, {}});
    auto writer = rdb::archiveWriterFor(path);
    REQUIRE(writer);
    std::string err;
    REQUIRE_MESSAGE(writer->save(path, graph, &err), err);

    // Fresh game wipes the graph.
    gs.newGame(15);
    for (double v : gs.evalHistory())
        (void)v; // history is now empty
    REQUIRE(gs.history().moveCount() == 0);

    // Open it back.
    auto reader = rdb::archiveReaderFor(path);
    auto back   = reader->load(path, &err);
    REQUIRE_MESSAGE(back.has_value(), err);
    REQUIRE_MESSAGE(rdb::applyGameGraphToState(gs, *back, &err), err);

    auto restored = gs.evalHistory();
    REQUIRE(restored.size() == orig.size());
    for (size_t i = 0; i < orig.size(); ++i) {
        if (std::isnan(orig[i]))
            CHECK(std::isnan(restored[i]));
        else
            CHECK(restored[i] == doctest::Approx(orig[i]));
    }

    std::remove(path.string().c_str());
}
