#include "search_overlay_animator.h"

#include <algorithm>
#include <cmath>

namespace {

bool isPercentLabel(const std::string &label)
{
    return label.size() >= 2 && label.back() == '%'
           && std::all_of(label.begin(), label.end() - 1,
                          [](unsigned char c) { return std::isdigit(c) != 0; });
}

/// Same rounding and 1..99 cap as the protocol's overlayTagText().
std::string percentLabel(double winrate)
{
    int pct = static_cast<int>(std::lround(std::clamp(winrate, 0.0, 1.0) * 100.0));
    return std::to_string(std::clamp(pct, 1, 99)) + "%";
}

constexpr double kValueSnap = 0.002;   // closer than 0.2 points: done

}  // namespace

void SearchOverlayAnimator::setTarget(const std::vector<Mark> &target)
{
    std::vector<Item> next;
    next.reserve(target.size() + items_.size());

    for (const Mark &t : target) {
        auto old = std::find_if(items_.begin(), items_.end(),
                                [&](const Item &it) { return it.shown.pos == t.pos; });
        Item it;
        it.shown         = t;
        it.targetWinrate = t.winrate;
        it.percentLabel  = t.kind == Mark::Kind::Tag && isPercentLabel(t.label);
        if (old == items_.end()) {
            it.shown.alpha = 0.0;   // new on this cell: fade in
        } else {
            it.shown.alpha = old->shown.alpha;   // (re)appearing mid-fade continues from there
            // Count from the value on screen only between two percent tags;
            // a mate/draw tag or another kind of mark has no number to ease.
            if (it.percentLabel && old->percentLabel && old->shown.kind == Mark::Kind::Tag) {
                it.shown.winrate = old->shown.winrate;
                it.shown.label   = percentLabel(it.shown.winrate);
            }
        }
        next.push_back(std::move(it));
    }

    for (Item &old : items_) {
        const bool kept = std::any_of(target.begin(), target.end(),
                                      [&](const Mark &t) { return t.pos == old.shown.pos; });
        if (kept || old.shown.alpha <= 0.0) continue;
        old.leaving       = true;
        old.shown.isBest  = false;   // the best-move glow belongs to live marks only
        old.targetWinrate = old.shown.winrate;
        next.push_back(std::move(old));
    }

    items_ = std::move(next);
    rebuild();
}

bool SearchOverlayAnimator::advance(double dtSeconds)
{
    if (dtSeconds < 0.0) dtSeconds = 0.0;
    const double blend = 1.0 - std::exp(-dtSeconds / kValueTauSeconds);

    for (Item &it : items_) {
        if (it.leaving) {
            it.shown.alpha = std::max(0.0, it.shown.alpha - dtSeconds / kFadeOutSeconds);
            continue;
        }
        it.shown.alpha = std::min(1.0, it.shown.alpha + dtSeconds / kFadeInSeconds);
        const double diff = it.targetWinrate - it.shown.winrate;
        if (std::fabs(diff) <= kValueSnap) it.shown.winrate = it.targetWinrate;
        else it.shown.winrate += diff * blend;
        if (it.percentLabel) it.shown.label = percentLabel(it.shown.winrate);
    }
    std::erase_if(items_, [](const Item &it) { return it.leaving && it.shown.alpha <= 0.0; });

    rebuild();
    return animating_;
}

void SearchOverlayAnimator::snap()
{
    std::erase_if(items_, [](const Item &it) { return it.leaving; });
    for (Item &it : items_) {
        it.shown.alpha   = 1.0;
        it.shown.winrate = it.targetWinrate;
        if (it.percentLabel) it.shown.label = percentLabel(it.shown.winrate);
    }
    rebuild();
}

void SearchOverlayAnimator::rebuild()
{
    shown_.clear();
    animating_ = false;
    for (const Item &it : items_) {
        shown_.push_back(it.shown);
        if (it.leaving || it.shown.alpha < 1.0 || it.shown.winrate != it.targetWinrate)
            animating_ = true;
    }
}
