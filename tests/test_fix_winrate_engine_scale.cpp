// Regression test: a winrate percentage must come from the engine's own
// `INFO WINRATE`, never from re-deriving it out of centipawns.
//
// Rapfi's cp -> winrate scale is engine/config dependent (≈116.37 for the
// yixin-net build, 200 only by default). The original Yixin-Board (main.c:6596)
// reads `INFO WINRATE` and uses `INFO EVAL` for mate detection only. We used to
// overwrite the engine's winrate with sigmoid(cp/200) whenever a later
// `MESSAGE Depth ... | Eval <cp>` / `MESSAGE (n) <cp> | ...` / UCI-like `ev <cp>`
// line arrived, so the status bar / PV panel disagreed with the board tag.
//
// Transcript values are real `yixin-net/pbrain-rapfi` output. This transcript
// IS the permanent fixture -- do not delete it.

#include "vendor/doctest.h"

#include "engine/gomocup_protocol.h"

#include <cmath>
#include <string>
#include <vector>

namespace {

struct Capture {
    GomocupProtocol     proto{15};
    std::vector<PVLine> pvs;
    EngineStatus        status;

    Capture() {
        proto.signal_analysis.connect([this](const std::vector<PVLine> &p, const EngineStatus &s) {
            pvs    = p;
            status = s;
        });
    }
    void feed(const std::vector<std::string> &lines) {
        for (const auto &l : lines) proto.parseLine(l);
    }
};

std::vector<std::string> infoBlock(int idx, int numPv, int eval, const std::string &wr) {
    return {"INFO PV " + std::to_string(idx),
            "INFO NUMPV " + std::to_string(numPv),
            "INFO DEPTH 12",
            "INFO SELDEPTH 24",
            "INFO NODES 1000",
            "INFO EVAL " + std::to_string(eval),
            "INFO WINRATE " + wr,
            "INFO BESTLINE 7,9 8,8 9,9",
            "INFO PV DONE"};
}

constexpr double kEps = 1e-6;
constexpr double kWr1 = 0.130211;   // engine: eval -221 (sigmoid(-221/200) would be 0.2488)
constexpr double kWr2 = 0.143394;   // engine: eval -208

}  // namespace

TEST_CASE("winrate: MESSAGE Depth/Eval line keeps the engine's INFO WINRATE") {
    Capture c;
    c.feed(infoBlock(0, 1, -221, "0.130211"));
    c.feed({"MESSAGE Depth 12-24 | Eval -221 | Time 21ms | K9 I11 J8"});
    CHECK(c.status.winrate == doctest::Approx(kWr1).epsilon(kEps));
    REQUIRE(!c.pvs.empty());
    CHECK(c.pvs[0].score == doctest::Approx(kWr1).epsilon(kEps));
}

TEST_CASE("winrate: MESSAGE (n) ranked lines keep each PV's INFO WINRATE") {
    Capture c;
    c.feed(infoBlock(0, 2, -221, "0.130211"));
    c.feed(infoBlock(1, 2, -208, "0.143394"));
    c.feed({"MESSAGE (1) -221 | 12-24 | H8 I9",
            "MESSAGE (2) -208 | 12-24 | H9 I9"});
    REQUIRE(c.pvs.size() == 2);
    CHECK(c.pvs[0].score == doctest::Approx(kWr1).epsilon(kEps));
    CHECK(c.pvs[1].score == doctest::Approx(kWr2).epsilon(kEps));
    CHECK(c.status.winrate == doctest::Approx(kWr1).epsilon(kEps));
}

TEST_CASE("winrate: UCI-like 'ev' line keeps the engine's INFO WINRATE") {
    Capture c;
    c.feed(infoBlock(0, 1, -221, "0.130211"));
    c.feed({"MESSAGE depth 12-24 ev -221 n 20K n/ms 953 tm 21 pv H8 I9"});
    CHECK(c.status.winrate == doctest::Approx(kWr1).epsilon(kEps));
}

TEST_CASE("winrate: a cp with no matching INFO WINRATE still falls back to sigmoid(cp/200)") {
    Capture c;   // MESSAGE-only engine (no INFO stream)
    c.feed({"MESSAGE Depth 12-24 | Eval 70 | Time 21ms | K9 I11"});
    CHECK(c.status.winrate == doctest::Approx(1.0 / (1.0 + std::exp(-70.0 / 200.0))).epsilon(kEps));
}

TEST_CASE("winrate: mate eval still wins over a stored INFO WINRATE") {
    Capture c;
    c.feed(infoBlock(0, 1, -221, "0.130211"));
    c.feed({"MESSAGE Depth 14-26 | Eval +M5 | Time 30ms | K9 I11"});
    CHECK(c.status.mateStep == 5);
    CHECK(c.status.winrate == doctest::Approx(1.0));
}
