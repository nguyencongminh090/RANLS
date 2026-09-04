// Regression tests for ANLZ-05 (model / engine layer, no gtkmm).
//
// ANLZ-05 lets a board click during an in-flight Analyze-Mode search place the
// stone. The fix is at the MainWindow layer (the click handler calls
// controller_.stopAnalysis() before gameState_.makeMove()) — GameState's
// `if (analyzing_) return false;` guard in makeMove() is deliberately NOT
// weakened. These tests pin the two facts the click handler relies on:
//
//   1. while analyzing_ is set, makeMove() still returns false (guard intact);
//   2. after EngineController::stopAnalysis(), analyzing_ is clear and the very
//      next makeMove() succeeds — i.e. the stop-then-move sequence works.
//
// See docs/fix-log/2026-09-04-anlz05-analyze-mode-no-automove-mid-search-click.md.

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

// Stand-in "engine": /bin/cat stays alive reading stdin as long as its pipe is
// open. EngineController::startEngine() is optimistic (→ Idle, see ENG-01), and
// analyze()/stopAnalysis() only need a running, Idle/Analyzing process for their
// state bookkeeping. The board is kept empty in the process-backed case so the
// only lines sent are keyword commands (YXBOARD/DONE/YXNBEST/STOP) — cat echoes
// them straight back but none are coordinate-shaped, so nothing is parsed as an
// engine move.
constexpr const char *kFakeEngine = "/bin/cat";

} // namespace

TEST_CASE("ANLZ-05: makeMove() still refuses while analyzing_ is set (guard unchanged)")
{
    GameState gs;
    REQUIRE(gs.makeMove(at(7, 7)));

    gs.setAnalyzing(true);
    CHECK_FALSE(gs.makeMove(at(8, 8)));   // swallowed — the model invariant holds
    CHECK(gs.history().moveCount() == 1);

    gs.setAnalyzing(false);
    CHECK(gs.makeMove(at(8, 8)));         // clears once the flag is down
    CHECK(gs.history().moveCount() == 2);
}

TEST_CASE("ANLZ-05: stopAnalysis() then makeMove() succeeds — the click-handler sequence")
{
    GameState gs;
    EngineProcess proc;
    EngineController ctrl(gs, proc);

    EngineConfig cfg = gs.engineConfig();
    cfg.enginePath = kFakeEngine;
    gs.setEngineConfig(cfg);

    ctrl.startEngine();
    REQUIRE(ctrl.engineState() == EngineController::EngineState::Idle);
    REQUIRE(proc.isRunning());

    // Analyze-Mode search running (empty board): analyzing_ set, clicks swallowed.
    ctrl.analyze();
    CHECK(ctrl.engineState() == EngineController::EngineState::Analyzing);
    CHECK(gs.isAnalyzing());
    CHECK_FALSE(gs.makeMove(at(7, 7)));
    CHECK(gs.history().moveCount() == 0);

    // The click handler stops the search first — synchronous, per
    // EngineController::stopAnalysis().
    ctrl.stopAnalysis();
    CHECK(ctrl.engineState() == EngineController::EngineState::Idle);
    CHECK_FALSE(gs.isAnalyzing());

    // ...and now the move lands.
    CHECK(gs.makeMove(at(7, 7)));
    CHECK(gs.history().moveCount() == 1);

    ctrl.stopEngine([] {});
    pumpUntil([&] { return !proc.isRunning(); });
}
