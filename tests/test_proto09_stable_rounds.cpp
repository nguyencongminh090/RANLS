// Regression tests for PROTO-09: the analysis state must not flicker between
// engine lines, in every Rapfi message mode.
//
// Line shapes are copied from real `pbrain-rapfi` 0.44.01 transcripts captured
// with the GUI's own command sequence (YXSHOWINFO, INFO SHOW_DETAIL 3,
// YXBOARD ... DONE, YXNBEST n) under message_mode = normal / ucilike, for both
// alpha-beta and MCTS search; only the PV tails are shortened. BRIEF is
// upgraded to NORMAL by YXSHOWINFO, and NONE leaves the INFO stream alone, so
// those two are covered by the NORMAL and INFO-only cases.
//
// What used to go wrong (each has a case below):
//  1. Every depth round the MESSAGE "(1)"/"multipv 1" line truncated the PV
//     list to one row, and INFO NUMPV then re-grew it with empty placeholders,
//     so rows 2..N blanked for the whole round.
//  2. MCTS ucilike lines ("... w 33.40 d 8.68 ...") had their DRAW rate read as
//     the depth; the bouncing depth then wiped board tags, the best move's
//     included.
//  3. MCTS reports no depth at all, so depth-keyed stale-tag cleanup never
//     ran and tags of moves that left the top-N stayed forever.
//  4. With the INFO stream off, no board tag was ever written.
//  5. selDepth alternated between "max over all PVs" (INFO) and "PV #1's"
//     (MESSAGE Depth line).
//  6. An eval with no INFO WINRATE of its own was converted with /200 whatever
//     the engine's real scale.
//
// These transcripts ARE the permanent fixture -- do not delete them.

#include "vendor/doctest.h"

#include "engine/gomocup_protocol.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace {

struct Probe {
    GomocupProtocol     proto{15};
    std::vector<PVLine> pvs;
    EngineStatus        status;
    AnalysisOverlay     overlay;

    Probe() {
        proto.signal_analysis.connect([this](const std::vector<PVLine> &p, const EngineStatus &s) {
            pvs    = p;
            status = s;
        });
        proto.signal_analysis_overlay.connect([this](const AnalysisOverlay &o) { overlay = o; });
    }
    void feed(const std::string &line) { proto.parseLine(line); }
    void feed(const std::vector<std::string> &lines) {
        for (const auto &l : lines) proto.parseLine(l);
    }
    int filledRows() const {
        int n = 0;
        for (const auto &pv : pvs) n += pv.moves.empty() ? 0 : 1;
        return n;
    }
    std::string tag(Coord c) const {
        auto it = overlay.cells.find(c);
        return it == overlay.cells.end() ? std::string{} : it->second.tag;
    }
    int tagCount() const {
        int n = 0;
        for (const auto &kv : overlay.cells) n += kv.second.tag.empty() ? 0 : 1;
        return n;
    }
};

// printPvCompletes (alpha-beta): one INFO block per PV per depth.
std::vector<std::string> abInfo(int idx, int numPv, int depth, int selDepth, int eval,
                                const std::string &wr, const std::string &line) {
    return {"INFO PV " + std::to_string(idx),
            "INFO NUMPV " + std::to_string(numPv),
            "INFO DEPTH " + std::to_string(depth),
            "INFO SELDEPTH " + std::to_string(selDepth),
            "INFO NODES " + std::to_string(1000 * (idx + 1)),
            "INFO TOTALNODES 30000",
            "INFO TOTALTIME 28",
            "INFO SPEED 1075000",
            "INFO EVAL " + std::to_string(eval),
            "INFO WINRATE " + wr,
            "INFO BESTLINE " + line,
            "INFO PV DONE"};
}

// printRootMoves (MCTS): no INFO DEPTH, extra DRAWRATE/PRIOR/STDEV/LCBVALUE.
std::vector<std::string> mctsInfo(int idx, int numPv, int eval, const std::string &wr,
                                  const std::string &line) {
    return {"INFO PV " + std::to_string(idx),
            "INFO NUMPV " + std::to_string(numPv),
            "INFO SELDEPTH 56",
            "INFO NODES " + std::to_string(182141 / (idx + 1)),
            "INFO TOTALNODES 184815",
            "INFO TOTALTIME 1000",
            "INFO SPEED 184815",
            "INFO EVAL " + std::to_string(eval),
            "INFO WINRATE " + wr,
            "INFO DRAWRATE 0.0853078",
            "INFO PRIOR 0.63916",
            "INFO STDEV 0.5078",
            "INFO LCBVALUE -0.355125",
            "INFO BESTLINE " + line,
            "INFO PV DONE"};
}

// "y,x" wire coords -> Coord{x, y}.
const Coord kA{6, 8};   // "8,6"
const Coord kB{7, 9};   // "9,7"
const Coord kC{7, 6};   // "6,7"
const Coord kD{5, 9};   // "9,5"

}  // namespace

// ── 1. PV list never collapses between rounds (NORMAL, INFO + MESSAGE) ───────
TEST_CASE("PROTO-09: NORMAL multi-PV rounds never blank rows 2..N") {
    Probe p;
    auto round = [&](int depth, bool checkEveryLine) {
        const std::vector<std::vector<std::string>> blocks = {
            abInfo(0, 3, depth, depth + 2, -110, "0.279841", "8,6 8,5 7,9"),
            {"MESSAGE (1) -110 | " + std::to_string(depth) + "-10 | I7 I6 H7",
             "MESSAGE REALTIME REFRESH", "MESSAGE REALTIME BEST 8,6"},
            abInfo(1, 3, depth, depth + 9, -272, "0.088", "9,7 8,5 5,9"),
            {"MESSAGE (2) -272 | " + std::to_string(depth) + "-20 | H10 I7 F10",
             "MESSAGE REALTIME REFRESH", "MESSAGE REALTIME BEST 8,6"},
            abInfo(2, 3, depth, depth + 9, -268, "0.090", "6,7 8,5 7,7"),
            {"MESSAGE (3) -268 | " + std::to_string(depth) + "-20 | F10 I7 H6",
             "MESSAGE REALTIME REFRESH", "MESSAGE REALTIME BEST 8,6",
             "MESSAGE Depth " + std::to_string(depth) + "-10 | Eval -110 | Time 25ms | I7 I6 H7"}};
        for (const auto &block : blocks) {
            for (const auto &line : block) {
                p.feed(line);
                if (checkEveryLine) {
                    INFO("depth " << depth << " after: " << line);
                    CHECK(p.pvs.size() == 3);
                    CHECK(p.filledRows() == 3);
                }
            }
        }
    };
    round(11, /*checkEveryLine=*/false);   // first round fills the list
    REQUIRE(p.pvs.size() == 3);
    for (int depth = 12; depth <= 15; ++depth) round(depth, true);

    // INFO wrote each row this round: the MESSAGE text must not overwrite its
    // per-move node count with the search total, nor its engine winrate.
    CHECK(p.pvs[1].nodes == 2000);
    CHECK(p.pvs[1].score == doctest::Approx(0.088));
    CHECK(p.tagCount() == 3);
}

TEST_CASE("PROTO-09: UCILIKE multi-PV rounds never blank rows 2..N") {
    Probe p;
    auto round = [&](int depth, bool check) {
        const std::vector<std::vector<std::string>> blocks = {
            abInfo(0, 3, depth, depth + 2, -84, "0.326", "8,6 8,5 7,9"),
            {"MESSAGE depth " + std::to_string(depth) + "-14 multipv 1 ev -84 n 30K n/ms 1075 tm 28 pv I7 I6 H7"},
            abInfo(1, 3, depth, depth + 3, -302, "0.070", "9,7 8,5 5,9"),
            {"MESSAGE depth " + std::to_string(depth) + "-15 multipv 2 ev -302 n 32K n/ms 1068 tm 30 pv H10 I7"},
            abInfo(2, 3, depth, depth + 3, -323, "0.058", "6,7 8,5 7,7"),
            {"MESSAGE depth " + std::to_string(depth) + "-15 multipv 3 ev -323 n 32K n/ms 1054 tm 31 pv F10 H10"}};
        for (const auto &block : blocks)
            for (const auto &line : block) {
                p.feed(line);
                if (check) {
                    INFO("depth " << depth << " after: " << line);
                    CHECK(p.pvs.size() == 3);
                    CHECK(p.filledRows() == 3);
                }
            }
    };
    round(12, false);
    for (int depth = 13; depth <= 16; ++depth) round(depth, true);
}

// ── 2. MCTS ucilike: `d` is the draw rate, not a depth ───────────────────────
TEST_CASE("PROTO-09: MCTS ucilike draw rate is not read as depth and tags stay put") {
    Probe p;
    for (int round = 0; round < 3; ++round) {
        p.feed(mctsInfo(0, 3, -80, "0.333983", "8,6 8,5 7,6"));
        p.feed("MESSAGE multipv 1 ev -80 w 33.40 d 8.68 stdev 0.52 v 182K seldepth 58 n 475K n/ms 177 tm 1000 prior 0.64 pv I7 I6");
        p.feed(mctsInfo(1, 3, -185, "0.169", "9,7 8,6 5,9"));
        p.feed("MESSAGE multipv 2 ev -185 w 16.93 d 15.90 stdev 0.23 v 1257 seldepth 22 n 475K n/ms 177 tm 1000 prior 0.14 pv H10 I7");
        p.feed(mctsInfo(2, 3, -255, "0.100", "6,7 8,6 7,7"));
        p.feed("MESSAGE multipv 3 ev -255 w 10.05 d 10.56 stdev 0.23 v 1149 seldepth 29 n 475K n/ms 177 tm 1000 prior 0.10 pv F10 H10");

        INFO("round " << round);
        CHECK(p.status.depth == 0);            // MCTS has no iteration depth
        CHECK(p.status.selDepth == 58);
        CHECK(p.pvs.size() == 3);
        CHECK(p.tag(kA) == "33%");             // the best move's tag was the one being wiped
        CHECK(p.tag(kB) == "17%");
        CHECK(p.tag(kC) == "10%");
    }
}

TEST_CASE("PROTO-09: MCTS ucilike MESSAGE-only line yields winrate, visits and seldepth") {
    Probe p;   // SHOW_DETAIL 0/1: no INFO stream at all
    p.feed("MESSAGE multipv 1 ev -80 w 33.40 d 8.68 stdev 0.52 v 182K seldepth 58 n 475K n/ms 177 tm 1000 prior 0.64 pv 8,6 8,5");
    p.feed("MESSAGE multipv 2 ev -185 w 16.93 d 15.90 stdev 0.23 v 1257 seldepth 22 n 475K n/ms 177 tm 1000 prior 0.14 pv 9,7 8,6");
    REQUIRE(p.pvs.size() == 2);
    CHECK(p.status.depth == 0);
    CHECK(p.status.winrate == doctest::Approx(0.334));   // the engine's own, not sigmoid(-80/200)
    CHECK(p.pvs[0].score == doctest::Approx(0.334));
    CHECK(p.pvs[0].nodes == 182000);
    CHECK(p.pvs[0].selDepth == 58);
    CHECK(p.pvs[1].score == doctest::Approx(0.1693));
    CHECK(p.pvs[1].nodes == 1257);
    CHECK(p.status.nodes == 475000);
    CHECK(p.tag(kA) == "33%");
    CHECK(p.tag(kB) == "17%");
}

TEST_CASE("PROTO-09: a bare `d <int>` before any `w` still means depth") {
    Probe p;
    p.feed("MESSAGE d 12 ev 40 pv 8,6 8,5");
    CHECK(p.status.depth == 12);
}

// ── 3. MCTS (no depth): a move that leaves the top-N loses its tag ──────────
TEST_CASE("PROTO-09: stale tags clear per round even without INFO DEPTH") {
    Probe p;
    p.feed(mctsInfo(0, 3, -84, "0.326", "8,6 8,5"));
    p.feed(mctsInfo(1, 3, -185, "0.169", "9,7 8,6"));
    p.feed(mctsInfo(2, 3, -255, "0.093", "6,7 8,6"));
    CHECK(p.tagCount() == 3);
    CHECK(p.tag(kC) == "9%");

    // Next dump: third candidate is now D; C dropped out.
    p.feed(mctsInfo(0, 3, -88, "0.317", "8,6 8,5"));
    p.feed(mctsInfo(1, 3, -181, "0.172", "9,7 8,6"));
    CHECK(p.tag(kC) == "9%");                  // round not over yet: keep it (no flicker)
    p.feed(mctsInfo(2, 3, -206, "0.151", "9,5 8,6"));
    CHECK(p.tag(kC).empty());
    CHECK(p.tag(kD) == "15%");
    CHECK(p.tagCount() == 3);
    CHECK(p.status.depth == 0);
}

// ── 4. MESSAGE-only streams also feed the board tags ────────────────────────
TEST_CASE("PROTO-09: NORMAL MESSAGE-only stream writes and retires board tags") {
    Probe p;
    p.feed({"MESSAGE (1) 55 | 9-7 | 8,6 8,5", "MESSAGE (2) 12 | 9-7 | 9,7 8,6",
            "MESSAGE (3) -30 | 9-8 | 6,7 8,6", "MESSAGE Depth 9-7 | Eval 55 | Time 18ms | 8,6 8,5"});
    CHECK(p.pvs.size() == 3);                  // the Depth summary is not a new round
    CHECK(p.tagCount() == 3);
    CHECK_FALSE(p.tag(kC).empty());

    p.feed({"MESSAGE (1) 60 | 10-9 | 8,6 8,5", "MESSAGE (2) 14 | 10-9 | 9,7 8,6",
            "MESSAGE (3) -28 | 10-9 | 9,5 8,6", "MESSAGE Depth 10-9 | Eval 60 | Time 30ms | 8,6 8,5"});
    // No PV count on this stream: the round is only known complete when the
    // next one opens, so C's tag lives one more round, then goes.
    CHECK_FALSE(p.tag(kC).empty());
    p.feed("MESSAGE (1) 61 | 11-9 | 8,6 8,5");
    CHECK(p.tag(kC).empty());
    CHECK_FALSE(p.tag(kD).empty());
    CHECK(p.pvs.size() == 3);
}

TEST_CASE("PROTO-09: single-PV NORMAL stream — each Depth line is its own round") {
    Probe p;
    p.feed("MESSAGE Depth 9-7 | Eval 55 | Time 18ms | 8,6 8,5");
    CHECK_FALSE(p.tag(kA).empty());
    p.feed("MESSAGE Depth 10-9 | Eval 20 | Time 30ms | 9,7 8,6");   // best move changed
    CHECK_FALSE(p.tag(kB).empty());
    p.feed("MESSAGE Depth 11-9 | Eval 22 | Time 40ms | 9,7 8,6");
    CHECK(p.tag(kA).empty());                  // the old best is no longer tagged
    CHECK(p.tagCount() == 1);
    CHECK(p.pvs.size() == 1);
}

TEST_CASE("PROTO-09: NORMAL MCTS '(n)' line uses the engine's W and V fields") {
    Probe p;
    p.feed("MESSAGE (1) -84 (W 32.62, D 8.53, S 0.51) | V 182K | SD 56 | 8,6 8,5");
    REQUIRE(p.pvs.size() == 1);
    CHECK(p.pvs[0].score == doctest::Approx(0.3262));
    CHECK(p.pvs[0].nodes == 182000);
    CHECK(p.pvs[0].selDepth == 56);
    CHECK(p.status.winrate == doctest::Approx(0.3262));
    CHECK(p.tag(kA) == "33%");
}

// ── 5. selDepth never steps back ────────────────────────────────────────────
TEST_CASE("PROTO-09: status depth/selDepth are monotonic within a search") {
    Probe p;
    p.feed(abInfo(0, 2, 11, 13, -110, "0.279841", "8,6 8,5"));
    p.feed(abInfo(1, 2, 11, 20, -272, "0.088", "9,7 8,5"));
    CHECK(p.status.selDepth == 20);
    p.feed("MESSAGE Depth 11-10 | Eval -110 | Time 25ms | I7 I6");
    CHECK(p.status.depth == 11);
    CHECK(p.status.selDepth == 20);
    p.feed("MESSAGE Speed 925K | Depth 11-10 | Eval -110 | Node 2749K | Time 2970ms");
    CHECK(p.status.selDepth == 20);
    p.feed("MESSAGE depth 11-9 ev -110 n 30K n/ms 1075 tm 28 pv I7 I6");
    CHECK(p.status.selDepth == 20);
}

// ── 6. Evals without their own INFO WINRATE use the engine's learned scale ──
TEST_CASE("PROTO-09: unmatched MESSAGE eval is converted with the engine's own scale") {
    Probe p;
    // Real yixin-net pair: eval -221 <-> winrate 0.130211  =>  scale ~= 116.4.
    p.feed(abInfo(0, 1, 12, 24, -221, "0.130211", "8,6 8,5"));
    const double k = -221.0 / std::log(0.130211 / (1.0 - 0.130211));
    CHECK(k == doctest::Approx(116.4).epsilon(0.01));

    // End-of-search summary with a different eval (another thread's result).
    p.feed("MESSAGE Speed 647K | Depth 12-24 | Eval -130 | Node 1924K | Time 2973ms");
    CHECK(p.status.winrate == doctest::Approx(1.0 / (1.0 + std::exp(130.0 / k))).epsilon(1e-6));
    // ... which is far from what the hardcoded /200 used to show.
    CHECK(std::fabs(p.status.winrate - 1.0 / (1.0 + std::exp(130.0 / 200.0))) > 0.05);

    // The scale is an engine constant: it survives a position change.
    p.proto.clearAnalysisState();
    p.feed("MESSAGE Depth 9-7 | Eval -130 | Time 18ms | 8,6 8,5");
    CHECK(p.status.winrate == doctest::Approx(1.0 / (1.0 + std::exp(130.0 / k))).epsilon(1e-6));
}

// ── 7. A fresh think starts clean ───────────────────────────────────────────
TEST_CASE("PROTO-09: clearAnalysisState resets the round bookkeeping") {
    Probe p;
    p.feed(mctsInfo(0, 2, -84, "0.326", "8,6 8,5"));
    p.feed(mctsInfo(1, 2, -185, "0.169", "9,7 8,6"));
    p.proto.clearAnalysisState();
    p.feed(mctsInfo(0, 2, -50, "0.40", "6,7 8,6"));
    CHECK(p.pvs.size() == 1);
    CHECK(p.tagCount() == 1);
    CHECK(p.tag(kC) == "40%");
}
