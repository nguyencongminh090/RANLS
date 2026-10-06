// UX-08 render tests: hover crosshair below stones/marks + emphasised margin
// labels. Rendered for real into a Cairo image surface and compared pixel-wise
// against a render with no hover. Needs gtkmm/cairomm only, no display.
//
// NOT covered here: the BoardView query-tooltip signal wiring (see the UX-08
// fix-log) -- BoardView needs a GTK display/widget tree; the text it shows is
// covered by test_ux08_tooltip_model.cpp.

#include "vendor/doctest.h"

#include "model/board_view_model.h"
#include "model/game_state.h"
#include "ui/board_renderer.h"

#include <cstdint>

namespace {

using Surf = Cairo::RefPtr<Cairo::ImageSurface>;
constexpr int kSize = 400;

Surf render(const BoardViewModel &vm, BoardRenderer::Geometry *g = nullptr,
            double fgc = 0.15) {
    auto surf = Cairo::ImageSurface::create(Cairo::ImageSurface::Format::ARGB32, kSize, kSize);
    auto cr   = Cairo::Context::create(surf);
    BoardRenderer r(vm);
    r.setCoordinateColor(fgc, fgc, fgc);
    r.draw(cr, kSize, kSize);
    if (g) *g = r.computeGeometry(kSize, kSize);
    surf->flush();
    return surf;
}

uint32_t px(const Surf &s, int x, int y) {
    const unsigned char *row = s->get_data() + y * s->get_stride();
    return reinterpret_cast<const uint32_t *>(row)[x];
}

bool rectDiffers(const Surf &a, const Surf &b, int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (px(a, x, y) != px(b, x, y)) return true;
    return false;
}

struct Fixture {
    GameState gs;
    BoardViewModel vm{gs};
    Coord stone{9, 4};     // on the hovered row
    Coord hover{3, 4};
    explicit Fixture(int bs = 15) : gs(bs) {
        gs.makeMove(stone);
        vm.update();
        vm.hoverMove = Coord{};
        vm.pvPreview.clear();
    }
};

// A thin box along the hovered row / column inside cell `far` (excludes the hovered cell).
bool rowChanged(const Surf &a, const Surf &b, const BoardRenderer::Geometry &g, Coord h, int farCol) {
    const int y  = static_cast<int>(g.marginTop + g.cellSize * (h.y + 0.5));
    const int x0 = static_cast<int>(g.marginLeft + g.cellSize * (farCol + 0.15));
    const int x1 = static_cast<int>(g.marginLeft + g.cellSize * (farCol + 0.85));
    return rectDiffers(a, b, x0, y - 2, x1, y + 3);
}
bool colChanged(const Surf &a, const Surf &b, const BoardRenderer::Geometry &g, Coord h, int farRow) {
    const int x  = static_cast<int>(g.marginLeft + g.cellSize * (h.x + 0.5));
    const int y0 = static_cast<int>(g.marginTop + g.cellSize * (farRow + 0.15));
    const int y1 = static_cast<int>(g.marginTop + g.cellSize * (farRow + 0.85));
    return rectDiffers(a, b, x - 2, y0, x + 3, y1);
}

} // namespace

TEST_CASE("UX-08: hovering an empty cell draws a crosshair along its row and column") {
    for (int bs : {15, 22}) {
        Fixture f(bs);
        BoardRenderer::Geometry g;
        auto base = render(f.vm, &g);
        f.vm.hoverMove = f.hover;
        auto hov = render(f.vm);

        CHECK(rowChanged(base, hov, g, f.hover, bs - 2));   // far right of the row
        CHECK(rowChanged(base, hov, g, f.hover, 0));
        CHECK(colChanged(base, hov, g, f.hover, bs - 2));   // far bottom of the column
        CHECK(colChanged(base, hov, g, f.hover, 0));
        // A cell off the cross is untouched.
        const int ox = static_cast<int>(g.marginLeft + g.cellSize * 7.15);
        const int oy = static_cast<int>(g.marginTop + g.cellSize * 8.15);
        CHECK_FALSE(rectDiffers(base, hov, ox, oy, ox + 3, oy + 3));
    }
}

TEST_CASE("UX-08: the crosshair is below stones; a stone on the hovered row stays intact") {
    Fixture f;
    BoardRenderer::Geometry g;
    auto base = render(f.vm, &g);
    f.vm.hoverMove = f.hover;
    auto hov = render(f.vm);

    const int cx = static_cast<int>(g.marginLeft + g.cellSize * (f.stone.x + 0.5));
    const int cy = static_cast<int>(g.marginTop + g.cellSize * (f.stone.y + 0.5));
    const int r  = static_cast<int>(g.cellSize * 0.30);
    CHECK_FALSE(rectDiffers(base, hov, cx - r, cy - r, cx + r + 1, cy + r + 1));
    CHECK(rowChanged(base, hov, g, f.hover, 12));   // the line really runs along that row
}

TEST_CASE("UX-08: a database mark on the hovered row stays visible above the crosshair") {
    Fixture f;
    f.gs.addDatabaseEntry([] { DatabaseEntry d; d.pos = {6, 4}; d.value = 90; d.label = "w3"; return d; }());
    f.vm.update();
    f.vm.hoverMove = f.hover;
    BoardRenderer::Geometry g;
    auto withMark = render(f.vm, &g);
    auto saved = f.vm.databaseMarkers;
    f.vm.databaseMarkers.clear();
    auto withoutMark = render(f.vm);
    f.vm.databaseMarkers = saved;

    const int cx = static_cast<int>(g.marginLeft + g.cellSize * 6.5);
    const int cy = static_cast<int>(g.marginTop + g.cellSize * 4.5);
    CHECK(rectDiffers(withMark, withoutMark, cx - 4, cy - 4, cx + 5, cy + 5));
}

TEST_CASE("UX-08: hovering an occupied cell draws no crosshair and changes nothing") {
    Fixture f;
    auto base = render(f.vm);
    f.vm.hoverMove = f.stone;
    auto hov = render(f.vm);
    CHECK_FALSE(rectDiffers(base, hov, 0, 0, kSize, kSize));   // whole frame identical
}

TEST_CASE("UX-08: hovered column letter and row number are emphasised in the margin") {
    for (double fgc : {0.1, 0.9}) {   // light theme (dark text) / dark theme (light text)
        Fixture f;
        BoardRenderer::Geometry g;
        auto base = render(f.vm, &g, fgc);
        f.vm.hoverMove = f.hover;
        auto hov = render(f.vm, nullptr, fgc);
        const int top = static_cast<int>(g.marginTop) - 1;
        const int left = static_cast<int>(g.marginLeft) - 1;

        auto colBox = [&](int col, const Surf &a, const Surf &b) {
            return rectDiffers(a, b, static_cast<int>(g.marginLeft + g.cellSize * (col + 0.1)), 0,
                               static_cast<int>(g.marginLeft + g.cellSize * (col + 0.9)), top);
        };
        auto rowBox = [&](int row, const Surf &a, const Surf &b) {
            return rectDiffers(a, b, 0, static_cast<int>(g.marginTop + g.cellSize * (row + 0.1)),
                               left, static_cast<int>(g.marginTop + g.cellSize * (row + 0.9)));
        };
        CHECK(colBox(f.hover.x, base, hov));
        CHECK(rowBox(f.hover.y, base, hov));
        CHECK_FALSE(colBox(10, base, hov));   // other labels unchanged
        CHECK_FALSE(rowBox(10, base, hov));
    }
}

TEST_CASE("UX-08: an occupied hover leaves the margin labels unchanged") {
    Fixture f;
    BoardRenderer::Geometry g;
    auto base = render(f.vm, &g);
    f.vm.hoverMove = f.stone;
    auto hov = render(f.vm);
    CHECK_FALSE(rectDiffers(base, hov, 0, 0, kSize, static_cast<int>(g.marginTop)));
    CHECK_FALSE(rectDiffers(base, hov, 0, 0, static_cast<int>(g.marginLeft), kSize));
}
