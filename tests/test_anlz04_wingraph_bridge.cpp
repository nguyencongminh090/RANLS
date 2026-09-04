// ANLZ-04: the WinGraph must bridge a residual interior NaN run with ONE
// faint dashed connector instead of lifting the pen and fragmenting the
// trace (UI-01's original "disjoint segments" rule, softened — see
// docs/audit/2026-09-04-wingraph-nan-bridge.md).
//
// The dashed-style rendering lives in WinGraphView::onDraw (Cairo, out of
// reach of this no-gtkmm binary). The "which real points does a NaN run
// separate" decision is factored into the pure helper computeGapBridges()
// in src/ui/win_graph_bridge.h — mirrors how buildWinGraphSeries was split
// out for UX-06 — and that is what is pinned here.

#include "vendor/doctest.h"

#include "ui/win_graph_bridge.h"

#include <limits>

namespace {
const double kNaN = std::numeric_limits<double>::quiet_NaN();
} // namespace

TEST_CASE("ANLZ-04: an interior NaN run yields exactly one bridge pair spanning it") {
    std::vector<double> data = {0.5, 0.6, kNaN, kNaN, 0.55};
    auto bridges = computeGapBridges(data);
    REQUIRE(bridges.size() == 1);
    CHECK(bridges[0].first == 1);   // last real point before the gap
    CHECK(bridges[0].second == 4);  // first real point after the gap
}

TEST_CASE("ANLZ-04: a single interior NaN ply is bridged") {
    std::vector<double> data = {0.5, kNaN, 0.55};
    auto bridges = computeGapBridges(data);
    REQUIRE(bridges.size() == 1);
    CHECK(bridges[0].first == 0);
    CHECK(bridges[0].second == 2);
}

TEST_CASE("ANLZ-04: consecutive real points produce no bridge") {
    std::vector<double> data = {0.5, 0.6, 0.55, 0.7};
    CHECK(computeGapBridges(data).empty());
}

TEST_CASE("ANLZ-04: a leading NaN run has no anchor on one side — no bridge") {
    std::vector<double> data = {kNaN, kNaN, 0.5, 0.6};
    CHECK(computeGapBridges(data).empty());
}

TEST_CASE("ANLZ-04: a trailing NaN run has no anchor on one side — no bridge") {
    std::vector<double> data = {0.5, 0.6, kNaN, kNaN};
    CHECK(computeGapBridges(data).empty());
}

TEST_CASE("ANLZ-04: leading + interior + trailing gaps — only the interior one bridges") {
    std::vector<double> data = {kNaN, 0.5, kNaN, kNaN, 0.6, kNaN};
    auto bridges = computeGapBridges(data);
    REQUIRE(bridges.size() == 1);
    CHECK(bridges[0].first == 1);
    CHECK(bridges[0].second == 4);
}

TEST_CASE("ANLZ-04: multiple interior gaps each get their own bridge") {
    std::vector<double> data = {0.5, kNaN, 0.6, kNaN, kNaN, 0.55};
    auto bridges = computeGapBridges(data);
    REQUIRE(bridges.size() == 2);
    CHECK(bridges[0].first == 0);
    CHECK(bridges[0].second == 2);
    CHECK(bridges[1].first == 2);
    CHECK(bridges[1].second == 5);
}

TEST_CASE("ANLZ-04: edge cases do not crash and produce no bridge") {
    CHECK(computeGapBridges({}).empty());
    CHECK(computeGapBridges({0.5}).empty());
    CHECK(computeGapBridges({kNaN}).empty());
    CHECK(computeGapBridges({kNaN, kNaN, kNaN}).empty());
}
