// UI-04: the PV list must reflect ONLY the current position's current
// analysis. Two failure modes are guarded here:
//
//   (a) successive analysis snapshots for the SAME PV index must overwrite
//       that index's slot, not append -- so MultiPV=1 never shows more than
//       one row no matter how many depth iterations stream in.
//   (b) a position change must discard whatever the protocol is still
//       accumulating (GomocupProtocol::clearAnalysisState), so a late async
//       engine message for the previous position can't repopulate stale rows.
//
// These are exercised at the GomocupProtocol layer (the source of the
// std::vector<PVLine> that PVView renders); EngineController wires
// clearAnalysisState() to GameState::signal_board_changed and drops
// signal_analysis while !isAnalyzing().

#include "vendor/doctest.h"

#include "engine/gomocup_protocol.h"

namespace {

struct Recorder {
    GomocupProtocol proto;
    int analysisCount = 0;
    std::vector<PVLine> lastPVs;
    EngineStatus lastStatus;

    explicit Recorder(int boardSize = 15) : proto(boardSize) {
        proto.signal_analysis.connect(
            [this](const std::vector<PVLine> &pvs, const EngineStatus &status) {
                ++analysisCount;
                lastPVs = pvs;
                lastStatus = status;
            });
    }
};

} // namespace

TEST_CASE("UI-04: repeated PV #1 snapshots for one position overwrite a single slot") {
    Recorder rec;

    // MultiPV = 1: the engine streams the primary line once per depth
    // iteration. Each is index 0 ("PV #1"); none should append.
    rec.proto.parseLine("MESSAGE (1) 50 | 4-3 | 7,7 8,8");
    rec.proto.parseLine("MESSAGE (1) 52 | 6-5 | 7,7 8,8 9,9");
    rec.proto.parseLine("MESSAGE (1) 55 | 9-7 | 7,7 8,8 9,9 10,10");

    REQUIRE(rec.lastPVs.size() == 1);
    CHECK(rec.lastPVs[0].pvIndex == 1);
    CHECK(rec.lastPVs[0].moves.size() == 4); // latest snapshot, not the first

    // The realtime-PV format for the same index must also overwrite slot 0.
    rec.proto.parseLine("MESSAGE REALTIME PV 7,7 8,8");
    REQUIRE(rec.lastPVs.size() == 1);
    CHECK(rec.lastPVs[0].moves.size() == 2);
}

TEST_CASE("UI-04: MultiPV = N keeps at most one row per PV index") {
    Recorder rec;

    rec.proto.parseLine("MESSAGE (1) 55 | 9-7 | 7,8 8,7");
    rec.proto.parseLine("MESSAGE (2) 52 | 9-7 | 7,9 8,9");
    rec.proto.parseLine("MESSAGE (3) 50 | 10-8 | 7,7 8,8");
    REQUIRE(rec.lastPVs.size() == 3);

    // A second, deeper round for the same position -- still 3 slots.
    rec.proto.parseLine("MESSAGE (1) 60 | 11-9 | 7,8 8,7 9,6");
    rec.proto.parseLine("MESSAGE (2) 57 | 11-9 | 7,9 8,9 9,9");
    rec.proto.parseLine("MESSAGE (3) 54 | 11-9 | 7,7 8,8 9,9");
    REQUIRE(rec.lastPVs.size() == 3);
    CHECK(rec.lastPVs[0].pvIndex == 1);
    CHECK(rec.lastPVs[1].pvIndex == 2);
    CHECK(rec.lastPVs[2].pvIndex == 3);
}

TEST_CASE("UI-04: clearAnalysisState drops all PV lines for a position change") {
    Recorder rec;

    rec.proto.parseLine("MESSAGE (1) 55 | 9-7 | 7,8 8,7");
    rec.proto.parseLine("MESSAGE (2) 52 | 9-7 | 7,9 8,9");
    REQUIRE(rec.lastPVs.size() == 2);

    // Simulates EngineController's GameState::signal_board_changed handler
    // (move / undo / redo / New Game / load).
    rec.proto.clearAnalysisState();

    // Any subsequent emission for the new position must carry no stale rows.
    // "REALTIME BEST" emits signal_analysis with the (now-empty) PV vector.
    rec.proto.parseLine("MESSAGE REALTIME BEST 8,8");
    CHECK(rec.lastPVs.empty());
    CHECK(rec.lastStatus.depth == 0);
}

TEST_CASE("UI-04: a fresh analyze request also clears prior-position PV lines") {
    Recorder rec;

    rec.proto.parseLine("MESSAGE (1) 55 | 9-7 | 7,8 8,7");
    rec.proto.parseLine("MESSAGE (2) 52 | 9-7 | 7,9 8,9");
    REQUIRE(rec.lastPVs.size() == 2);

    rec.proto.generateAnalyzeRequest({Coord{7, 7}}, 1);

    rec.proto.parseLine("MESSAGE REALTIME BEST 8,8");
    CHECK(rec.lastPVs.empty());
}
