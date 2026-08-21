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

// ─── NAV-01: bulk navigation must not flood signal_board_changed ───────────
//
// undoAll/redoAll/gotoMove used to be loops over the single-step undoMove()/
// redoMove(), each of which independently emitted signal_board_changed (and
// called clearDatabase(), which emits signal_database_updated). On an N-move
// game that meant N emissions of both signals for one bulk-navigation click.
// These tests pin "exactly one emission of each, regardless of move count"
// so a future regression that reintroduces the per-ply loop fails loudly.

namespace {

/// Builds a GameState with `count` moves played (row-major sweep across the
/// board — GameState::makeMove only checks occupancy/bounds, not adjacency
/// or gomoku rules, so this is a valid way to build a long game for a test).
GameState makeGameWithMoves(int count)
{
    GameState gs;
    int size = gs.boardSize();
    int placed = 0;
    for (int y = 0; y < size && placed < count; ++y) {
        for (int x = 0; x < size && placed < count; ++x) {
            REQUIRE(gs.makeMove(at(x, y)));
            ++placed;
        }
    }
    REQUIRE(gs.history().moveCount() == count);
    return gs;
}

} // namespace

TEST_CASE("NAV-01: undoAll on a 120-move game fires signal_board_changed exactly once") {
    GameState gs = makeGameWithMoves(120);

    int boardChangedCount    = 0;
    int databaseUpdatedCount = 0;
    gs.signal_board_changed.connect([&]() { ++boardChangedCount; });
    gs.signal_database_updated.connect([&]() { ++databaseUpdatedCount; });

    gs.undoAll();

    CHECK(gs.history().moveCount() == 0);
    CHECK(boardChangedCount == 1);
    // clearDatabase() is what triggers the engine database query in
    // main_window.cpp; exactly one call for the whole bulk op means exactly
    // one query, not one per undone ply.
    CHECK(databaseUpdatedCount == 1);
}

TEST_CASE("NAV-01: redoAll on a 120-move game fires signal_board_changed exactly once") {
    GameState gs = makeGameWithMoves(120);
    gs.undoAll();
    REQUIRE(gs.history().moveCount() == 0);

    int boardChangedCount    = 0;
    int databaseUpdatedCount = 0;
    gs.signal_board_changed.connect([&]() { ++boardChangedCount; });
    gs.signal_database_updated.connect([&]() { ++databaseUpdatedCount; });

    gs.redoAll();

    CHECK(gs.history().moveCount() == 120);
    CHECK(boardChangedCount == 1);
    CHECK(databaseUpdatedCount == 1);
}

TEST_CASE("NAV-01: undoAll/redoAll on an empty/already-fully-redone game emit nothing") {
    GameState gs;

    int boardChangedCount = 0;
    gs.signal_board_changed.connect([&]() { ++boardChangedCount; });

    gs.undoAll(); // nothing to undo
    gs.redoAll(); // nothing to redo

    CHECK(boardChangedCount == 0);
}

TEST_CASE("NAV-01: gotoMove across many plies fires signal_board_changed exactly once") {
    GameState gs = makeGameWithMoves(120);
    // Jump backward from move 120 to move 10 — 110 plies of undo collapsed
    // into one bulk op.
    gs.gotoMove(10);
    REQUIRE(gs.history().currentIndex() == 10);

    int boardChangedCount    = 0;
    int databaseUpdatedCount = 0;
    int moveSelectedCount    = 0;
    int lastSelectedIndex    = -1;
    gs.signal_board_changed.connect([&]() { ++boardChangedCount; });
    gs.signal_database_updated.connect([&]() { ++databaseUpdatedCount; });
    gs.signal_move_selected.connect([&](int idx) { ++moveSelectedCount; lastSelectedIndex = idx; });

    // Jump forward from move 10 to move 100 — 90 plies of redo.
    gs.gotoMove(100);

    CHECK(gs.history().currentIndex() == 100);
    CHECK(boardChangedCount == 1);
    CHECK(databaseUpdatedCount == 1);
    CHECK(moveSelectedCount == 1);
    CHECK(lastSelectedIndex == 100);
}

TEST_CASE("NAV-01: gotoMove to the already-current move emits signal_move_selected but not signal_board_changed") {
    GameState gs = makeGameWithMoves(10);

    int boardChangedCount = 0;
    int moveSelectedCount = 0;
    gs.signal_board_changed.connect([&]() { ++boardChangedCount; });
    gs.signal_move_selected.connect([&](int) { ++moveSelectedCount; });

    gs.gotoMove(gs.history().currentIndex());

    CHECK(boardChangedCount == 0);
    CHECK(moveSelectedCount == 1);
}

TEST_CASE("NAV-01: gotoPath on a long path fires signal_board_changed exactly once") {
    // gotoPath only jumps to a path that already exists in the variation
    // tree, so first play the moves (populating the tree), then undo back to
    // the start, then gotoPath to the far position in one bulk call.
    GameState gs = makeGameWithMoves(100);
    std::vector<Coord> path = gs.currentPath();
    gs.undoAll();
    REQUIRE(gs.history().moveCount() == 0);

    int boardChangedCount    = 0;
    int databaseUpdatedCount = 0;
    gs.signal_board_changed.connect([&]() { ++boardChangedCount; });
    gs.signal_database_updated.connect([&]() { ++databaseUpdatedCount; });

    REQUIRE(gs.gotoPath(path));

    CHECK(gs.history().moveCount() == 100);
    CHECK(boardChangedCount == 1);
    CHECK(databaseUpdatedCount == 1);
}
