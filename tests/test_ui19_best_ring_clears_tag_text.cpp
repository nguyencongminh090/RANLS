// Regression test: the cyan best-move ring was sized from the tag disc only, so
// a wide winrate label (e.g. "100.0%") extended past it and the ring was drawn
// over the text. Rendered for real into a Cairo image surface (no display).
//
// The ring is drawn after the label, so any overlap overwrites white label
// pixels: the count of near-white pixels in the cell must be the same with
// and without the best flag.

#include "vendor/doctest.h"

#include "model/board_view_model.h"
#include "model/game_state.h"
#include "ui/board_renderer.h"

#include <cstdint>

namespace {

constexpr int kSize = 400;

int whitePixelsAt(BoardViewModel &vm, Coord c) {
    auto surf = Cairo::ImageSurface::create(Cairo::ImageSurface::Format::ARGB32, kSize, kSize);
    auto cr   = Cairo::Context::create(surf);
    BoardRenderer r(vm);
    r.draw(cr, kSize, kSize);
    surf->flush();
    auto g = r.computeGeometry(kSize, kSize);
    int x0 = static_cast<int>(g.marginLeft + g.cellSize * (c.x - 0.5));
    int y0 = static_cast<int>(g.marginTop  + g.cellSize * (c.y - 0.5));
    int n  = static_cast<int>(g.cellSize * 2);
    int count = 0;
    for (int y = y0; y < y0 + n; ++y) {
        const auto *row = reinterpret_cast<const uint32_t *>(surf->get_data() + y * surf->get_stride());
        for (int x = x0; x < x0 + n; ++x) {
            uint32_t p = row[x];
            if (((p >> 16) & 0xFF) > 200 && ((p >> 8) & 0xFF) > 200 && (p & 0xFF) > 200) ++count;
        }
    }
    return count;
}

} // namespace

TEST_CASE("best ring does not cover a wide winrate tag label") {
    GameState gs{15};
    BoardViewModel vm{gs};
    vm.update();
    BoardViewModel::SearchOverlayMark t;
    t.pos     = Coord{7, 7};
    t.kind    = BoardViewModel::SearchOverlayMark::Kind::Tag;
    t.label   = "100.0%";
    t.winrate = 0.99;
    t.isBest  = false;
    vm.searchOverlay = {t};
    const int plain = whitePixelsAt(vm, t.pos);
    REQUIRE(plain > 0);

    vm.searchOverlay[0].isBest = true;
    CHECK(whitePixelsAt(vm, t.pos) == plain);
}
