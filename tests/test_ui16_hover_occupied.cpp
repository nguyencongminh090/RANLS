// Regression tests for UI-16: BoardRenderer::drawHover and drawPVHighlight
// painted a translucent stone on top of cells that already hold a real
// stone. Neither layer checked occupancy, so hovering a stone (or a PV path
// crossing one) visibly tinted it.
//
// Rendered for real into a Cairo image surface: the pixel at the centre of the
// occupied cell must be identical with and without the hover / PV preview.
// Empty cells must still show the hover (guards against over-suppression).
// Needs gtkmm/cairomm only, no display server.

#include "vendor/doctest.h"

#include "model/board_view_model.h"
#include "model/game_state.h"
#include "ui/board_renderer.h"

#include <cstdint>

namespace {

constexpr int kSize = 400;

uint32_t pixelAt(const Cairo::RefPtr<Cairo::ImageSurface> &surf, double fx, double fy) {
    surf->flush();
    int x = static_cast<int>(fx), y = static_cast<int>(fy);
    const unsigned char *row = surf->get_data() + y * surf->get_stride();
    return reinterpret_cast<const uint32_t *>(row)[x];
}

/// Render `vm` and return the pixel at the centre of board cell `c`.
uint32_t renderCentre(BoardViewModel &vm, Coord c) {
    auto surf = Cairo::ImageSurface::create(Cairo::ImageSurface::Format::ARGB32, kSize, kSize);
    auto cr   = Cairo::Context::create(surf);
    BoardRenderer r(vm);
    r.draw(cr, kSize, kSize);
    auto g = r.computeGeometry(kSize, kSize);
    return pixelAt(surf, g.marginLeft + g.cellSize * (c.x + 0.5),
                         g.marginTop  + g.cellSize * (c.y + 0.5));
}

struct Fixture {
    GameState gs{15};
    BoardViewModel vm{gs};
    Coord stone{7, 7};
    Coord empty{3, 3};
    Fixture() {
        gs.makeMove(stone);        // Black at (7,7); White to move next
        vm.update();
        vm.hoverMove = Coord{};
        vm.pvPreview.clear();
    }
};

} // namespace

TEST_CASE("UI-16: hovering an occupied cell does not tint the stone") {
    Fixture f;
    const uint32_t before = renderCentre(f.vm, f.stone);

    f.vm.hoverMove = f.stone;
    CHECK(renderCentre(f.vm, f.stone) == before);
}

TEST_CASE("UI-16: hovering an empty cell still draws the hover stone") {
    Fixture f;
    const uint32_t before = renderCentre(f.vm, f.empty);

    f.vm.hoverMove = f.empty;
    CHECK(renderCentre(f.vm, f.empty) != before);
}

TEST_CASE("UI-16: a PV ghost path crossing an occupied cell does not paint over it") {
    Fixture f;
    const uint32_t occBefore = renderCentre(f.vm, f.stone);
    const uint32_t emptyBefore = renderCentre(f.vm, f.empty);

    f.vm.pvPreview = {f.empty, f.stone};   // 2nd ghost lands on the real stone
    CHECK(renderCentre(f.vm, f.stone) == occBefore);
    CHECK(renderCentre(f.vm, f.empty) != emptyBefore);   // ghosts on empty cells still drawn
}
