// RDB-02 regression tests: the archive layer (game_archive.*) that wires the
// RDB-01 substrate into Save/Open.
//
// Covers instruction "Verification" tier 3:
//   - archiveReaderFor / archiveWriterFor pick the right impl by (lowercased)
//     extension; there is NO `.yxgame` writer
//   - RdbArchive save -> load round-trips a GameGraph (tree + branches +
//     comments + in-range evals survive)
//   - YxgameReader turns a hand-written legacy `.yxgame` into the right linear
//     move chain with NO analysis on any node (=> every eval NaN)
//   - applyGameGraphToState rebuilds a GameState's tree, advances the mainline,
//     and rejects an off-board move coord without touching the current game
//
// gtkmm-free — lives in the model-only ranls-gui-tests target.

#include "vendor/doctest.h"

#include "model/rdb/game_archive.h"
#include "model/rdb/game_graph_convert.h"
#include "model/game_state.h"
#include "model/variation_tree.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

using rdb::GameGraph;

namespace {

std::filesystem::path tmpFile(const std::string &name)
{
    return std::filesystem::temp_directory_path() / ("ranls-rdb02-" + name);
}

int countNodes(const TreeNode *n)
{
    int c = 0;
    for (const auto &ch : n->children)
        c += 1 + countNodes(ch.get());
    return c;
}

} // namespace

TEST_CASE("RDB-02 factory: reader/writer chosen by lowercased extension")
{
    CHECK(dynamic_cast<rdb::RdbArchive *>(rdb::archiveReaderFor("game.rdb").get()));
    CHECK(dynamic_cast<rdb::RdbArchive *>(rdb::archiveReaderFor("GAME.RDB").get()));
    CHECK(dynamic_cast<rdb::YxgameReader *>(rdb::archiveReaderFor("old.yxgame").get()));
    CHECK(dynamic_cast<rdb::YxgameReader *>(rdb::archiveReaderFor("OLD.YxGame").get()));
    // Unknown / missing extension: fall back to the binary reader, never guess
    // `.yxgame`.
    CHECK(dynamic_cast<rdb::RdbArchive *>(rdb::archiveReaderFor("mystery").get()));

    CHECK(dynamic_cast<rdb::RdbArchive *>(rdb::archiveWriterFor("game.rdb").get()));
    CHECK(dynamic_cast<rdb::RdbArchive *>(rdb::archiveWriterFor("game.RDB").get()));
    // No `.yxgame` writer — saving that format is a caller-visible error.
    CHECK(rdb::archiveWriterFor("old.yxgame") == nullptr);
    CHECK(rdb::archiveWriterFor("whatever.txt") == nullptr);
}

TEST_CASE("RDB-02 RdbArchive: save then load round-trips tree + branches + comments")
{
    GameState gs(15);
    gs.newGame(15);
    gs.setRule(GameRule::Renju);
    REQUIRE(gs.makeMove({7, 7}));
    REQUIRE(gs.makeMove({8, 8}));
    REQUIRE(gs.makeMove({7, 8}));

    // A branch off move 1 (the {7,7} node) + a comment + an in-range eval.
    TreeNode *m1 = gs.tree().root()->children.front().get();
    TreeNode *branch = gs.tree().addMove(m1, Coord{3, 3});
    branch->comment = "sideline";
    branch->eval    = 0.62;
    branch->depth   = 12;
    branch->nodes   = 4242;

    const int originalNodes = countNodes(gs.tree().root());

    auto graph = rdb::toGameGraph(gs.tree(), gs.boardSize(), gs.rule(),
                                  rdb::GraphMeta{"test", {}, {}});

    auto path   = tmpFile("roundtrip.rdb");
    auto writer = rdb::archiveWriterFor(path);
    REQUIRE(writer);
    std::string err;
    REQUIRE_MESSAGE(writer->save(path, graph, &err), err);

    auto reader = rdb::archiveReaderFor(path);
    auto back   = reader->load(path, &err);
    REQUIRE_MESSAGE(back.has_value(), err);
    CHECK(back->board == 15);
    CHECK(back->rule == 2);
    CHECK(back->nodes.size() == graph.nodes.size());

    // Apply onto a *different* GameState and confirm the model rebuilds.
    GameState loaded(19);
    loaded.newGame(19);
    REQUIRE_MESSAGE(rdb::applyGameGraphToState(loaded, *back, &err), err);
    CHECK(loaded.boardSize() == 15);
    CHECK(loaded.rule() == GameRule::Renju);
    CHECK(countNodes(loaded.tree().root()) == originalNodes);
    // Mainline (first-child chain) replayed through makeMove().
    CHECK(loaded.history().moveCount() == 3);

    // The branch node survived with its comment + eval.
    const TreeNode *lm1 = loaded.tree().root()->children.front().get();
    REQUIRE(lm1->children.size() == 2);
    const TreeNode *lb = lm1->findChild(Coord{3, 3});
    REQUIRE(lb);
    CHECK(lb->comment == "sideline");
    CHECK(lb->eval == doctest::Approx(0.62));

    std::remove(path.string().c_str());
}

TEST_CASE("RDB-02 YxgameReader: legacy file -> linear chain, every node NaN")
{
    auto path = tmpFile("legacy.yxgame");
    {
        std::ofstream out(path, std::ios::trunc);
        out << "yxgame_version=1\nboard_size=15\nrule=1\n"
            << "move=7,7\nmove=8,8\nmove=9,9\n";
    }

    rdb::YxgameReader reader;
    std::string       err;
    auto              g = reader.load(path, &err);
    REQUIRE_MESSAGE(g.has_value(), err);

    CHECK(g->board == 15);
    CHECK(g->rule == 1);
    REQUIRE(g->nodes.size() == 4); // sentinel + 3 moves
    CHECK_FALSE(g->nodes[0].hasParent);
    CHECK_FALSE(g->nodes[0].move.has_value());

    const rdb::Move expected[3] = {{7, 7}, {8, 8}, {9, 9}};
    for (size_t i = 1; i < g->nodes.size(); ++i) {
        CHECK(g->nodes[i].hasParent);
        CHECK(g->nodes[i].parent == i - 1); // linear chain
        REQUIRE(g->nodes[i].move.has_value());
        CHECK(*g->nodes[i].move == expected[i - 1]);
        CHECK_FALSE(g->nodes[i].analysis.has_value()); // never evaluated => NaN
    }

    // Applied to a GameState it looks exactly like a fresh 3-move game.
    GameState gs(19);
    gs.newGame(19);
    REQUIRE(rdb::applyGameGraphToState(gs, *g, &err));
    CHECK(gs.boardSize() == 15);
    CHECK(gs.rule() == GameRule::Standard);
    CHECK(gs.history().moveCount() == 3);
    for (const auto &nan : gs.evalHistory())
        CHECK(std::isnan(nan));

    std::remove(path.string().c_str());
}

TEST_CASE("RDB-02 YxgameReader: a bad legacy line fails cleanly")
{
    auto path = tmpFile("bad.yxgame");
    {
        std::ofstream out(path, std::ios::trunc);
        out << "yxgame_version=1\nboard_size=15\nrule=0\nmove=99,99\n";
    }
    rdb::YxgameReader reader;
    std::string       err;
    auto              g = reader.load(path, &err);
    CHECK_FALSE(g.has_value());
    CHECK_FALSE(err.empty());
    std::remove(path.string().c_str());
}

TEST_CASE("RDB-02 applyGameGraphToState: an off-board coord is rejected, game untouched")
{
    GameState gs(15);
    gs.newGame(15);
    gs.setRule(GameRule::Freestyle);
    REQUIRE(gs.makeMove({5, 5}));

    // Hand-build a graph carrying an off-board move (RDB-01's applyGameGraph
    // does not range-check coords — RDB-02's helper must).
    GameGraph g;
    g.schema = rdb::kSchemaVersion;
    g.board  = 15;
    g.rule   = 0;
    rdb::GraphNode root;
    root.hasParent = false;
    g.nodes.push_back(root);
    rdb::GraphNode bad;
    bad.hasParent = true;
    bad.parent    = 0;
    bad.move      = rdb::Move{40, 40};
    g.nodes.push_back(bad);

    std::string err;
    CHECK_FALSE(rdb::applyGameGraphToState(gs, g, &err));
    CHECK_FALSE(err.empty());
    // Current game intact.
    CHECK(gs.boardSize() == 15);
    CHECK(gs.history().moveCount() == 1);
}
