// UX-06: the WinGraph "Single line" vs "Two lines" modes must produce
// genuinely different series once there is eval data. This pins the pure
// conversion in src/ui/win_graph_series.h (the visible graph is Cairo code
// out of reach of this no-gtkmm test binary).
//
// UI-09 reversed the UX-06 "SingleSide follows MatchConfig::enginePlays"
// decision: SingleSide is now ALWAYS Black's perspective. The former
// "Auto follows enginePlays == White" case below is replaced by a guard that
// SingleSide ignores enginePlays entirely.

#include "vendor/doctest.h"

#include "ui/win_graph_series.h"

#include <cmath>

namespace {
// raw[i] = eval AFTER move i, from the side to move in that position.
// i even -> White to move; i odd -> Black to move.
const std::vector<double> kRaw = {0.70, 0.65, 0.80, 0.55};
} // namespace

TEST_CASE("UX-06: SingleSide draws one line (white series empty), BothSide draws two") {
    auto single = buildWinGraphSeries(kRaw, WinGraphMode::SingleSide, EnginePlaysSide::Off);
    auto both   = buildWinGraphSeries(kRaw, WinGraphMode::BothSide,   EnginePlaysSide::Off);

    CHECK(single.black.size() == kRaw.size());
    CHECK(single.white.empty());

    CHECK(both.black.size() == kRaw.size());
    CHECK(both.white.size() == kRaw.size());

    // The two modes are visibly different: BothSide's white line is the
    // per-move complement of its black line and is populated; SingleSide has
    // no white line at all.
    for (size_t i = 0; i < kRaw.size(); ++i)
        CHECK(both.white[i] == doctest::Approx(1.0 - both.black[i]));
}

TEST_CASE("UX-06: SingleSide default perspective is Black (converts opponent-perspective evals)") {
    auto s = buildWinGraphSeries(kRaw, WinGraphMode::SingleSide, EnginePlaysSide::Off);
    // i=0: White to move, eval 0.70 for White -> Black perspective 0.30.
    CHECK(s.black[0] == doctest::Approx(0.30));
    // i=1: Black to move, eval 0.65 already Black's -> 0.65.
    CHECK(s.black[1] == doctest::Approx(0.65));
    // i=2: White to move, 0.80 -> 0.20 ; i=3: Black to move, 0.55 -> 0.55.
    CHECK(s.black[2] == doctest::Approx(0.20));
    CHECK(s.black[3] == doctest::Approx(0.55));
}

TEST_CASE("UI-09: SingleSide is always Black's perspective, regardless of enginePlays") {
    auto off   = buildWinGraphSeries(kRaw, WinGraphMode::SingleSide, EnginePlaysSide::Off);
    auto black = buildWinGraphSeries(kRaw, WinGraphMode::SingleSide, EnginePlaysSide::Black);
    auto white = buildWinGraphSeries(kRaw, WinGraphMode::SingleSide, EnginePlaysSide::White);

    // All three enginePlays settings produce the identical Black-perspective
    // series — UI-09 removed the UX-06 "follow the engine's side" coupling.
    for (size_t i = 0; i < kRaw.size(); ++i) {
        CHECK(off.black[i] == doctest::Approx(black.black[i]));
        CHECK(off.black[i] == doctest::Approx(white.black[i]));
    }

    // And that series really is Black's perspective (not White's complement):
    // i=0 White to move, eval 0.70 for White -> Black 0.30 (NOT 0.70).
    CHECK(off.black[0] == doctest::Approx(0.30));
    CHECK(off.black[2] == doctest::Approx(0.20));
}

TEST_CASE("UX-06: NaN gaps are preserved, never interpolated, in both modes") {
    std::vector<double> raw = {0.60, std::numeric_limits<double>::quiet_NaN(), 0.40};

    auto single = buildWinGraphSeries(raw, WinGraphMode::SingleSide, EnginePlaysSide::Off);
    auto both   = buildWinGraphSeries(raw, WinGraphMode::BothSide,   EnginePlaysSide::Off);

    CHECK(std::isnan(single.black[1]));
    CHECK(std::isnan(both.black[1]));
    CHECK(std::isnan(both.white[1]));
    CHECK_FALSE(std::isnan(single.black[0]));
    CHECK_FALSE(std::isnan(both.black[2]));
}
