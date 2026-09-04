// RDB-03 regression tests: persist + restore per-node analysis end-to-end —
// the carried-over ANLZ-03 regression set (save a populated WinGraph, reopen,
// the graph is back with no re-analysis).
//
// Covers instruction "Verification" tier 3:
//   - mixed evaluated/NaN tree -> full round-trip (convert -> CBOR -> container
//     -> back) -> evalHistory() equals the original vector exactly, NaN
//     positions still NaN
//   - a NaN node serialises no winrate; a restored NaN node never reads 0.5
//   - a legacy .yxgame via YxgameReader -> every node NaN, no crash, empty
//     WinGraph like a fresh game
//   - winrate = 1.7 in a crafted GameGraph/blob -> treated as absent, load OK
//   - PV + evalText + glyph + comment survive a node round-trip
//   - a FRESH game (no load) still produces an all-NaN evalHistory() — proves
//     the D1 gate / TreeNode::eval-default change did not regress the
//     unanalysed case
//
// gtkmm-free — lives in the model-only ranls-gui-tests target.

#include "vendor/doctest.h"

#include "model/rdb/game_archive.h"
#include "model/rdb/game_graph_cbor.h"
#include "model/rdb/game_graph_convert.h"
#include "model/game_state.h"
#include "model/variation_tree.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

using rdb::GameGraph;

namespace {

Coord at(int x, int y) { return Coord{x, y}; }

std::filesystem::path tmpFile(const std::string &name)
{
    return std::filesystem::temp_directory_path() / ("ranls-rdb03-" + name);
}

// Full pipeline: VariationTree -> GameGraph -> CBOR bytes -> GameGraph -> a
// fresh GameState. Mirrors exactly what Save then Open does (minus the on-disk
// container framing, which test_rdb01_container.cpp already pins).
bool roundTrip(const GameState &src, GameState &dst, const rdb::GraphMeta &meta = {})
{
    GameGraph g = rdb::toGameGraph(src.tree(), src.boardSize(), src.rule(), meta);
    auto      decoded = rdb::decodeCbor(rdb::encodeCbor(g));
    if (!decoded)
        return false;
    std::string err;
    return rdb::applyGameGraphToState(dst, *decoded, &err);
}

void expectSameEvalHistory(const std::vector<double> &a, const std::vector<double> &b)
{
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::isnan(a[i]))
            CHECK(std::isnan(b[i]));
        else {
            CHECK_FALSE(std::isnan(b[i]));
            CHECK(b[i] == doctest::Approx(a[i]));
        }
    }
}

} // namespace

TEST_CASE("RDB-03: a fresh, never-analysed game has an all-NaN evalHistory (D1 guard)")
{
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));
    REQUIRE(gs.makeMove(at(8, 8)));
    REQUIRE(gs.makeMove(at(9, 9)));

    auto hist = gs.evalHistory();
    REQUIRE(hist.size() == 3);
    for (double v : hist)
        CHECK(std::isnan(v));
}

TEST_CASE("RDB-03: mixed evaluated/NaN mainline round-trips evalHistory() exactly")
{
    GameState gs(15);
    gs.newGame(15);
    gs.setRule(GameRule::Renju);
    const Coord moves[] = {at(7, 7), at(7, 8), at(8, 8), at(8, 7), at(9, 9)};
    for (const auto &m : moves)
        REQUIRE(gs.makeMove(m));

    // Analyse plies 0 and 2 directly on the tree; leave 1, 3, 4 unevaluated.
    TreeNode *n0 = gs.tree().getNode({moves[0]});
    REQUIRE(n0);
    n0->eval = 0.63; n0->depth = 14; n0->nodes = 900000;
    TreeNode *n2 = gs.tree().getNode({moves[0], moves[1], moves[2]});
    REQUIRE(n2);
    n2->eval = 0.29; n2->depth = 11; n2->nodes = 400000;

    const auto orig = gs.evalHistory();
    REQUIRE(orig.size() == 5);
    REQUIRE_FALSE(std::isnan(orig[0]));
    REQUIRE(std::isnan(orig[1]));
    REQUIRE_FALSE(std::isnan(orig[2]));
    REQUIRE(std::isnan(orig[3]));
    REQUIRE(std::isnan(orig[4]));

    GameState loaded(19);
    loaded.newGame(19);
    REQUIRE(roundTrip(gs, loaded, rdb::GraphMeta{"RANLS test", {}, {}, {}}));

    CHECK(loaded.boardSize() == 15);
    CHECK(loaded.rule() == GameRule::Renju);
    expectSameEvalHistory(orig, loaded.evalHistory());
}

TEST_CASE("RDB-03: a NaN node serialises no winrate and never restores as 0.5")
{
    GameState gs(15);
    gs.newGame(15);
    REQUIRE(gs.makeMove(at(7, 7)));
    REQUIRE(gs.makeMove(at(8, 8)));
    // ply 0 analysed, ply 1 left NaN.
    gs.tree().getNode({at(7, 7)})->eval = 0.7;
    gs.tree().getNode({at(7, 7)})->depth = 9;

    GameGraph g = rdb::toGameGraph(gs.tree(), 15, gs.rule(), {});
    REQUIRE(g.nodes.size() == 3);
    // nodes[1] = ply 0 (analysed), nodes[2] = ply 1 (NaN).
    CHECK(g.nodes[1].analysis.has_value());
    CHECK_FALSE(g.nodes[2].analysis.has_value());

    GameState loaded(15);
    loaded.newGame(15);
    REQUIRE(roundTrip(gs, loaded));

    auto hist = loaded.evalHistory();
    REQUIRE(hist.size() == 2);
    CHECK_FALSE(std::isnan(hist[0]));
    CHECK(std::isnan(hist[1]));
    CHECK(hist[1] != doctest::Approx(0.5));
}

TEST_CASE("RDB-03: legacy .yxgame imports as all-NaN, exactly like a fresh game")
{
    auto path = tmpFile("legacy.yxgame");
    {
        std::ofstream out(path, std::ios::trunc);
        out << "yxgame_version=1\nboard_size=15\nrule=0\n"
            << "move=7,7\nmove=8,8\nmove=9,9\nmove=10,10\n";
    }

    rdb::YxgameReader reader;
    std::string       err;
    auto              g = reader.load(path, &err);
    REQUIRE_MESSAGE(g.has_value(), err);

    GameState gs(19);
    gs.newGame(19);
    REQUIRE(rdb::applyGameGraphToState(gs, *g, &err));
    CHECK(gs.boardSize() == 15);
    CHECK(gs.history().moveCount() == 4);

    auto hist = gs.evalHistory();
    REQUIRE(hist.size() == 4);
    for (double v : hist)
        CHECK(std::isnan(v));

    std::remove(path.string().c_str());
}

TEST_CASE("RDB-03: an out-of-range persisted winrate (1.7) is treated as absent, load succeeds")
{
    // Hand-built graph: one analysed-looking node whose winrate is 1.7.
    GameGraph g;
    g.schema = rdb::kSchemaVersion;
    g.board  = 15;
    g.rule   = 0;
    rdb::GraphNode root;
    root.hasParent = false;
    g.nodes.push_back(root);
    rdb::GraphNode n;
    n.hasParent = true;
    n.parent    = 0;
    n.move      = rdb::Move{7, 7};
    rdb::NodeAnalysis a;
    a.winrate = 1.7; // out of [0,1]
    a.depth   = 10;
    n.analysis = a;
    g.nodes.push_back(n);

    // Direct apply.
    GameState gs(15);
    gs.newGame(15);
    std::string err;
    REQUIRE_MESSAGE(rdb::applyGameGraphToState(gs, g, &err), err);
    auto hist = gs.evalHistory();
    REQUIRE(hist.size() == 1);
    CHECK(std::isnan(hist[0]));

    // Same through a CBOR round-trip (the decoder also drops it).
    auto decoded = rdb::decodeCbor(rdb::encodeCbor(g));
    REQUIRE(decoded.has_value());
    GameState gs2(15);
    gs2.newGame(15);
    REQUIRE(rdb::applyGameGraphToState(gs2, *decoded, &err));
    CHECK(std::isnan(gs2.evalHistory()[0]));
}

TEST_CASE("RDB-03: PV, evalText, glyph and comment survive a node round-trip")
{
    GameState gs(15);
    gs.newGame(15);
    REQUIRE(gs.makeMove(at(7, 7)));
    REQUIRE(gs.makeMove(at(8, 8)));

    TreeNode *n0 = gs.tree().getNode({at(7, 7)});
    REQUIRE(n0);
    n0->eval    = 0.55;
    n0->depth   = 12;
    n0->nodes   = 123456;
    n0->comment = "White must block";
    NodeAnalysisExtras ex;
    ex.evalText    = "+0.53";
    ex.pv          = {at(8, 8), at(7, 9), at(9, 9)};
    ex.glyph       = "?!";
    ex.engineRef   = 0;
    ex.analyzedUtc = 1725449000;
    n0->analysis   = ex;

    GameState loaded(15);
    loaded.newGame(15);
    REQUIRE(roundTrip(gs, loaded));

    TreeNode *r0 = loaded.tree().getNode({at(7, 7)});
    REQUIRE(r0);
    CHECK(r0->eval == doctest::Approx(0.55));
    CHECK(r0->depth == 12);
    CHECK(r0->nodes == 123456);
    CHECK(r0->comment == "White must block");
    REQUIRE(r0->analysis.has_value());
    CHECK(r0->analysis->evalText == "+0.53");
    CHECK(r0->analysis->glyph == "?!");
    CHECK(r0->analysis->engineRef == 0);
    CHECK(r0->analysis->analyzedUtc == 1725449000);
    REQUIRE(r0->analysis->pv.size() == 3);
    CHECK(r0->analysis->pv[0] == at(8, 8));
    CHECK(r0->analysis->pv[2] == at(9, 9));
}

TEST_CASE("RDB-03: an off-board pv coord is dropped, the node's eval still restores")
{
    GameState gs(15);
    gs.newGame(15);
    REQUIRE(gs.makeMove(at(7, 7)));
    TreeNode *n0 = gs.tree().getNode({at(7, 7)});
    n0->eval  = 0.4;
    n0->depth = 8;
    NodeAnalysisExtras ex;
    ex.pv = {at(8, 8), at(99, 99)}; // second coord off-board
    n0->analysis = ex;

    GameState loaded(15);
    loaded.newGame(15);
    REQUIRE(roundTrip(gs, loaded));

    TreeNode *r0 = loaded.tree().getNode({at(7, 7)});
    REQUIRE(r0);
    CHECK(r0->eval == doctest::Approx(0.4));
    REQUIRE(r0->analysis.has_value());
    CHECK(r0->analysis->pv.empty()); // whole pv dropped, load did not abort
}

TEST_CASE("RDB-03: engines[] metadata round-trips and an empty list never fails the load")
{
    GameState gs(15);
    gs.newGame(15);
    REQUIRE(gs.makeMove(at(7, 7)));

    rdb::GraphMeta meta;
    meta.generator = "RANLS";
    rdb::EngineInfo ei;
    ei.id = 0; ei.name = "pbrain-rapfi"; ei.params = "threads=4";
    meta.engines.push_back(ei);

    GameGraph g = rdb::toGameGraph(gs.tree(), 15, gs.rule(), meta);
    REQUIRE(g.engines.size() == 1);
    auto decoded = rdb::decodeCbor(rdb::encodeCbor(g));
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->engines.size() == 1);
    CHECK(decoded->engines[0].name == "pbrain-rapfi");

    GameState loaded(15);
    loaded.newGame(15);
    std::string err;
    CHECK(rdb::applyGameGraphToState(loaded, *decoded, &err));

    // Empty engines list: still loads.
    GameGraph g2 = g;
    g2.engines.clear();
    auto decoded2 = rdb::decodeCbor(rdb::encodeCbor(g2));
    REQUIRE(decoded2.has_value());
    GameState loaded2(15);
    loaded2.newGame(15);
    CHECK(rdb::applyGameGraphToState(loaded2, *decoded2, &err));
}
