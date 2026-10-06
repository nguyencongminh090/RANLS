// Regression tests for UI-17: BoardRenderer layers shared one Cairo context
// with no save()/restore(), so drawForbiddenPoints' select_font_face(BOLD)
// leaked into every later text layer (database labels, search tags, PV ghost
// numbers). The label weight therefore depended on whether any Renju
// forbidden point existed.
//
// Rendered for real into a Cairo image surface: a database marker label drawn
// in a cell far from any forbidden point must be pixel-identical with and
// without forbidden points elsewhere on the board. No display server needed.

#include "vendor/doctest.h"

#include "model/board_view_model.h"
#include "model/game_state.h"
#include "ui/board_renderer.h"

#include <cstdint>
#include <vector>

namespace {

constexpr int kSize = 400;

/// Render `vm`; return the ARGB pixels of the square around the centre of `c`.
std::vector<uint32_t> renderCell(BoardViewModel &vm, Coord c) {
    auto surf = Cairo::ImageSurface::create(Cairo::ImageSurface::Format::ARGB32, kSize, kSize);
    auto cr   = Cairo::Context::create(surf);
    BoardRenderer r(vm);
    r.draw(cr, kSize, kSize);
    surf->flush();
    auto g = r.computeGeometry(kSize, kSize);
    int x0 = static_cast<int>(g.marginLeft + g.cellSize * c.x);
    int y0 = static_cast<int>(g.marginTop  + g.cellSize * c.y);
    int n  = static_cast<int>(g.cellSize);
    std::vector<uint32_t> px;
    for (int y = y0; y < y0 + n; ++y) {
        const unsigned char *row = surf->get_data() + y * surf->get_stride();
        for (int x = x0; x < x0 + n; ++x)
            px.push_back(reinterpret_cast<const uint32_t *>(row)[x]);
    }
    return px;
}

struct Fixture {
    GameState gs{15};
    BoardViewModel vm{gs};
    Coord labelCell{10, 10};
    Fixture() {
        vm.update();
        BoardViewModel::Marker m;
        m.pos   = labelCell;
        m.label = "W12";
        m.eval  = 0.7;
        vm.databaseMarkers = {m};
    }
};

} // namespace

TEST_CASE("UI-17: database label rendering does not depend on forbidden points elsewhere") {
    Fixture f;
    const auto without = renderCell(f.vm, f.labelCell);

    f.vm.forbiddenPoints = {Coord{2, 2}};   // draws a BOLD "X" in a different cell
    const auto with = renderCell(f.vm, f.labelCell);

    CHECK(with == without);
}

TEST_CASE("UI-17: search-overlay tag rendering does not depend on forbidden points elsewhere") {
    Fixture f;
    f.vm.databaseMarkers.clear();
    BoardViewModel::SearchOverlayMark t;
    t.pos     = f.labelCell;
    t.kind    = BoardViewModel::SearchOverlayMark::Kind::Tag;
    t.label   = "62%";
    t.winrate = 0.62;
    f.vm.searchOverlay = {t};
    const auto without = renderCell(f.vm, f.labelCell);

    f.vm.forbiddenPoints = {Coord{2, 2}};
    const auto with = renderCell(f.vm, f.labelCell);

    CHECK(with == without);
}
