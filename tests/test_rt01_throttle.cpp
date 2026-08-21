// Regression tests for RT-01: throttle/coalesce the realtime analysis signal
// path (src/model/game_state.h/.cpp GameState::setAnalysisData/tickAnalysis/
// flush/evalHistory).
//
// Before this change, GameState::setAnalysisData emitted signal_engine_analysis
// unconditionally, once per call -- 1:1 with GomocupProtocol::signal_analysis.
// With multiPV=8 that meant 8 full UI rebuilds per depth iteration. These
// tests replay a canned engine-output sequence through the real
// GomocupProtocol parser (same pattern as test_gomocup_protocol.cpp) wired to
// a real GameState (same pattern as test_game_state.cpp), and count both the
// wire-level signal_analysis emissions and the UI-facing
// signal_engine_analysis emissions to pin the actual coalescing ratio.

#include "vendor/doctest.h"

#include "model/game_state.h"
#include "engine/gomocup_protocol.h"

#include <string>

namespace {

/// Wires a GomocupProtocol to a GameState the same way
/// EngineController::connectProtocolSignals does, and counts emissions on
/// both sides of the throttle.
struct Harness {
    GomocupProtocol proto;
    GameState       gs;
    int protocolAnalysisCount = 0; ///< signal_analysis emissions (wire-level, unthrottled)
    int uiAnalysisCount       = 0; ///< signal_engine_analysis emissions (throttled)

    explicit Harness(int boardSize = 15) : proto(boardSize), gs(boardSize) {
        proto.signal_analysis.connect(
            [this](const std::vector<PVLine> &pvs, const EngineStatus &status) {
                ++protocolAnalysisCount;
                gs.setAnalysisData(pvs, status);
            });
        gs.signal_engine_analysis.connect([this]() { ++uiAnalysisCount; });
    }

    /// Feeds one full PV commit for multiPV slot `idx` at `depth`, the same
    /// INFO-line shape a real engine sends per PV per depth iteration
    /// (mirrors "well-formed INFO PV n / PV DONE sequence" in
    /// test_gomocup_protocol.cpp). Triggers exactly one signal_analysis
    /// emission via GomocupProtocol::onPVDone().
    void feedPvCommit(int idx, int depth) {
        proto.parseLine("INFO PV " + std::to_string(idx));
        proto.parseLine("INFO DEPTH " + std::to_string(depth));
        proto.parseLine("INFO EVAL 50");
        proto.parseLine("INFO BESTLINE 7,7 8,8");
        proto.parseLine("INFO PV DONE");
    }

    /// Feeds one full depth iteration across `numPV` multiPV slots.
    void feedDepthIteration(int numPV, int depth) {
        proto.parseLine("INFO NUMPV " + std::to_string(numPV));
        for (int i = 0; i < numPV; ++i)
            feedPvCommit(i, depth);
    }
};

} // namespace

TEST_CASE("RT-01: multiPV=1, 20 depth iterations - measured signal counts") {
    Harness h;
    const int depths = 20;
    for (int d = 1; d <= depths; ++d)
        h.feedDepthIteration(1, d);

    // Wire-level signal still fires once per PV commit, unthrottled -- RT-01
    // explicitly requires GomocupProtocol to keep emitting per line/commit
    // since other consumers (EngineController/BottomPanel) may want every one.
    CHECK(h.protocolAnalysisCount == depths); // 1 PV * 20 depths = 20

    // Before RT-01: GameState::setAnalysisData emitted signal_engine_analysis
    // unconditionally at the end of every call, so uiAnalysisCount would be
    // 20 here too (1:1 with protocolAnalysisCount). After RT-01, nothing is
    // emitted until a tick/flush consumes the dirty flag.
    CHECK(h.uiAnalysisCount == 0);

    // One tick coalesces the whole burst into exactly one UI update, and
    // delivers the most recent data (depth 20), not a stale one.
    CHECK(h.gs.tickAnalysis());
    CHECK(h.uiAnalysisCount == 1);
    CHECK(h.gs.engineStatus().depth == depths);

    // A second tick with nothing new pending is a no-op -- must not re-emit.
    CHECK_FALSE(h.gs.tickAnalysis());
    CHECK(h.uiAnalysisCount == 1);
}

TEST_CASE("RT-01: multiPV=8, 20 depth iterations - measured signal counts (8x commitPV storm)") {
    Harness h;
    const int depths = 20;
    const int numPV  = 8;
    for (int d = 1; d <= depths; ++d)
        h.feedDepthIteration(numPV, d);

    // Wire-level count: 8 PV commits * 20 depths = 160 -- this is exactly the
    // "8 full-UI rebuilds per depth iteration" the todo describes, measured
    // at the protocol layer (still honest/unthrottled, as required).
    CHECK(h.protocolAnalysisCount == numPV * depths); // 160

    // Before RT-01: uiAnalysisCount would also be 160 here (1:1, unconditional
    // emit -- see the removed unconditional `signal_engine_analysis.emit()`
    // previously at the end of GameState::setAnalysisData).
    // After RT-01: still zero until a tick/flush fires -- an 8x-per-depth
    // storm collapses to nothing until explicitly consumed.
    CHECK(h.uiAnalysisCount == 0);

    CHECK(h.gs.tickAnalysis());
    CHECK(h.uiAnalysisCount == 1); // 160 -> 1: the actual coalescing ratio for this workload
    CHECK(h.gs.pvLines().size() == static_cast<size_t>(numPV));
    CHECK(h.gs.engineStatus().depth == depths);
}

TEST_CASE("RT-01: flush() delivers the final update immediately without waiting for a tick") {
    Harness h;
    h.feedDepthIteration(8, 5);
    CHECK(h.uiAnalysisCount == 0);

    // Mirrors EngineController calling gameState_.flush() on search
    // completion (protocol_->signal_move handler) / analysis-stopped
    // (stopAnalysis()) -- must not wait for the next periodic tick.
    h.gs.flush();
    CHECK(h.uiAnalysisCount == 1);
    CHECK(h.gs.engineStatus().depth == 5);

    // flush() with nothing pending is a no-op (must not double-emit).
    h.gs.flush();
    CHECK(h.uiAnalysisCount == 1);
}

TEST_CASE("RT-01: last update is never dropped -- burst then silence still delivers final state") {
    Harness h;
    // Classic throttle bug this guards against: data arrives, a tick already
    // consumed an earlier burst, the engine goes quiet, and the final result
    // never renders because nothing is scheduled to pick it up. Here we
    // drive tickAnalysis() manually (standing in for the real UI timer) and
    // assert the dirty flag stays armed across quiet periods.
    h.feedDepthIteration(4, 1);
    CHECK(h.gs.tickAnalysis()); // first tick consumes the depth-1 burst
    CHECK(h.uiAnalysisCount == 1);

    // Engine goes quiet -- ticks with nothing new pending must not re-emit.
    CHECK_FALSE(h.gs.tickAnalysis());
    CHECK_FALSE(h.gs.tickAnalysis());
    CHECK(h.uiAnalysisCount == 1);

    // One final burst arrives (e.g. the last depth before search completion).
    h.feedDepthIteration(4, 2);
    CHECK(h.uiAnalysisCount == 1); // not emitted yet -- waiting on the next tick/flush

    // The next tick (in the real app: the periodic UI timer, or flush() from
    // EngineController on search completion/stop) must deliver it -- not drop it.
    CHECK(h.gs.tickAnalysis());
    CHECK(h.uiAnalysisCount == 2);
    CHECK(h.gs.engineStatus().depth == 2);
}

TEST_CASE("RT-01: evalHistory() is cached and only recomputed when the tree/board actually changes") {
    GameState gs;
    REQUIRE(gs.makeMove(Coord{7, 7}));

    auto first  = gs.evalHistory();
    auto second = gs.evalHistory(); // same tree/board state -- must be the cached value
    CHECK(first == second);
    REQUIRE(first.size() == 1);

    // Feeding analysis data that changes the current node's eval/depth/nodes
    // must invalidate the cache so the next read reflects the new value --
    // otherwise the throttle would just move the recompute cost around
    // instead of eliminating it (see instruction.md pitfalls).
    PVLine pv;
    pv.pvIndex = 1;
    pv.depth   = 10;
    pv.nodes   = 100;
    pv.score   = 0.9;
    pv.moves   = {Coord{7, 7}};
    EngineStatus status;
    status.depth = 10;
    gs.setAnalysisData({pv}, status);

    auto third = gs.evalHistory();
    REQUIRE(third.size() == 1);
    CHECK(third[0] == doctest::Approx(0.9));
}
