// PROTO-09: SearchOverlayAnimator — the eased copy of the live search overlay
// that BoardView draws. Pure logic (no GTK, no clock), driven here with
// explicit time steps.

#include "vendor/doctest.h"

#include "model/search_overlay_animator.h"

#include <string>
#include <vector>

namespace {

using Mark = BoardViewModel::SearchOverlayMark;

Mark tag(Coord pos, const std::string &label, double winrate, bool best = false) {
    Mark m;
    m.pos     = pos;
    m.kind    = Mark::Kind::Tag;
    m.label   = label;
    m.winrate = winrate;
    m.isBest  = best;
    return m;
}

/// Run the animation to rest in 60 Hz frames; returns the frames it took.
int settle(SearchOverlayAnimator &a) {
    int frames = 0;
    while (a.animating() && frames < 600) {
        a.advance(1.0 / 60.0);
        ++frames;
    }
    return frames;
}

const Coord kA{6, 8}, kB{7, 9};

}  // namespace

TEST_CASE("PROTO-09 animator: a new mark fades in instead of popping") {
    SearchOverlayAnimator a;
    a.setTarget({tag(kA, "28%", 0.28)});
    REQUIRE(a.shown().size() == 1);
    CHECK(a.shown()[0].alpha == doctest::Approx(0.0));
    CHECK(a.animating());

    a.advance(SearchOverlayAnimator::kFadeInSeconds / 2);
    CHECK(a.shown()[0].alpha == doctest::Approx(0.5));

    settle(a);
    CHECK(a.shown()[0].alpha == doctest::Approx(1.0));
    CHECK(a.shown()[0].label == "28%");
    CHECK_FALSE(a.animating());
}

TEST_CASE("PROTO-09 animator: a winrate tag counts to its new value, never jumps") {
    SearchOverlayAnimator a;
    a.setTarget({tag(kA, "28%", 0.28)});
    settle(a);

    a.setTarget({tag(kA, "43%", 0.43)});
    // Target adopted, but what is on screen is still the old number.
    CHECK(a.shown()[0].label == "28%");
    CHECK(a.shown()[0].alpha == doctest::Approx(1.0));   // no re-fade on a value change
    CHECK(a.animating());

    double prev = a.shown()[0].winrate;
    bool   sawIntermediate = false;
    for (int i = 0; i < 120 && a.animating(); ++i) {
        a.advance(1.0 / 60.0);
        const double now = a.shown()[0].winrate;
        CHECK(now >= prev);            // monotonic: no overshoot, no bounce
        CHECK(now <= 0.43 + 1e-9);
        if (a.shown()[0].label != "28%" && a.shown()[0].label != "43%") sawIntermediate = true;
        prev = now;
    }
    CHECK(sawIntermediate);
    CHECK_FALSE(a.animating());
    CHECK(a.shown()[0].winrate == doctest::Approx(0.43));
    CHECK(a.shown()[0].label == "43%");
}

TEST_CASE("PROTO-09 animator: a retargeted value keeps counting from where it is") {
    SearchOverlayAnimator a;
    a.setTarget({tag(kA, "20%", 0.20)});
    settle(a);
    a.setTarget({tag(kA, "60%", 0.60)});
    a.advance(0.05);
    const double mid = a.shown()[0].winrate;
    REQUIRE(mid > 0.20);
    REQUIRE(mid < 0.60);
    a.setTarget({tag(kA, "30%", 0.30)});   // the engine changed its mind mid-count
    CHECK(a.shown()[0].winrate == doctest::Approx(mid));
    settle(a);
    CHECK(a.shown()[0].label == "30%");
}

TEST_CASE("PROTO-09 animator: a dropped mark fades out, then is gone") {
    SearchOverlayAnimator a;
    a.setTarget({tag(kA, "28%", 0.28, /*best=*/true), tag(kB, "9%", 0.09)});
    settle(a);

    a.setTarget({tag(kA, "28%", 0.28, true)});
    REQUIRE(a.shown().size() == 2);
    CHECK(a.shown()[0].pos == kA);          // live marks first, leavers after
    CHECK(a.shown()[1].pos == kB);
    CHECK(a.shown()[1].alpha == doctest::Approx(1.0));
    CHECK(a.animating());

    a.advance(SearchOverlayAnimator::kFadeOutSeconds / 2);
    CHECK(a.shown()[1].alpha == doctest::Approx(0.5));
    CHECK(a.shown()[1].label == "9%");      // keeps its last text while fading

    settle(a);
    REQUIRE(a.shown().size() == 1);
    CHECK(a.shown()[0].pos == kA);
    CHECK_FALSE(a.animating());
}

TEST_CASE("PROTO-09 animator: a mark that returns mid-fade continues, it does not restart") {
    SearchOverlayAnimator a;
    a.setTarget({tag(kB, "9%", 0.09)});
    settle(a);
    a.setTarget({});
    a.advance(SearchOverlayAnimator::kFadeOutSeconds / 2);
    a.setTarget({tag(kB, "9%", 0.09)});
    REQUIRE(a.shown().size() == 1);
    CHECK(a.shown()[0].alpha == doctest::Approx(0.5));
    settle(a);
    CHECK(a.shown()[0].alpha == doctest::Approx(1.0));
}

TEST_CASE("PROTO-09 animator: dropFading removes a leaver at once (stone played on its cell)") {
    SearchOverlayAnimator a;
    a.setTarget({tag(kA, "28%", 0.28), tag(kB, "9%", 0.09)});
    settle(a);
    a.setTarget({tag(kA, "28%", 0.28)});
    a.dropFading([](Coord c) { return c == kB; });
    REQUIRE(a.shown().size() == 1);
    CHECK(a.shown()[0].pos == kA);
    // A live mark is never dropped by it.
    a.dropFading([](Coord) { return true; });
    CHECK(a.shown().size() == 1);
}

TEST_CASE("PROTO-09 animator: snap shows the target immediately") {
    SearchOverlayAnimator a;
    a.setTarget({tag(kA, "28%", 0.28), tag(kB, "9%", 0.09)});
    settle(a);
    a.setTarget({tag(kA, "43%", 0.43)});
    a.snap();
    REQUIRE(a.shown().size() == 1);
    CHECK(a.shown()[0].label == "43%");
    CHECK(a.shown()[0].alpha == doctest::Approx(1.0));
    CHECK_FALSE(a.animating());
}

TEST_CASE("PROTO-09 animator: mate and draw labels are shown as is, not counted") {
    SearchOverlayAnimator a;
    a.setTarget({tag(kA, "62%", 0.62)});
    settle(a);
    a.setTarget({tag(kA, "+M7", 1.0)});
    CHECK(a.shown()[0].label == "+M7");
    CHECK(a.shown()[0].winrate == doctest::Approx(1.0));
    a.advance(0.02);
    CHECK(a.shown()[0].label == "+M7");

    Mark lost;
    lost.pos  = kB;
    lost.kind = Mark::Kind::Lost;
    a.setTarget({lost});
    settle(a);
    REQUIRE(a.shown().size() == 1);
    CHECK(a.shown()[0].kind == Mark::Kind::Lost);
    CHECK(a.shown()[0].label.empty());
}
