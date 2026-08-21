// Regression tests for RT-04: signal_tree_updated must not be emitted at
// analysis-update frequency for a per-tick eval/depth/nodes refresh of the
// current tree node (src/model/game_state.h/.cpp GameState::setAnalysisData/
// tickAnalysis/flush). Before this change, setAnalysisData emitted
// signal_tree_updated synchronously and unconditionally whenever the current
// node's depth or nodes changed -- essentially once per parsed engine line,
// driving a full rebuild of both tree views (TreeExplorer, TreeNodeView) at
// that frequency. This reuses RT-01's dirty-flag/tick mechanism (treeDirty_)
// instead of a second one, coalescing the tree-view refresh onto the same
// ~75ms tick as signal_engine_analysis.
//
// Mirrors the harness pattern in test_rt01_throttle.cpp.

#include "vendor/doctest.h"

#include "model/game_state.h"
#include "engine/gomocup_protocol.h"

#include <string>

namespace {

struct Harness {
    GomocupProtocol proto;
    GameState       gs;
    int treeUpdatedCount = 0; ///< signal_tree_updated emissions

    explicit Harness(int boardSize = 15) : proto(boardSize), gs(boardSize) {
        proto.signal_analysis.connect(
            [this](const std::vector<PVLine> &pvs, const EngineStatus &status) {
                gs.setAnalysisData(pvs, status);
            });
        gs.signal_tree_updated.connect([this]() { ++treeUpdatedCount; });
    }

    void feedPvCommit(int idx, int depth) {
        proto.parseLine("INFO PV " + std::to_string(idx));
        proto.parseLine("INFO DEPTH " + std::to_string(depth));
        proto.parseLine("INFO EVAL 50");
        proto.parseLine("INFO BESTLINE 7,7 8,8");
        proto.parseLine("INFO PV DONE");
    }

    void feedDepthIteration(int numPV, int depth) {
        proto.parseLine("INFO NUMPV " + std::to_string(numPV));
        for (int i = 0; i < numPV; ++i)
            feedPvCommit(i, depth);
    }
};

} // namespace

TEST_CASE("RT-04: analysis bursts that change the current node's depth/nodes "
          "don't emit signal_tree_updated until a tick/flush consumes them") {
    Harness h;
    REQUIRE(h.gs.makeMove(Coord{7, 7})); // give the tree a current node to update

    // makeMove() is itself a structural change (see the last TEST_CASE below)
    // and emits signal_tree_updated synchronously once -- that's the baseline
    // every count below is relative to, not part of the per-tick coalescing
    // this test is actually checking.
    const int baseline = h.treeUpdatedCount;
    REQUIRE(baseline == 1);

    const int depths = 20;
    for (int d = 1; d <= depths; ++d)
        h.feedDepthIteration(1, d);

    // Before RT-04: setAnalysisData emitted signal_tree_updated synchronously
    // whenever depth/nodes changed, so this would already be baseline+20 (one
    // per depth iteration -- the "full rebuild many times per second" bug).
    CHECK(h.treeUpdatedCount == baseline);

    // One tick coalesces the whole burst into exactly one tree-view refresh.
    CHECK(h.gs.tickAnalysis());
    CHECK(h.treeUpdatedCount == baseline + 1);

    // A second tick with nothing new pending must not re-emit.
    CHECK_FALSE(h.gs.tickAnalysis());
    CHECK(h.treeUpdatedCount == baseline + 1);
}

TEST_CASE("RT-04: multiPV=8 storm still collapses to one signal_tree_updated per tick") {
    Harness h;
    REQUIRE(h.gs.makeMove(Coord{7, 7}));
    const int baseline = h.treeUpdatedCount; // makeMove's own synchronous emission

    const int depths = 10;
    const int numPV  = 8;
    for (int d = 1; d <= depths; ++d)
        h.feedDepthIteration(numPV, d);

    CHECK(h.treeUpdatedCount == baseline);
    CHECK(h.gs.tickAnalysis());
    CHECK(h.treeUpdatedCount == baseline + 1); // 80 PV commits -> 1 tree refresh
}

TEST_CASE("RT-04: flush() delivers a pending tree update immediately, "
          "without waiting for the next tick") {
    Harness h;
    REQUIRE(h.gs.makeMove(Coord{7, 7}));
    const int baseline = h.treeUpdatedCount; // makeMove's own synchronous emission

    h.feedDepthIteration(1, 5);
    CHECK(h.treeUpdatedCount == baseline);

    h.gs.flush();
    CHECK(h.treeUpdatedCount == baseline + 1);

    // flush() with nothing pending is a no-op.
    h.gs.flush();
    CHECK(h.treeUpdatedCount == baseline + 1);
}

TEST_CASE("RT-04: repeating the same depth/nodes leaves the tree flag clear -- "
          "setAnalysisData only marks the tree dirty on an actual change") {
    GameState gs;
    REQUIRE(gs.makeMove(Coord{7, 7}));

    int treeUpdatedCount = 0;
    gs.signal_tree_updated.connect([&]() { ++treeUpdatedCount; });

    PVLine pv;
    pv.pvIndex = 1;
    pv.depth   = 10;
    pv.nodes   = 100;
    pv.score   = 0.9;
    pv.moves   = {Coord{7, 7}};
    EngineStatus status;
    status.depth = 10;

    // First call actually changes depth/nodes on the current node -- dirty.
    gs.setAnalysisData({pv}, status);
    CHECK(gs.tickAnalysis());
    CHECK(treeUpdatedCount == 1);

    // Second call with the exact same depth/nodes -- current_node->depth/nodes
    // already match, so setAnalysisData's `treeChanged` guard stays false and
    // treeDirty_ is never set. The tick should have nothing tree-related to
    // deliver even though analysisDirty_ is set again (pvLines_ is replaced
    // unconditionally on every call).
    gs.setAnalysisData({pv}, status);
    CHECK(gs.tickAnalysis()); // still emits signal_engine_analysis (analysisDirty_)
    CHECK(treeUpdatedCount == 1); // but not signal_tree_updated again
}

TEST_CASE("RT-04: structural tree changes (new move) still emit "
          "signal_tree_updated synchronously, unaffected by the throttle") {
    GameState gs;
    int treeUpdatedCount = 0;
    gs.signal_tree_updated.connect([&]() { ++treeUpdatedCount; });

    // makeMove() is a structural change (adds a tree node), not a per-tick
    // eval refresh -- must still be synchronous/immediate, not gated behind
    // tickAnalysis()/flush().
    REQUIRE(gs.makeMove(Coord{7, 7}));
    CHECK(treeUpdatedCount == 1);
}
