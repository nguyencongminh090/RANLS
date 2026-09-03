// Regression tests for UI-13: the win graph must record the engine's win% for
// every position it actually scored during a search, regardless of side to
// move / MatchConfig::enginePlays.
//
// Root cause pinned here (see docs/fix-log/2026-09-03-wingraph-record-eval-
// regardless-of-side.md): GameState::setAnalysisData() only ever wrote the eval
// of the single node at currentPath() — the position being searched. The
// engine's best PV also implies an eval for the position AFTER its best move,
// but that child node was never written, so the very next ply stayed NaN
// ("unevaluated" sentinel, UI-01) and rendered as a visible gap.
//
// Fix A (this test target): setAnalysisData() also fills the eval of the child
// node for bestPv.moves[0] when that child is already on the played line, using
// the complementary win% (side to move flips for the child) at a derived
// depth > 0. It never fabricates a node for an un-played PV move, and never
// overwrites a child that has its own real analysis.
//
// The EngineController::signal_move flush/ordering half of UI-13 is a pure
// statement reorder inside one signal handler (deliver the final analysis for
// the searched position, THEN play the move, THEN transition state); it is
// guarded by the existing ENG-01/ENG-02 controller tests staying green — no
// gtkmm-free harness feeds analysis+move through a real EngineController.

#include "vendor/doctest.h"

#include "model/game_state.h"

#include <cmath>

namespace {

Coord at(int x, int y) { return Coord{x, y}; }

PVLine pvWith(int depth, int64_t nodes, double score, std::vector<Coord> moves) {
    PVLine pv;
    pv.pvIndex = 1;
    pv.depth   = depth;
    pv.nodes   = nodes;
    pv.score   = score;
    pv.moves   = std::move(moves);
    return pv;
}

EngineStatus dummyStatus() { return EngineStatus{}; }

} // namespace

TEST_CASE("UI-13: setAnalysisData fills an existing child node from the best PV") {
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));  // ply 0 (Black)
    REQUIRE(gs.makeMove(at(7, 8)));  // ply 1 (White)
    REQUIRE(gs.makeMove(at(8, 8)));  // ply 2 (Black)

    // Step back to the position after ply 0. The tree keeps [7,8] and [8,8] as
    // nodes on the played line even though the cursor has moved back.
    gs.gotoMove(0);
    REQUIRE(gs.currentPath().size() == 1);

    // Engine searches this position and returns a best line into the next ply.
    gs.setAnalysisData({pvWith(12, 4000, 0.72, {at(7, 8), at(8, 8)})}, dummyStatus());

    // The searched position's own node is written as before.
    TreeNode *self = gs.tree().getNode({at(7, 7)});
    REQUIRE(self != nullptr);
    CHECK(self->eval == doctest::Approx(0.72));

    // UI-13: the child node [7,7]->[7,8] is filled with the complementary win%
    // at a derived depth > 0 (so evalHistory() does not treat it as unevaluated).
    TreeNode *child = gs.tree().getNode({at(7, 7), at(7, 8)});
    REQUIRE(child != nullptr);
    CHECK(child->depth > 0);
    CHECK(child->eval == doctest::Approx(0.28));

    // Walking the full played line, ply 1 is no longer a NaN gap.
    gs.gotoMove(2);
    auto hist = gs.evalHistory();
    REQUIRE(hist.size() == 3);
    CHECK_FALSE(std::isnan(hist[0]));
    CHECK_FALSE(std::isnan(hist[1]));  // was NaN before UI-13
}

TEST_CASE("UI-13: a child with its own real analysis is not overwritten by the derived estimate") {
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));
    REQUIRE(gs.makeMove(at(7, 8)));

    // Real, deep analysis of the child position.
    gs.setAnalysisData({pvWith(20, 100000, 0.65, {at(9, 9)})}, dummyStatus());

    gs.gotoMove(0);
    gs.setAnalysisData({pvWith(10, 2000, 0.90, {at(7, 8)})}, dummyStatus());

    TreeNode *child = gs.tree().getNode({at(7, 7), at(7, 8)});
    REQUIRE(child != nullptr);
    CHECK(child->eval == doctest::Approx(0.65));  // untouched
    CHECK(child->depth == 20);
}

TEST_CASE("UI-13: setAnalysisData never fabricates a child node for an un-played PV move") {
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));

    gs.setAnalysisData({pvWith(12, 4000, 0.6, {at(3, 3), at(4, 4)})}, dummyStatus());

    CHECK(gs.tree().getNode({at(7, 7), at(3, 3)}) == nullptr);
}

TEST_CASE("UI-13: no NaN gap for any position the engine scored, across a full line") {
    GameState gs;
    const Coord moves[] = {at(7, 7), at(7, 8), at(8, 8), at(8, 7), at(9, 9)};
    for (const auto &m : moves)
        REQUIRE(gs.makeMove(m));

    // "Engine plays" style: the engine only searched its own turns (positions
    // after ply 0 and ply 2), each time returning a best line into the ply the
    // engine then played.
    gs.gotoMove(0);
    gs.setAnalysisData({pvWith(14, 5000, 0.80, {moves[1], moves[2]})}, dummyStatus());
    gs.gotoMove(2);
    gs.setAnalysisData({pvWith(14, 5000, 0.40, {moves[3], moves[4]})}, dummyStatus());

    gs.gotoMove(4);
    auto hist = gs.evalHistory();
    REQUIRE(hist.size() == 5);

    // ply 0 / ply 2: scored directly (search root). ply 1 / ply 3: scored via
    // the UI-13 child-fill from the best PV. None may be a NaN gap.
    CHECK_FALSE(std::isnan(hist[0]));
    CHECK_FALSE(std::isnan(hist[1]));
    CHECK_FALSE(std::isnan(hist[2]));
    CHECK_FALSE(std::isnan(hist[3]));

    // ply 4 was never searched and no search root's best move reached it — it
    // stays a genuine gap (UI-01: no false 50%).
    CHECK(std::isnan(hist[4]));

    // Complementary-perspective sanity: ply 1's node holds 1 - 0.80.
    CHECK(hist[1] == doctest::Approx(0.20));
}
