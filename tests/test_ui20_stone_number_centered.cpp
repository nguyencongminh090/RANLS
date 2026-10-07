// Regression test: move numbers on stones were positioned from the text
// advance box without x/y_bearing, so the ink was visibly off-centre.
// Renders a numbered white stone into a Cairo image surface (no display) and
// checks the centroid of the dark number pixels is at the stone centre.

#include "vendor/doctest.h"

#include "model/board_view_model.h"
#include "model/game_state.h"
#include "ui/board_renderer.h"

#include <cmath>
#include <cstdint>

namespace {
constexpr int kSize = 400;

void centroidOffset(int moves, double &dx, double &dy, double &cell) {
    GameState gs{15};
    for (int i = 0; i < moves; ++i) gs.makeMove(Coord{i % 15, i / 15});
    BoardViewModel vm{gs};
    vm.viewConfig.showMoveNumbers = true;
    vm.update();
    auto surf = Cairo::ImageSurface::create(Cairo::ImageSurface::Format::ARGB32, kSize, kSize);
    auto cr   = Cairo::Context::create(surf);
    BoardRenderer r(vm);
    r.draw(cr, kSize, kSize);
    surf->flush();
    auto g = r.computeGeometry(kSize, kSize);
    cell = g.cellSize;
    // Move #2 is White at (1,0): dark digit on a light stone.
    const double cx = g.marginLeft + g.cellSize * 1.5;
    const double cy = g.marginTop + g.cellSize * 0.5;
    double sx = 0, sy = 0, n = 0;
    const int rad = static_cast<int>(g.cellSize * 0.4);
    for (int y = int(cy) - rad; y <= int(cy) + rad; ++y)
        for (int x = int(cx) - rad; x <= int(cx) + rad; ++x) {
            const auto *row = reinterpret_cast<const uint32_t *>(surf->get_data() + y * surf->get_stride());
            uint32_t p = row[x];
            int lum = (((p >> 16) & 0xFF) + ((p >> 8) & 0xFF) + (p & 0xFF)) / 3;
            if (lum < 110) { sx += x; sy += y; n += 1; }
        }
    REQUIRE(n > 0);
    dx = sx / n - cx;
    dy = sy / n - cy;
}
} // namespace

TEST_CASE("move number ink is centred on the stone") {
    double dx, dy, cell;
    centroidOffset(3, dx, dy, cell);
    CHECK(std::fabs(dx) < cell * 0.02);
    CHECK(std::fabs(dy) < cell * 0.02);
}
