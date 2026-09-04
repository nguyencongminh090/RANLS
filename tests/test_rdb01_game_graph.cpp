// RDB-01 regression tests: GameGraph DTO <-> CBOR, and VariationTree <->
// GameGraph conversion.
//
// Covers instruction "Verification" tier 3, test_rdb01_game_graph.cpp bullets:
//   - CBOR round-trip of a GameGraph with analysed + NaN nodes, >=3 branches,
//     comments, a unicode comment
//   - a blob carrying unknown map keys still decodes (keys skipped, not errored)
//   - a "winrate" of 1.4 decodes as absent (not clamped, not a parse failure)
//   - random truncations of a good CBOR blob never crash decodeCbor
//   - toGameGraph o applyGameGraph reproduces tree structure + the per-node eval
//     vector exactly, with NaN staying NaN (never 0 / 0.5)
//   - applyGameGraph rejects forward/self/out-of-range parent references

#include "vendor/doctest.h"

#include "model/rdb/game_graph.h"
#include "model/rdb/game_graph_cbor.h"
#include "model/rdb/game_graph_convert.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <string>

using rdb::GameGraph;
using rdb::GraphNode;
using rdb::Move;
using rdb::NodeAnalysis;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// A CBOR uint header helper mirroring the encoder, for hand-building blobs.
void putHead(std::string &o, int major, uint64_t val)
{
    const auto mb = static_cast<unsigned char>(major << 5);
    if (val < 24) {
        o.push_back(static_cast<char>(mb | val));
    } else if (val <= 0xFF) {
        o.push_back(static_cast<char>(mb | 24));
        o.push_back(static_cast<char>(val));
    } else {
        o.push_back(static_cast<char>(mb | 25));
        o.push_back(static_cast<char>((val >> 8) & 0xFF));
        o.push_back(static_cast<char>(val & 0xFF));
    }
}
void putText(std::string &o, const std::string &s)
{
    putHead(o, 3, s.size());
    o += s;
}
void putDouble(std::string &o, double d)
{
    uint64_t bits;
    std::memcpy(&bits, &d, 8);
    o.push_back(static_cast<char>(0xFB));
    for (int i = 7; i >= 0; --i)
        o.push_back(static_cast<char>((bits >> (8 * i)) & 0xFF));
}

GameGraph sampleGraph()
{
    GameGraph g;
    g.schema    = rdb::kSchemaVersion;
    g.board     = 15;
    g.rule      = 2;
    g.created   = 1725446400;
    g.generator = "RANLS test";

    // nodes[0] root sentinel
    g.nodes.push_back(GraphNode{});

    auto analysed = [](uint32_t parent, int x, int y, double w, int d, int64_t n,
                       const std::string &c) {
        GraphNode gn;
        gn.parent = parent;
        gn.hasParent = true;
        gn.move = Move{x, y};
        gn.comment = c;
        NodeAnalysis a;
        a.winrate = w;
        a.depth = d;
        a.nodes = n;
        a.evalText = "+0.5";
        a.pv = {Move{8, 8}, Move{7, 9}};
        gn.analysis = std::move(a);
        return gn;
    };
    auto bare = [](uint32_t parent, int x, int y, const std::string &c) {
        GraphNode gn;
        gn.parent = parent;
        gn.hasParent = true;
        gn.move = Move{x, y};
        gn.comment = c;
        return gn; // no analysis => NaN
    };

    g.nodes.push_back(analysed(0, 7, 7, 0.53, 18, 4211234, "opening"));   // 1
    g.nodes.push_back(analysed(1, 8, 8, 0.47, 16, 900000, "reply"));      // 2
    g.nodes.push_back(bare(2, 7, 8, "unevaluated \xe2\x99\x9f branch"));  // 3 unicode
    g.nodes.push_back(analysed(1, 9, 9, 0.61, 12, 100, "sibline B"));     // 4 branch of 1
    g.nodes.push_back(bare(1, 6, 6, "sibline C"));                        // 5 branch of 1
    g.nodes.push_back(analysed(4, 10, 10, 0.4, 8, 42, ""));              // 6
    return g;
}

} // namespace

TEST_CASE("RDB-01 CBOR: round-trips a graph with analysed + NaN nodes and branches") {
    GameGraph g = sampleGraph();
    std::string blob = rdb::encodeCbor(g);
    auto back = rdb::decodeCbor(blob);
    REQUIRE(back.has_value());
    CHECK(*back == g);

    // node 3's comment survived unicode bytes
    CHECK(back->nodes[3].comment == g.nodes[3].comment);
    // NaN nodes have no analysis block
    CHECK_FALSE(back->nodes[3].analysis.has_value());
    CHECK_FALSE(back->nodes[5].analysis.has_value());
    // branch structure: nodes 2,4,5 all have parent 1
    CHECK(back->nodes[2].parent == 1);
    CHECK(back->nodes[4].parent == 1);
    CHECK(back->nodes[5].parent == 1);
}

TEST_CASE("RDB-01 CBOR: unknown map keys are skipped, not errored") {
    // Build { "schema":1, "board":15, "rule":0, "mystery":[1,2,3],
    //         "nodes":[ {} ], "extra": "ignored" }
    std::string blob;
    putHead(blob, 5, 6); // map, 6 pairs
    putText(blob, "schema");  putHead(blob, 0, 1);
    putText(blob, "board");   putHead(blob, 0, 15);
    putText(blob, "rule");    putHead(blob, 0, 0);
    putText(blob, "mystery"); putHead(blob, 4, 3);
        putHead(blob, 0, 1); putHead(blob, 0, 2); putHead(blob, 0, 3);
    putText(blob, "nodes");   putHead(blob, 4, 1);
        putHead(blob, 5, 0); // one empty node map (root sentinel)
    putText(blob, "extra");   putText(blob, "ignored");

    auto g = rdb::decodeCbor(blob);
    REQUIRE(g.has_value());
    CHECK(g->board == 15);
    CHECK(g->nodes.size() == 1);
}

TEST_CASE("RDB-01 CBOR: winrate outside [0,1] decodes as absent") {
    std::string blob;
    putHead(blob, 5, 4);
    putText(blob, "schema"); putHead(blob, 0, 1);
    putText(blob, "board");  putHead(blob, 0, 15);
    putText(blob, "rule");   putHead(blob, 0, 0);
    putText(blob, "nodes");  putHead(blob, 4, 2);
        putHead(blob, 5, 0);                 // root
        putHead(blob, 5, 2);                 // node 1
            putText(blob, "p"); putHead(blob, 0, 0);
            putText(blob, "a"); putHead(blob, 5, 1);
                putText(blob, "w"); putDouble(blob, 1.4);

    auto g = rdb::decodeCbor(blob);
    REQUIRE(g.has_value());
    REQUIRE(g->nodes.size() == 2);
    REQUIRE(g->nodes[1].analysis.has_value());
    CHECK_FALSE(g->nodes[1].analysis->winrate.has_value());
}

TEST_CASE("RDB-01 CBOR: random truncations never crash decodeCbor") {
    std::string blob = rdb::encodeCbor(sampleGraph());
    std::mt19937 rng(42);
    for (int i = 0; i < 500; ++i) {
        std::uniform_int_distribution<size_t> dist(0, blob.size());
        std::string t = blob.substr(0, dist(rng));
        auto g = rdb::decodeCbor(t); // must not throw / read OOB
        if (t.size() < blob.size())
            CHECK_FALSE(g.has_value());
    }
    // full blob still decodes
    CHECK(rdb::decodeCbor(blob).has_value());
}

TEST_CASE("RDB-01 convert: toGameGraph o applyGameGraph reproduces structure + evals") {
    VariationTree tree;
    // mainline: (7,7) -> (8,8) -> (7,8)
    TreeNode *a = tree.addMove(tree.root(), Coord{7, 7});
    a->eval = 0.55; a->depth = 10; a->nodes = 1000;
    TreeNode *b = tree.addMove(a, Coord{8, 8});
    b->eval = kNaN; // unevaluated
    TreeNode *c = tree.addMove(b, Coord{7, 8});
    c->eval = 0.42; c->depth = 7; c->nodes = 55;
    c->comment = "tactical";
    // branch off a: (9,9), (6,6)
    TreeNode *d = tree.addMove(a, Coord{9, 9});
    d->eval = 0.61; d->depth = 5; d->nodes = 20;
    TreeNode *e = tree.addMove(a, Coord{6, 6});
    e->eval = kNaN;

    GameGraph g = rdb::toGameGraph(tree, 15, GameRule::Renju,
                                   rdb::GraphMeta{"RANLS test", 111, 222});

    // Survive a full CBOR round-trip too.
    auto reg = rdb::decodeCbor(rdb::encodeCbor(g));
    REQUIRE(reg.has_value());
    CHECK(*reg == g);

    VariationTree out;
    int rebBoard = 0;
    GameRule rebRule = GameRule::Freestyle;
    std::string err;
    REQUIRE_MESSAGE(rdb::applyGameGraph(g, out, rebBoard, rebRule, &err), err);
    CHECK(rebBoard == 15);
    CHECK(rebRule == GameRule::Renju);

    TreeNode *ra = out.root()->findChild(Coord{7, 7});
    REQUIRE(ra != nullptr);
    CHECK(ra->eval == doctest::Approx(0.55));
    REQUIRE(ra->children.size() == 3); // (8,8), (9,9), (6,6) in that order
    CHECK(ra->children[0]->move == Coord{8, 8});
    CHECK(ra->children[1]->move == Coord{9, 9});
    CHECK(ra->children[2]->move == Coord{6, 6});
    CHECK(std::isnan(ra->children[0]->eval)); // was NaN, still NaN
    CHECK(std::isnan(ra->children[2]->eval));

    TreeNode *rc = ra->children[0]->findChild(Coord{7, 8});
    REQUIRE(rc != nullptr);
    CHECK(rc->eval == doctest::Approx(0.42));
    CHECK(rc->comment == "tactical");
    CHECK(rc->depth == 7);
    CHECK(rc->nodes == 55);
}

TEST_CASE("RDB-01 convert: applyGameGraph rejects bad parent references") {
    auto base = []() {
        GameGraph g;
        g.board = 15;
        g.rule  = 0;
        g.nodes.push_back(GraphNode{}); // root
        return g;
    };

    SUBCASE("forward reference") {
        GameGraph g = base();
        GraphNode n; n.hasParent = true; n.parent = 5; n.move = Move{7, 7};
        g.nodes.push_back(n);
        VariationTree out; int bs = 0; GameRule r{}; std::string err;
        CHECK_FALSE(rdb::applyGameGraph(g, out, bs, r, &err));
        CHECK_FALSE(err.empty());
    }
    SUBCASE("self reference") {
        GameGraph g = base();
        GraphNode n; n.hasParent = true; n.parent = 1; n.move = Move{7, 7};
        g.nodes.push_back(n);
        VariationTree out; int bs = 0; GameRule r{}; std::string err;
        CHECK_FALSE(rdb::applyGameGraph(g, out, bs, r, &err));
    }
    SUBCASE("missing parent flag") {
        GameGraph g = base();
        GraphNode n; n.hasParent = false; n.move = Move{7, 7};
        g.nodes.push_back(n);
        VariationTree out; int bs = 0; GameRule r{}; std::string err;
        CHECK_FALSE(rdb::applyGameGraph(g, out, bs, r, &err));
    }
    SUBCASE("board size out of range") {
        GameGraph g = base();
        g.board = 99;
        VariationTree out; int bs = 0; GameRule r{}; std::string err;
        CHECK_FALSE(rdb::applyGameGraph(g, out, bs, r, &err));
    }
    SUBCASE("schema too new") {
        GameGraph g = base();
        g.schema = rdb::kSchemaVersion + 1;
        VariationTree out; int bs = 0; GameRule r{}; std::string err;
        CHECK_FALSE(rdb::applyGameGraph(g, out, bs, r, &err));
    }
}
