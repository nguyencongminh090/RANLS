// RDB-02 widget-level regression guard: the Save/Open wiring compiled and
// linked into the real gtkmm MainWindow, plus a full save -> newGame -> open
// round trip through the archive layer.
//
// The archive/apply helpers are model-layer (gtkmm-free) and get their detailed
// coverage in tests/test_rdb02_archive.cpp (ranls-gui-tests). This case adds
// what only the gtkmm-linked target can prove: that MainWindow still builds with
// the new rdb::archive* / rdb::toGameGraph wiring, that the "save-game" /
// "load-game" actions still exist, and that a tree WITH A BRANCH survives a
// disk round trip (board size / rule / total node count / mainline move count).
//
// Links gtkmm; self-skips with a clean exit 0 when no display server is
// available (main() lives in test_ui07_pv_view_rows.cpp and probes
// gtk_init_check()).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "main_window.h"

#include "model/game_state.h"
#include "model/variation_tree.h"
#include "model/rdb/game_archive.h"
#include "model/rdb/game_graph_convert.h"

#include <cstdio>
#include <filesystem>

namespace {
bool gtkReady() { return gtk_init_check(); }

int countNodes(const TreeNode *n)
{
    int c = 0;
    for (const auto &ch : n->children)
        c += 1 + countNodes(ch.get());
    return c;
}
} // namespace

TEST_CASE("RDB-02: a real MainWindow links with the .rdb Save/Open wiring")
{
    if (!gtkReady()) return;

    MainWindow window;
    CHECK(window.lookup_action("save-game"));
    CHECK(window.lookup_action("load-game"));
}

TEST_CASE("RDB-02: save a branched tree to .rdb, newGame, open it -> tree survives")
{
    if (!gtkReady()) return;

    GameState gs(15);
    gs.newGame(15);
    gs.setRule(GameRule::Renju);
    REQUIRE(gs.makeMove({7, 7}));
    REQUIRE(gs.makeMove({8, 8}));
    REQUIRE(gs.makeMove({9, 9}));

    // Branch off the first move.
    TreeNode *m1 = gs.tree().root()->children.front().get();
    TreeNode *branch = gs.tree().addMove(m1, Coord{5, 11});
    branch->comment = "branch";

    const int totalNodes = countNodes(gs.tree().root()); // 4

    auto path = std::filesystem::temp_directory_path() / "ranls-rdb02-ui.rdb";
    auto graph = rdb::toGameGraph(gs.tree(), gs.boardSize(), gs.rule(),
                                  rdb::GraphMeta{"RANLS", {}, {}});
    auto writer = rdb::archiveWriterFor(path);
    REQUIRE(writer);
    std::string err;
    REQUIRE_MESSAGE(writer->save(path, graph, &err), err);

    // Fresh game, then open.
    gs.newGame(19);
    REQUIRE(gs.boardSize() == 19);

    auto reader = rdb::archiveReaderFor(path);
    auto back   = reader->load(path, &err);
    REQUIRE_MESSAGE(back.has_value(), err);
    REQUIRE_MESSAGE(rdb::applyGameGraphToState(gs, *back, &err), err);

    CHECK(gs.boardSize() == 15);
    CHECK(gs.rule() == GameRule::Renju);
    CHECK(countNodes(gs.tree().root()) == totalNodes);
    CHECK(gs.history().moveCount() == 3); // mainline replayed

    std::remove(path.string().c_str());
}
