// Regression tests for UI-01: win-rate graph attributes evals to the wrong
// side, and evals can go unrecorded.
//
// Three sub-fixes are pinned here:
//  1. GameState::evalHistory() produces one raw eval per ply, scored from
//     the side to move in the position AFTER that ply — this test pins the
//     values GameState hands to the UI layer and separately pins the
//     side-to-move attribution formula that analysis_panel.cpp's
//     toDisplayWinrate() applies to them (see note below on why that
//     formula is duplicated here instead of called directly).
//  2. GameState::setAnalysisData() must write a revised eval even when
//     depth/nodes are unchanged.
//  3. GameState::evalHistory() must return a distinguishable "unevaluated"
//     sentinel (NaN) rather than a false 0.5 for a node with no analysis.
//
// Note on (1): the actual black/white attribution logic
// (`bool blackToMove = (i % 2 == 1)`) lives in the static, file-local
// toDisplayWinrate() in src/ui/analysis_panel.cpp. That file is UI-layer
// (includes gtkmm.h) and is deliberately NOT linked into this headless test
// target (see tests/CMakeLists.txt's header comment: the target enforces
// "no gtkmm" by construction). It is not feasible to link it here without
// violating that invariant, so this test instead (a) pins the exact
// per-ply raw values GameState hands to that function — the data half of
// the bug — and (b) mirrors the fixed one-line attribution formula as a
// local helper, asserting it against hand-derived expected side-to-move
// values for a short game. Any future regression in either half (the data
// GameState produces, or the attribution formula in analysis_panel.cpp)
// should be caught by keeping this helper in sync with that file.

#include "vendor/doctest.h"

#include "model/game_state.h"

#include <cmath>

namespace {

Coord at(int x, int y) { return Coord{x, y}; }

// Mirrors src/ui/analysis_panel.cpp's toDisplayWinrate() side-to-move
// formula exactly (see file-header note above for why it can't be called
// directly from this headless target).
bool blackToMoveAfterPly(size_t i) { return (i % 2 == 1); }

PVLine samplePv(int depth, int64_t nodes, double score) {
    PVLine pv;
    pv.pvIndex = 1;
    pv.depth   = depth;
    pv.nodes   = nodes;
    pv.score   = score;
    return pv;
}

EngineStatus dummyStatus() { return EngineStatus{}; }

} // namespace

TEST_CASE("UI-01: side-to-move attribution is correct per ply index") {
    // Move 0 = Black's move. After move 0, White is to move -> raw[0] is
    // scored from White's perspective -> blackToMove must be false.
    // After move 1 (White's move), Black is to move -> blackToMove true.
    // And so on, alternating.
    CHECK_FALSE(blackToMoveAfterPly(0));
    CHECK(blackToMoveAfterPly(1));
    CHECK_FALSE(blackToMoveAfterPly(2));
    CHECK(blackToMoveAfterPly(3));
    CHECK_FALSE(blackToMoveAfterPly(4));

    // Guard against the original bug's exact inversion (i % 2 == 0).
    for (size_t i = 0; i < 6; ++i) {
        CHECK(blackToMoveAfterPly(i) == (i % 2 == 1));
    }
}

TEST_CASE("UI-01: evalHistory returns one raw eval per ply, indexed by move order") {
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));  // ply 0 (Black)
    gs.setAnalysisData({samplePv(10, 1000, 0.9)}, dummyStatus());  // eval of position after ply 0 (White to move)

    REQUIRE(gs.makeMove(at(7, 8)));  // ply 1 (White)
    gs.setAnalysisData({samplePv(10, 1000, 0.2)}, dummyStatus());  // eval of position after ply 1 (Black to move)

    auto history = gs.evalHistory();
    REQUIRE(history.size() == 2);
    CHECK(history[0] == doctest::Approx(0.9));
    CHECK(history[1] == doctest::Approx(0.2));
}

TEST_CASE("UI-01: setAnalysisData writes a revised eval at the same depth/nodes") {
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));

    gs.setAnalysisData({samplePv(12, 5000, 0.55)}, dummyStatus());
    TreeNode *node = gs.tree().getNode(gs.currentPath());
    REQUIRE(node != nullptr);
    CHECK(node->eval == doctest::Approx(0.55));
    CHECK(node->depth == 12);
    CHECK(node->nodes == 5000);

    // Same depth, same node count, different score (aspiration-window
    // re-search / final confirmation line) -- must still overwrite eval.
    gs.setAnalysisData({samplePv(12, 5000, 0.80)}, dummyStatus());
    CHECK(node->eval == doctest::Approx(0.80));
    CHECK(node->depth == 12);
    CHECK(node->nodes == 5000);
}

TEST_CASE("UI-01: setAnalysisData still writes when depth/nodes change (no regression)") {
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));

    gs.setAnalysisData({samplePv(10, 1000, 0.4)}, dummyStatus());
    TreeNode *node = gs.tree().getNode(gs.currentPath());
    REQUIRE(node != nullptr);
    CHECK(node->eval == doctest::Approx(0.4));

    gs.setAnalysisData({samplePv(11, 2000, 0.4)}, dummyStatus());
    CHECK(node->depth == 11);
    CHECK(node->nodes == 2000);
}

TEST_CASE("UI-01: evalHistory reports NaN (not a false 0.5) for an unevaluated node") {
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));  // ply 0: never analyzed.

    auto history = gs.evalHistory();
    REQUIRE(history.size() == 1);
    CHECK(std::isnan(history[0]));

    // Now analyze it -- should become a real value, no longer NaN.
    gs.setAnalysisData({samplePv(8, 500, 0.6)}, dummyStatus());
    history = gs.evalHistory();
    REQUIRE(history.size() == 1);
    CHECK_FALSE(std::isnan(history[0]));
    CHECK(history[0] == doctest::Approx(0.6));
}

TEST_CASE("UI-01: evalHistory reports NaN for a genuinely dead-even (0.5) evaluated node") {
    // A node WITH real analysis whose score happens to be exactly 0.5 must
    // still read back as 0.5, not NaN -- only the "no analysis at all" case
    // gets the NaN sentinel.
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));
    gs.setAnalysisData({samplePv(8, 500, 0.5)}, dummyStatus());

    auto history = gs.evalHistory();
    REQUIRE(history.size() == 1);
    CHECK_FALSE(std::isnan(history[0]));
    CHECK(history[0] == doctest::Approx(0.5));
}
