// Regression tests for ANLZ-01: "Analyze Mode" — continuous background analysis.
//
// The feature itself is orchestration in MainWindow (react to
// signal_board_changed; when Analyze Mode is on + engine Idle + not the
// engine's turn, stopAnalysis() then analyze()). What is model-testable here is
// the *outcome* that orchestration produces: an engine search rooted at every
// visited position fills GameState's evalHistory() with a real (non-NaN) point
// for every ply — whereas the pre-ANLZ-01 flow (search only at a couple of
// positions) leaves NaN gaps.
//
// This drives GameState + a fake analysis feed directly (no EngineController,
// no gtkmm), simulating what continuous re-analyse does: gotoMove(i) then
// setAnalysisData() with a search rooted at ply i, for every played ply.
//
// See docs/fix-log/2026-09-04-analyze-mode.md for the before/after trace.

#include "vendor/doctest.h"

#include "model/game_state.h"

#include <cmath>

namespace {

Coord at(int x, int y) { return Coord{x, y}; }

// A PV rooted at the current position. moves is left empty so this feed touches
// ONLY the search-root node (no UI-13 child-fill) — that keeps the "mode off"
// case a clean NaN-gap test.
PVLine rootPv(int depth, int64_t nodes, double score) {
    PVLine pv;
    pv.pvIndex = 1;
    pv.depth   = depth;
    pv.nodes   = nodes;
    pv.score   = score;
    return pv;
}

EngineStatus dummyStatus() { return EngineStatus{}; }

// The six-move repro line from the original screenshot flow.
const Coord kLine[] = {at(7, 7), at(7, 8), at(8, 8), at(8, 7), at(9, 9), at(9, 8)};

GameState playLine() {
    GameState gs;
    for (const auto &m : kLine)
        REQUIRE(gs.makeMove(m));
    return gs;
}

} // namespace

TEST_CASE("ANLZ-01: continuous re-analyse fills evalHistory with no NaN for any visited ply") {
    GameState gs = playLine();

    // Analyze Mode ON: the engine is re-rooted at every position the reviewer
    // walks through, each search writing that position's real eval.
    for (int i = 0; i < 6; ++i) {
        gs.gotoMove(i);
        gs.setAnalysisData({rootPv(12 + i, 4000, 0.5 + 0.03 * i)}, dummyStatus());
    }

    gs.gotoMove(5);
    auto hist = gs.evalHistory();
    REQUIRE(hist.size() == 6);
    for (int i = 0; i < 6; ++i)
        CHECK_FALSE(std::isnan(hist[i]));

    // The points are the measured values, not a formula estimate.
    CHECK(hist[0] == doctest::Approx(0.50));
    CHECK(hist[3] == doctest::Approx(0.59));
}

TEST_CASE("ANLZ-01: without the continuous feed the gaps remain (mode-off baseline)") {
    GameState gs = playLine();

    // Analyze Mode OFF: the pre-ANLZ-01 flow — the reviewer only pressed
    // "Analyze" at plies 0 and 2.
    gs.gotoMove(0);
    gs.setAnalysisData({rootPv(12, 4000, 0.60)}, dummyStatus());
    gs.gotoMove(2);
    gs.setAnalysisData({rootPv(12, 4000, 0.55)}, dummyStatus());

    gs.gotoMove(5);
    auto hist = gs.evalHistory();
    REQUIRE(hist.size() == 6);

    // Scored directly:
    CHECK_FALSE(std::isnan(hist[0]));
    CHECK_FALSE(std::isnan(hist[2]));
    // Never searched — genuine NaN gaps (UI-01), which is exactly what
    // Analyze Mode exists to eliminate. This distinguishes the two modes.
    CHECK(std::isnan(hist[1]));
    CHECK(std::isnan(hist[3]));
    CHECK(std::isnan(hist[4]));
    CHECK(std::isnan(hist[5]));
}

TEST_CASE("ANLZ-01: ViewConfig::analyzeMode defaults off and round-trips through GameState") {
    GameState gs;
    CHECK_FALSE(gs.viewConfig().analyzeMode);

    ViewConfig vc = gs.viewConfig();
    vc.analyzeMode = true;
    gs.setViewConfig(vc);
    CHECK(gs.viewConfig().analyzeMode);
}
