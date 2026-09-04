#pragma once

// ANLZ-04: pure computation of the NaN-gap "bridge" connectors for the
// WinGraph, split out of WinGraphView::onDraw so it can be unit-tested
// without gtkmm (see tests/test_anlz04_wingraph_bridge.cpp). The rendering
// (dashed, faint, thin — visually subordinate to the solid UI-09 series)
// stays in onDraw; only the "which real points does a NaN run separate"
// decision lives here.
//
// Given a plotted series where std::isnan(data[i]) marks an unevaluated ply
// (UI-01 sentinel — never interpolated, never a synthesised 50%), return one
// {fromIdx, toIdx} pair per interior NaN run: the last real point before the
// run and the first real point after it. Leading / trailing NaN runs have no
// anchor on one side and produce no bridge. n == 0/1 and all-NaN produce none.

#include <cmath>
#include <utility>
#include <vector>

inline std::vector<std::pair<int, int>>
computeGapBridges(const std::vector<double> &data)
{
    std::vector<std::pair<int, int>> bridges;
    int lastReal = -1;
    for (int i = 0; i < static_cast<int>(data.size()); ++i) {
        if (std::isnan(data[i]))
            continue;
        if (lastReal >= 0 && i > lastReal + 1)
            bridges.emplace_back(lastReal, i);
        lastReal = i;
    }
    return bridges;
}
