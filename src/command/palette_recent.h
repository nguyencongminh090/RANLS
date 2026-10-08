#pragma once

// PAL-02: pure "recently used" helpers for the command palette. The ranking
// engine (palette_search) is untouched; recency is applied to its hits here.

#include "command/palette_search.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace palette_recent {

inline constexpr std::size_t kCap = 50;
inline constexpr double      kMaxBoost = 0.2;  ///< The most recent id gets +20% score.

/// Move `id` to the front (newest first), de-duplicated, capped at kCap.
inline void touch(std::vector<std::string> &recent, const std::string &id)
{
    recent.erase(std::remove(recent.begin(), recent.end(), id), recent.end());
    recent.insert(recent.begin(), id);
    if (recent.size() > kCap)
        recent.resize(kCap);
}

/// Boost hits whose id is in `recent` (linearly: newest +kMaxBoost, oldest ~0),
/// then re-sort by score, stable so equal scores keep the engine's order.
inline void applyRecency(std::vector<palette_search::Hit> &hits, const palette_search::Index &index,
                         const std::vector<std::string> &recent)
{
    for (auto &h : hits) {
        const auto &id = index.entry(h.index).id;
        const auto  it = std::find(recent.begin(), recent.end(), id);
        if (it != recent.end()) {
            const double rank = static_cast<double>(it - recent.begin());
            h.score *= 1.0 + kMaxBoost * (1.0 - rank / static_cast<double>(kCap));
        }
    }
    std::stable_sort(hits.begin(), hits.end(),
                     [](const palette_search::Hit &a, const palette_search::Hit &b) { return a.score > b.score; });
}

}  // namespace palette_recent
