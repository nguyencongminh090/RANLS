#pragma once

#include "board_view_model.h"

#include <vector>

/// PROTO-09: eases the live search overlay between engine updates so the board
/// never jumps: a winrate tag counts from its old value to the new one, a new
/// mark fades in, a dropped one fades out.
///
/// Pure view state — GTK-free and clock-free (the caller passes elapsed time),
/// so it is unit-testable. BoardView owns one, feeds it the resolved
/// BoardViewModel::searchOverlay after every model update, and drives
/// advance() from the widget's frame clock only while something is moving.
class SearchOverlayAnimator {
public:
    using Mark = BoardViewModel::SearchOverlayMark;

    static constexpr double kFadeInSeconds  = 0.15;
    static constexpr double kFadeOutSeconds = 0.22;
    /// Time constant of the exponential approach of a displayed winrate to its
    /// target (≈95% of the way after 3x this).
    static constexpr double kValueTauSeconds = 0.09;

    /// Adopt a new resolved overlay as the target. Marks are matched by cell.
    void setTarget(const std::vector<Mark> &target);

    /// Advance by `dtSeconds`. Returns true while anything is still moving.
    bool advance(double dtSeconds);

    /// Jump straight to the target (animations disabled / widget not on screen).
    void snap();

    /// Drop fading-out marks for which `gone(pos)` is true — e.g. a stone was
    /// just played on that cell, so nothing may linger on top of it.
    template <typename Pred> void dropFading(Pred gone)
    {
        std::erase_if(items_, [&](const Item &it) { return it.leaving && gone(it.shown.pos); });
        rebuild();
    }

    bool animating() const { return animating_; }

    /// What to draw right now (target order, then marks still fading out).
    const std::vector<Mark> &shown() const { return shown_; }

private:
    struct Item {
        Mark   shown;                  ///< as drawn (eased winrate/label/alpha)
        double targetWinrate = 0.5;
        bool   percentLabel  = false;  ///< label is "NN%" and follows the eased winrate
        bool   leaving       = false;  ///< no longer in the target: fading out
    };
    void rebuild();

    std::vector<Item> items_;
    std::vector<Mark> shown_;
    bool              animating_ = false;
};
