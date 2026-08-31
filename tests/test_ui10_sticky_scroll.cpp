// UI-10: the Engine Log must stay pinned to the newest line while the engine
// streams analysis output, WITHOUT yanking a user who has scrolled up to read
// history back down.
//
// The GTK scroll plumbing (persistent end-of-buffer mark + deferred re-scroll)
// needs a realized TextView and can't run here. What IS pure and testable is
// the *decision*: given the remembered "stick" intent and the scroll geometry
// sampled on a flush tick, should flushPending() scroll to the bottom? That
// decision is where the bug lived — the old code recomputed at-bottom purely
// from a live adjustment whose `upper` lags the just-inserted text during a
// fast stream, so a genuinely-bottomed view read as "not at bottom" and
// stickiness silently turned itself off. These cases pin the fixed behaviour.

#include "vendor/doctest.h"

#include "ui/sticky_scroll.h"

using sticky_scroll::ScrollGeometry;
using sticky_scroll::isAtBottom;
using sticky_scroll::shouldStickToBottom;
using sticky_scroll::updateStickOnSettle;

constexpr double kEps = 1.0;

TEST_CASE("UI-10: isAtBottom is true within epsilon, false once scrolled up") {
    // value + pageSize == upper exactly -> at bottom.
    CHECK(isAtBottom({900.0, 100.0, 1000.0}, kEps));
    // half a pixel short -> still counts (epsilon tolerance).
    CHECK(isAtBottom({899.5, 100.0, 1000.0}, kEps));
    // scrolled up 50px -> not at bottom.
    CHECK_FALSE(isAtBottom({850.0, 100.0, 1000.0}, kEps));
    // content shorter than the viewport -> trivially at bottom.
    CHECK(isAtBottom({0.0, 100.0, 40.0}, kEps));
}

TEST_CASE("UI-10: at the bottom -> stick") {
    // Remembered intent true and geometry agrees.
    CHECK(shouldStickToBottom(true, {900.0, 100.0, 1000.0}, kEps));
    // Even if the remembered intent were somehow false, a live at-bottom
    // reading still sticks (e.g. user just scrolled back down this tick).
    CHECK(shouldStickToBottom(false, {900.0, 100.0, 1000.0}, kEps));
}

TEST_CASE("UI-10: user scrolled up -> do NOT stick (not yanked down)") {
    // Intent cleared by a real scroll-up event, geometry confirms we're up.
    CHECK_FALSE(shouldStickToBottom(false, {200.0, 100.0, 1000.0}, kEps));
}

TEST_CASE("UI-10: stale `upper` mid-stream must NOT disable stickiness") {
    // The regression scenario. We were pinned to the bottom last settle, so
    // rememberedStick == true. This flush tick inserted ~5 lines (~90px), but
    // the adjustment `upper` still reads the PRE-insert 1000 while `value` was
    // last driven to 900 -> value + pageSize (1000) < realUpper, and even
    // against the stale upper it's exactly at bottom. The key case is when the
    // stale sample reads *below* bottom yet the intent is intact:
    ScrollGeometry staleLooksScrolledUp{900.0, 100.0, 1090.0};  // upper already grew, value not yet
    CHECK_FALSE(isAtBottom(staleLooksScrolledUp, kEps));         // geometry alone would give up
    CHECK(shouldStickToBottom(true, staleLooksScrolledUp, kEps)); // intent keeps us stuck
}

TEST_CASE("UI-10: settle handler is the single place intent flips") {
    // Landed at the bottom after an auto-scroll -> keep sticking.
    CHECK(updateStickOnSettle({900.0, 100.0, 1000.0}, kEps));
    // User dragged the scrollbar up -> stop sticking until they return.
    CHECK_FALSE(updateStickOnSettle({120.0, 100.0, 1000.0}, kEps));
    // User dragged back to the bottom -> resume.
    CHECK(updateStickOnSettle({900.0, 100.0, 1000.0}, kEps));
}
