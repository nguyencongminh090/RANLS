// Regression tests for STATE-01: stale PV/engine-status data surviving a
// position change (New Game, a move, undo, redo, gotoMove, gotoPath).
//
// GameState::resetAnalysisState() is the single shared path that clears
// pvLines_/engineStatus_ and emits signal_engine_analysis on every operation
// that changes the current position. These tests pin that invariant per
// operation so a future position-changing op added without wiring the reset
// call fails loudly here instead of silently reintroducing stale-analysis
// bugs in the UI.

#include "vendor/doctest.h"

#include "model/game_state.h"

namespace {

Coord at(int x, int y) { return Coord{x, y}; }

std::vector<PVLine> samplePvLines()
{
    PVLine pv;
    pv.pvIndex = 1;
    pv.depth   = 12;
    pv.nodes   = 12345;
    pv.score   = 0.75;
    pv.moves   = {at(7, 7)};
    return {pv};
}

EngineStatus sampleStatus()
{
    EngineStatus s;
    s.depth    = 12;
    s.selDepth = 14;
    s.nodes    = 12345;
    s.nps      = 999;
    s.timeMs   = 500;
    s.winrate  = 0.75;
    s.evalText = "+0.75";
    s.bestMove = at(7, 7);
    return s;
}

bool isDefaultStatus(const EngineStatus &s)
{
    EngineStatus def;
    return s.depth == def.depth && s.selDepth == def.selDepth && s.nodes == def.nodes
        && s.nps == def.nps && s.timeMs == def.timeMs && s.winrate == def.winrate
        && s.mateStep == def.mateStep && s.evalText == def.evalText
        && s.bestMove == def.bestMove;
}

} // namespace

TEST_CASE("GameState: makeMove -> undoMove clears pvLines and engineStatus") {
    GameState gs;
    gs.setAnalysisData(samplePvLines(), sampleStatus());
    REQUIRE_FALSE(gs.pvLines().empty());

    REQUIRE(gs.makeMove(at(7, 7)));
    // makeMove itself must also clear the previous position's analysis.
    CHECK(gs.pvLines().empty());
    CHECK(isDefaultStatus(gs.engineStatus()));

    // Re-populate to simulate the engine having analyzed the new position.
    gs.setAnalysisData(samplePvLines(), sampleStatus());
    REQUIRE_FALSE(gs.pvLines().empty());

    REQUIRE(gs.undoMove());
    CHECK(gs.pvLines().empty());
    CHECK(isDefaultStatus(gs.engineStatus()));
}

TEST_CASE("GameState: setAnalysisData -> newGame clears data and fires signal_engine_analysis") {
    GameState gs;
    gs.setAnalysisData(samplePvLines(), sampleStatus());
    REQUIRE_FALSE(gs.pvLines().empty());

    bool signalFired = false;
    gs.signal_engine_analysis.connect([&]() { signalFired = true; });

    gs.newGame();

    CHECK(gs.pvLines().empty());
    CHECK(isDefaultStatus(gs.engineStatus()));
    CHECK(signalFired);
}

TEST_CASE("GameState: loadPosition clears stale analysis data") {
    GameState gs;
    gs.setAnalysisData(samplePvLines(), sampleStatus());

    bool signalFired = false;
    gs.signal_engine_analysis.connect([&]() { signalFired = true; });

    REQUIRE(gs.loadPosition({{at(3, 3), Stone::Black}, {at(3, 4), Stone::White}}));

    CHECK(gs.pvLines().empty());
    CHECK(isDefaultStatus(gs.engineStatus()));
    CHECK(signalFired);
}

TEST_CASE("GameState: redoMove clears stale analysis data") {
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));
    REQUIRE(gs.undoMove());

    gs.setAnalysisData(samplePvLines(), sampleStatus());
    REQUIRE_FALSE(gs.pvLines().empty());

    bool signalFired = false;
    gs.signal_engine_analysis.connect([&]() { signalFired = true; });

    REQUIRE(gs.redoMove());

    CHECK(gs.pvLines().empty());
    CHECK(isDefaultStatus(gs.engineStatus()));
    CHECK(signalFired);
}

TEST_CASE("GameState: gotoMove clears stale analysis data") {
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));
    REQUIRE(gs.makeMove(at(7, 8)));

    gs.setAnalysisData(samplePvLines(), sampleStatus());
    REQUIRE_FALSE(gs.pvLines().empty());

    bool signalFired = false;
    gs.signal_engine_analysis.connect([&]() { signalFired = true; });

    gs.gotoMove(0);

    CHECK(gs.pvLines().empty());
    CHECK(isDefaultStatus(gs.engineStatus()));
    CHECK(signalFired);
}

TEST_CASE("GameState: gotoPath clears stale analysis data") {
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));
    REQUIRE(gs.makeMove(at(7, 8)));
    REQUIRE(gs.undoMove());
    REQUIRE(gs.undoMove());

    gs.setAnalysisData(samplePvLines(), sampleStatus());
    REQUIRE_FALSE(gs.pvLines().empty());

    bool signalFired = false;
    gs.signal_engine_analysis.connect([&]() { signalFired = true; });

    REQUIRE(gs.gotoPath({at(7, 7), at(7, 8)}));

    CHECK(gs.pvLines().empty());
    CHECK(isDefaultStatus(gs.engineStatus()));
    CHECK(signalFired);
}

TEST_CASE("GameState: resetAnalysisState is idempotent, no spurious re-emission when already empty") {
    GameState gs;
    // Fresh GameState already has empty pvLines/default engineStatus.
    REQUIRE(gs.pvLines().empty());

    int fireCount = 0;
    gs.signal_engine_analysis.connect([&]() { ++fireCount; });

    // makeMove -> undoMove -> undoMove(no-op) chain: analysis state was
    // already empty throughout, so the shared reset helper must not fire
    // signal_engine_analysis on every call (see undoAll/redoAll flood concern
    // in NAV-01) when there's nothing to clear.
    REQUIRE(gs.makeMove(at(7, 7)));
    REQUIRE(gs.undoMove());
    CHECK_FALSE(gs.undoMove()); // nothing left to undo -> no-op

    CHECK(fireCount == 0);
}

TEST_CASE("GameState: analysis data is NOT cleared while analyzing the same position") {
    GameState gs;
    gs.setAnalysisData(samplePvLines(), sampleStatus());
    REQUIRE_FALSE(gs.pvLines().empty());

    gs.setAnalyzing(true);

    // Position-changing operations all guard on analyzing_ and must refuse to
    // run (and therefore must not touch pvLines_/engineStatus_) while an
    // in-flight search on the current position is running.
    CHECK_FALSE(gs.makeMove(at(7, 7)));
    CHECK_FALSE(gs.undoMove());
    CHECK_FALSE(gs.redoMove());
    CHECK_FALSE(gs.loadPosition({{at(3, 3), Stone::Black}}));
    CHECK_FALSE(gs.gotoPath({at(7, 7)}));

    CHECK_FALSE(gs.pvLines().empty());
    CHECK(gs.engineStatus().depth == 12);

    gs.setAnalyzing(false);
}
