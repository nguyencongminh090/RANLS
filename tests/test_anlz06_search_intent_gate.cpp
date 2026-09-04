// Regression tests for ANLZ-06 (model / engine layer, no gtkmm).
//
// ANLZ-05 promised that pressing Stop during an Analyze-Mode search never
// places a stone, and that a board click mid-search places exactly the
// user's stone — but neither was actually enforced: EngineController relayed
// EVERY inbound engine coordinate line to signal_engine_move unconditionally,
// including the best-move line analyze()'s YXNBEST search always emits on
// completion. The ANLZ-05 test (test_anlz05_stop_then_move.cpp) only ever
// fed outbound protocol lines through a real /bin/cat "engine" that never
// echoes a coordinate-shaped line back — so it could not catch this.
//
// This suite closes that gap by feeding INBOUND coordinate lines directly
// through EngineProcess::signal_line_received (a public signal EngineProcess
// already exposes; emitting on it is exactly what a real engine's stdout
// reader would do) and asserting on EngineController's reaction.
//
// See docs/fix-log/2026-09-04-analyze-mode-search-plays-stray-move.md.

#include "vendor/doctest.h"

#include "engine/engine_controller.h"
#include "engine/engine_process.h"
#include "model/game_state.h"

#include <glibmm.h>

#include <chrono>

namespace {

Coord at(int x, int y) { return Coord{x, y}; }

bool pumpUntil(const std::function<bool()> &done, int timeoutMs = 3000)
{
    auto *ctx = g_main_context_default();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!done()) {
        g_main_context_iteration(ctx, FALSE);
        if (std::chrono::steady_clock::now() >= deadline) return false;
        g_usleep(1000);
    }
    return true;
}

// Stand-in "engine": /bin/cat stays alive reading stdin as long as its pipe
// is open (same rationale as test_anlz05_stop_then_move.cpp). Real inbound
// traffic is simulated separately by emitting directly on
// EngineProcess::signal_line_received — cat itself never produces a
// coordinate-shaped line, so nothing it echoes back interferes with the
// assertions below.
constexpr const char *kFakeEngine = "/bin/cat";

struct Fixture {
    GameState gs;
    EngineProcess proc;
    EngineController ctrl {gs, proc};
    int moveCount = 0;
    Coord lastMove;

    Fixture()
    {
        ctrl.signal_engine_move.connect([this](Coord c) {
            ++moveCount;
            lastMove = c;
        });

        EngineConfig cfg = gs.engineConfig();
        cfg.enginePath = kFakeEngine;
        gs.setEngineConfig(cfg);

        ctrl.startEngine();
        REQUIRE(ctrl.engineState() == EngineController::EngineState::Idle);
        REQUIRE(proc.isRunning());
    }

    ~Fixture()
    {
        ctrl.stopEngine([] {});
        pumpUntil([&] { return !proc.isRunning(); });
    }
};

} // namespace

TEST_CASE("ANLZ-06: analyze()'s search-completion coordinate is discarded, not played")
{
    Fixture f;

    f.ctrl.analyze();
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Analyzing);
    REQUIRE(f.gs.isAnalyzing());

    // The engine's stdout reader would deliver this exactly like this.
    f.proc.signal_line_received.emit("7,7");

    CHECK(f.moveCount == 0);
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);
    CHECK_FALSE(f.gs.isAnalyzing());
    // UI-13: the searched position's analysis was flushed (setAnalyzing(false)
    // + flush() both ran) even though no move was played.
    CHECK(f.gs.history().moveCount() == 0);
}

TEST_CASE("ANLZ-06: requestEngineMove()'s coordinate still plays exactly once (ENG-02/UI-06 unchanged)")
{
    Fixture f;

    f.ctrl.requestEngineMove();
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Analyzing);

    f.proc.signal_line_received.emit("7,7");

    CHECK(f.moveCount == 1);
    CHECK(f.lastMove.x == 7);
    CHECK(f.lastMove.y == 7);
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);
}

TEST_CASE("ANLZ-06: a late coordinate after stopAnalysis() is inert (intent already reset)")
{
    Fixture f;

    f.ctrl.analyze();
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Analyzing);

    f.ctrl.stopAnalysis();
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);
    CHECK_FALSE(f.gs.isAnalyzing());

    // The aborted search's trailing coordinate line arrives after the stop.
    f.proc.signal_line_received.emit("7,7");

    CHECK(f.moveCount == 0);
    CHECK(f.gs.history().moveCount() == 0);
}

TEST_CASE("ANLZ-06: regression — stop, user move, then late engine coord must not double-move")
{
    Fixture f;

    // Position a1 a2 a3, White to move (mirrors the reported repro).
    REQUIRE(f.gs.makeMove(at(0, 0))); // a1 (Black)
    REQUIRE(f.gs.makeMove(at(1, 0))); // a2 (White)
    REQUIRE(f.gs.makeMove(at(2, 0))); // a3 (Black)
    REQUIRE(f.gs.history().moveCount() == 3);

    f.ctrl.analyze();
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Analyzing);

    // User clicks an empty point: click handler stops the search first...
    f.ctrl.stopAnalysis();
    CHECK_FALSE(f.gs.isAnalyzing());

    // ...then plays their own move (a4).
    REQUIRE(f.gs.makeMove(at(3, 0))); // a4
    CHECK(f.gs.history().moveCount() == 4);

    // The interrupted search's best move (b1) arrives late.
    f.proc.signal_line_received.emit("0,1"); // row=0,col=1 -> Coord{1,0} = b1

    CHECK(f.moveCount == 0);
    CHECK(f.gs.history().moveCount() == 4); // still only the user's a4 — no b1
}
