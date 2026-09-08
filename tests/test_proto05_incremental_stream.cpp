// Regression tests for PROTO-05: enable the engine incremental-analysis stream.
//
// Before PROTO-05 the GUI sent `INFO SHOW_DETAIL 0` and never `YXSHOWINFO`, so
// a stock Rapfi stayed at messageMode BRIEF with both the REALTIME and INFO
// detail feeds off. The parser (parseInfo/onPVDone, parseMessage's "(n)" /
// "Depth " / "Speed " branches) is written to consume an incremental feed, but
// against a default engine it only ever saw the two end-of-search MESSAGE
// lines -- one PV line, set once, no depth progression, no Multi-PV #2..#N.
//
// This test replays a transcript in exactly the shape Rapfi emits under
// `YXSHOWINFO` + `INFO SHOW_DETAIL 3` (messageMode NORMAL + REALTIME + INFO
// detail) -- the streams PROTO-05's command changes now switch on -- through
// the real GomocupProtocol wired to a real GameState + BoardViewModel, and
// pins the four acceptance facts from docs/instruction/PROTO-05-*.md:
//   1. engineStatus().depth increases across the transcript (not set once);
//   2. pvLines() reaches `multiPV` non-empty entries, each carrying its depth;
//   3. the live board overlay (BoardViewModel::searchOverlay, PROTO-06 — was
//      candidateMoves) has `multiPV` winrate-tag marks mid-search;
//   4. NPS from the end-of-search "Speed ..." summary line is applied.
//
// The line formats are taken verbatim from Rapfi/search/searchoutput.cpp
// (printPvCompletes / printDepthCompletes / printRootMoves / printSearchEnds).
// This transcript IS the permanent fixture -- do not delete it.

#include "vendor/doctest.h"

#include "model/game_state.h"
#include "model/board_view_model.h"
#include "engine/gomocup_protocol.h"

#include <string>
#include <vector>

namespace {

struct Harness {
    GomocupProtocol proto;
    GameState       gs;

    explicit Harness(int boardSize = 15) : proto(boardSize), gs(boardSize) {
        proto.signal_analysis.connect(
            [this](const std::vector<PVLine> &pvs, const EngineStatus &status) {
                if (gs.isAnalyzing())
                    gs.setAnalysisData(pvs, status);
            });
        proto.signal_analysis_overlay.connect([this](const AnalysisOverlay &ov) {
            if (gs.isAnalyzing())
                gs.setAnalysisOverlay(ov);
        });
        gs.signal_board_changed.connect([this]() { proto.clearAnalysisState(); });
    }

    void feed(const std::vector<std::string> &lines) {
        for (const auto &l : lines) proto.parseLine(l);
    }
};

// One completed depth iteration `d` over `numPV` PV slots, NORMAL + SHOW_DETAIL 3:
// per PV the full `INFO PV n ... INFO PV DONE` block, then the NORMAL
// `(n) <eval> | <d>-<sd> | <pv>` line; then the per-depth `Depth ...` summary.
std::vector<std::string> depthRound(int d, int numPV) {
    const int sd = d + 2;
    // distinct first move per PV slot so the overlay gets `numPV` tag marks.
    // PROTO-06: these must be EMPTY cells — the live overlay (like the original
    // Yixin-Board) only marks empty intersections, and the test plays 7,7 / 8,8.
    const char *firsts[3] = {"3,3", "3,4", "4,3"};
    std::vector<std::string> out;
    for (int i = 0; i < numPV; ++i) {
        std::string pv = std::string(firsts[i]) + " 8,8 9,9";
        int eval = 55 - i * 3;
        out.push_back("INFO PV " + std::to_string(i));
        out.push_back("INFO NUMPV " + std::to_string(numPV));
        out.push_back("INFO DEPTH " + std::to_string(d));
        out.push_back("INFO SELDEPTH " + std::to_string(sd));
        out.push_back("INFO NODES 1000");
        out.push_back("INFO TOTALNODES 5000");
        out.push_back("INFO TOTALTIME 100");
        out.push_back("INFO SPEED 50000");
        out.push_back("INFO EVAL " + std::to_string(eval));
        out.push_back("INFO WINRATE 0.55");
        out.push_back("INFO BESTLINE " + pv);
        out.push_back("INFO PV DONE");
        out.push_back("MESSAGE (" + std::to_string(i + 1) + ") " + std::to_string(eval)
                      + " | " + std::to_string(d) + "-" + std::to_string(sd) + " | " + pv);
    }
    out.push_back("MESSAGE Depth " + std::to_string(d) + "-" + std::to_string(sd)
                  + " | Eval 55 | Time 100ms | 7,7 8,8 9,9");
    return out;
}

} // namespace

TEST_CASE("PROTO-05: NORMAL + SHOW_DETAIL 3 transcript drives per-depth Multi-PV progression") {
    Harness h;
    const int numPV = 3;

    REQUIRE(h.gs.makeMove(Coord{7, 7}));
    REQUIRE(h.gs.makeMove(Coord{8, 8}));
    h.gs.setAnalyzing(true);

    std::vector<int> depthsSeen;

    for (int d = 10; d <= 12; ++d) {
        h.feed(depthRound(d, numPV));
        REQUIRE(h.gs.tickAnalysis());
        depthsSeen.push_back(h.gs.engineStatus().depth);

        if (d == 11) {
            // (3) the live overlay reaches multiPV winrate-tag marks mid-search.
            BoardViewModel bvm(h.gs);
            bvm.update();
            size_t tagMarks = 0;
            for (const auto &mk : bvm.searchOverlay)
                if (mk.kind == BoardViewModel::SearchOverlayMark::Kind::Tag) ++tagMarks;
            CHECK(tagMarks == static_cast<size_t>(numPV));
        }
    }

    // (1) depth strictly increases across the transcript, not set once at end.
    REQUIRE(depthsSeen.size() == 3);
    CHECK(depthsSeen[0] == 10);
    CHECK(depthsSeen[1] == 11);
    CHECK(depthsSeen[2] == 12);

    // End-of-search: YXNBEST ranked list (printRootMoves 4-part form) + summary.
    h.feed({
        "MESSAGE (1) 55 (W 55.00, D 2.00, S 0.30) | V 5000 | SD 14 | 7,7 8,8 9,9",
        "MESSAGE (2) 52 (W 52.00, D 2.00, S 0.30) | V 4000 | SD 14 | 7,8 8,8 9,9",
        "MESSAGE (3) 49 (W 49.00, D 2.00, S 0.30) | V 3000 | SD 14 | 8,7 8,8 9,9",
        "MESSAGE Speed 1234K | Depth 12-14 | Eval 55 | Node 5000 | Time 1.2s",
        "MESSAGE Bestline 7,7 8,8 9,9",
    });
    h.gs.flush();

    const auto &pvs = h.gs.pvLines();
    // (2) pvLines() reaches multiPV non-empty entries, each carrying its depth.
    REQUIRE(pvs.size() == static_cast<size_t>(numPV));
    for (const auto &pv : pvs) {
        CHECK_FALSE(pv.moves.empty());
        CHECK(pv.depth >= 12);
    }
    // The 4-part printRootMoves lines must not blank out the PV moves (the
    // pre-PROTO-05 parser read moves from parts[2] = "SD 14", losing them).
    CHECK(pvs[1].moves.front() == Coord{8, 7});
    CHECK(pvs[2].moves.front() == Coord{7, 8});

    // (4) NPS from the "Speed 1234K ..." summary line is applied (1234 * 1000).
    CHECK(h.gs.engineStatus().nps == 1234000);
}

TEST_CASE("PROTO-05: generated start/config commands switch the incremental streams on") {
    GomocupProtocol proto(15);

    auto startCmds = proto.generateStart(15);
    REQUIRE(startCmds.size() == 2);
    CHECK(startCmds[0] == "YXSHOWINFO");
    CHECK(startCmds[1] == "START 15");

    EngineConfig cfg;                       // default showDetail == 3
    auto configCmds = proto.generateConfig(cfg);
    bool sawDetail3 = false;
    for (const auto &c : configCmds)
        if (c == "INFO SHOW_DETAIL 3") sawDetail3 = true;
    CHECK(sawDetail3);

    // A user SHOW_DETAIL via customParams must still win (emitted after).
    cfg.customParams["SHOW_DETAIL"] = "1";
    auto overridden = proto.generateConfig(cfg);
    long detailIdx = -1, customIdx = -1;
    for (long i = 0; i < static_cast<long>(overridden.size()); ++i) {
        if (overridden[i] == "INFO SHOW_DETAIL 3") detailIdx = i;
        if (overridden[i] == "INFO SHOW_DETAIL 1") customIdx = i;
    }
    REQUIRE(detailIdx >= 0);
    REQUIRE(customIdx >= 0);
    CHECK(customIdx > detailIdx);
}
