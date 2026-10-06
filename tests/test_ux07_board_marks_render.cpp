// Render regression tests for UX-07: an engine mark, a database mark and a
// variant mark on the SAME empty cell must all stay visible/distinguishable.
// Before UX-07 the filled database diamond, the variant dot and the engine tag
// disc were drawn on top of each other (and the engine mark hid the others).
//
// Rendered for real into a Cairo image surface and compared region by region
// against a render with that mark removed. Needs gtkmm/cairomm only, no display.

#include "vendor/doctest.h"

#include "model/board_view_model.h"
#include "model/game_state.h"
#include "ui/board_renderer.h"

#include <cmath>
#include <cstdint>

namespace {

using Surf = Cairo::RefPtr<Cairo::ImageSurface>;

constexpr int kSize = 400;   // 15x15 -> cell ~23.5 px (badges shown); 22x22 -> ~16 px (hidden)

Surf render(const BoardViewModel &vm, BoardRenderer::Geometry *geoOut = nullptr) {
    auto surf = Cairo::ImageSurface::create(Cairo::ImageSurface::Format::ARGB32, kSize, kSize);
    auto cr   = Cairo::Context::create(surf);
    BoardRenderer r(vm);
    r.draw(cr, kSize, kSize);
    if (geoOut) *geoOut = r.computeGeometry(kSize, kSize);
    surf->flush();
    return surf;
}

uint32_t px(const Surf &s, int x, int y) {
    const unsigned char *row = s->get_data() + y * s->get_stride();
    return reinterpret_cast<const uint32_t *>(row)[x];
}

/// True if any pixel in the (2*half+1)-square around (fx, fy) differs.
bool regionDiffers(const Surf &a, const Surf &b, double fx, double fy, int half = 1) {
    const int cx = static_cast<int>(fx), cy = static_cast<int>(fy);
    for (int y = cy - half; y <= cy + half; ++y)
        for (int x = cx - half; x <= cx + half; ++x)
            if (px(a, x, y) != px(b, x, y)) return true;
    return false;
}

/// True if any pixel inside the cell's box differs.
bool cellDiffers(const Surf &a, const Surf &b, const BoardRenderer::Geometry &g, Coord c) {
    const int x0 = static_cast<int>(g.marginLeft + g.cellSize * c.x);
    const int y0 = static_cast<int>(g.marginTop + g.cellSize * c.y);
    const int n  = static_cast<int>(g.cellSize);
    for (int y = y0; y < y0 + n; ++y)
        for (int x = x0; x < x0 + n; ++x)
            if (px(a, x, y) != px(b, x, y)) return true;
    return false;
}

struct Fixture {
    GameState      gs;
    BoardViewModel vm{gs};
    Coord          a{7, 7}, c{3, 3}, d{11, 3}, e{11, 11};   // `c` is the multi-mark cell
    BoardRenderer::Geometry g;

    explicit Fixture(int bs = 15, int branches = 2) : gs(bs) {
        const Coord a2{bs / 2, bs / 2};
        gs.makeMove(a2);
        gs.makeMove(c);
        gs.undoMove();                          // at A, child C
        if (branches > 1) { gs.makeMove(d); gs.undoMove(); }
        if (branches > 2) { gs.makeMove(e); gs.undoMove(); }
        gs.addDatabaseEntry([&] {
            DatabaseEntry de; de.pos = c; de.value = 80; de.label = "w3"; return de; }());
        AnalysisOverlay ov;
        ov.cells[c].tag        = "62%";
        ov.cells[c].tagWinrate = 0.62;
        gs.setAnalyzing(true);
        gs.setAnalysisOverlay(ov);
        vm.update();
        vm.hoverMove = Coord{};
        vm.pvPreview.clear();
        render(vm, &g);
    }

    double cx() const { return g.marginLeft + g.cellSize * (c.x + 0.5); }
    double cy() const { return g.marginTop + g.cellSize * (c.y + 0.5); }
    double stoneR() const { return g.cellSize * 0.44; }
};

} // namespace

TEST_CASE("UX-07: engine, database and variant marks on one empty cell are all visible") {
    Fixture f;
    REQUIRE(f.vm.searchOverlay.size() == 1);
    REQUIRE(f.vm.databaseMarkers.size() == 1);
    REQUIRE(f.vm.variantMarkers.size() == 2);   // C and D

    // Region probes (all relative to the cell centre).
    const double ringX = f.cx() - f.stoneR() * 0.80, ringY = f.cy();         // variant ring
    const double dbX   = f.cx() + f.g.cellSize * 0.30,                       // DB corner badge,
                 dbY   = f.cy() - f.g.cellSize * 0.30 - f.g.cellSize * 0.14; //   top tip of the diamond
    const double cntX  = f.cx() + f.g.cellSize * 0.30,                       // variant count badge
                 cntY  = f.cy() + f.g.cellSize * 0.30 - f.g.cellSize * 0.13;

    BoardViewModel &vm = f.vm;
    auto all = render(vm);

    auto saveVariants = vm.variantMarkers;
    auto saveDb       = vm.databaseMarkers;
    auto saveEngine   = vm.searchOverlay;

    vm.variantMarkers.clear(); vm.databaseMarkers.clear(); vm.searchOverlay.clear();
    auto none = render(vm);

    vm.searchOverlay = saveEngine;
    auto engineOnly = render(vm);
    vm.databaseMarkers = saveDb;
    auto engineDb = render(vm);
    vm.variantMarkers = saveVariants;

    // Engine mark: centre of the cell changes against the bare board.
    CHECK(regionDiffers(engineOnly, none, f.cx(), f.cy()));
    // Database mark adds a corner badge that the engine mark alone does not have.
    CHECK(regionDiffers(engineDb, engineOnly, dbX, dbY));
    // Variant mark adds a ring around the engine tag, and a count badge.
    CHECK(regionDiffers(all, engineDb, ringX, ringY));
    CHECK(regionDiffers(all, engineDb, cntX, cntY));
}

TEST_CASE("UX-07: without an engine mark the database mark stays a full outlined diamond") {
    Fixture f;
    f.vm.searchOverlay.clear();
    f.vm.variantMarkers.clear();
    f.vm.databaseMarkers[0].label.clear();   // keep label glyphs out of the interior probe
    auto dbFull = render(f.vm);
    f.vm.databaseMarkers.clear();
    auto none = render(f.vm);

    // Full-size diamond: left tip of the outline sits at ~0.55 stone radius.
    CHECK(regionDiffers(dbFull, none, f.cx() - f.stoneR() * 0.55, f.cy()));
    // Outlined, not filled: a point well inside the outline is (within
    // anti-aliasing noise of 1-2 levels) the bare wood; a fill would differ by
    // tens of levels there.
    const int ix = static_cast<int>(f.cx());
    const int iy = static_cast<int>(f.cy() - f.stoneR() * 0.55 * 0.45);
    auto chan = [](uint32_t v, int sh) { return static_cast<int>((v >> sh) & 0xFF); };
    for (int sh : {0, 8, 16})
        CHECK(std::abs(chan(px(dbFull, ix, iy), sh) - chan(px(none, ix, iy), sh)) <= 3);
}

TEST_CASE("UX-07: the best database entry has a thicker outline than a normal one") {
    Fixture f;
    f.vm.searchOverlay.clear();
    f.vm.variantMarkers.clear();
    REQUIRE(f.vm.databaseMarkers.size() == 1);

    f.vm.databaseMarkers[0].isBest = false;
    auto normal = render(f.vm);
    f.vm.databaseMarkers[0].isBest = true;
    auto best = render(f.vm);

    CHECK(cellDiffers(best, normal, f.g, f.c));
}

TEST_CASE("UX-07: best flag from the model drives the outline end to end") {
    Fixture f;
    f.vm.searchOverlay.clear();
    f.vm.variantMarkers.clear();
    // A second, higher-valued entry elsewhere makes `c` a non-best entry.
    DatabaseEntry hi; hi.pos = {9, 9}; hi.value = 900; hi.label = "w1";
    f.gs.addDatabaseEntry(hi);
    f.vm.update();
    f.vm.searchOverlay.clear();
    f.vm.variantMarkers.clear();
    auto notBest = render(f.vm);

    f.gs.addDatabaseEntry([&] { DatabaseEntry de; de.pos = f.c; de.value = 5000; de.label = "w3"; return de; }());
    f.vm.update();
    f.vm.searchOverlay.clear();
    f.vm.variantMarkers.clear();
    auto isBest = render(f.vm);

    // `c` became the best entry -> its outline changed (colour changes too with
    // the value, so this only guards that the best path renders without hiding it).
    CHECK(cellDiffers(isBest, notBest, f.g, f.c));
}

TEST_CASE("UX-07: variant branch count shows only when the node has more than one branch") {
    Fixture multi(15, 2);
    Fixture single(15, 1);
    REQUIRE(multi.vm.variantMarkers.size() == 2);
    REQUIRE(single.vm.variantMarkers.size() == 1);
    CHECK(multi.vm.variantMarkers[0].branchCount == 2);
    CHECK(single.vm.variantMarkers[0].branchCount == 1);

    auto badgeRegion = [](Fixture &f) {
        // Variant-only render vs ring-only (count forced to 1) render.
        f.vm.searchOverlay.clear();
        f.vm.databaseMarkers.clear();
        auto withCount = render(f.vm);
        for (auto &m : f.vm.variantMarkers) m.branchCount = 1;
        auto noCount = render(f.vm);
        return regionDiffers(withCount, noCount, f.cx() + f.g.cellSize * 0.30,
                             f.cy() + f.g.cellSize * 0.30 - f.g.cellSize * 0.13);
    };
    CHECK(badgeRegion(multi));
    CHECK_FALSE(badgeRegion(single));
}

TEST_CASE("UX-07: badges and counts are hidden on small cells (22x22 board)") {
    Fixture f(22, 2);
    REQUIRE(f.g.cellSize < 20.0);
    BoardViewModel &vm = f.vm;
    REQUIRE(vm.databaseMarkers.size() == 1);

    // DB badge: with an engine mark present, the DB mark must not change the cell.
    auto engineDb = render(vm);
    auto saveDb = vm.databaseMarkers;
    vm.databaseMarkers.clear();
    auto engineOnly = render(vm);
    CHECK_FALSE(regionDiffers(engineDb, engineOnly, f.cx() + f.g.cellSize * 0.30,
                              f.cy() - f.g.cellSize * 0.30 - f.g.cellSize * 0.14, 2));

    // Count: forcing the count to 1 changes nothing below the threshold.
    vm.databaseMarkers = saveDb;
    vm.searchOverlay.clear();
    auto withCount = render(vm);
    for (auto &m : vm.variantMarkers) m.branchCount = 1;
    auto noCount = render(vm);
    CHECK_FALSE(cellDiffers(withCount, noCount, f.g, f.c));
}
