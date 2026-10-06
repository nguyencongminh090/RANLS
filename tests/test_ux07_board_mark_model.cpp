// UX-07 model tests (GTK-free): BoardViewModel::update() additions.
//   - moveNumberAt(): Coord -> 1-based move number, built once per update()
//     (replaces the per-stone std::find in BoardRenderer::drawStones).
//   - database Marker::isBest: entry with the highest `value`; ties flag all.
//   - variant Marker::branchCount: children of the current tree node, listed
//     in insertion order (index 0 = first-added child = main continuation).

#include "vendor/doctest.h"

#include "model/board_view_model.h"
#include "model/game_state.h"

namespace {

DatabaseEntry entry(Coord pos, int value, const char *label = "") {
    DatabaseEntry e;
    e.pos   = pos;
    e.value = value;
    e.label = label;
    return e;
}

} // namespace

TEST_CASE("UX-07: moveNumberAt maps each played stone to its 1-based move number") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.makeMove({7, 7});
    gs.makeMove({8, 7});
    gs.makeMove({3, 12});
    vm.update();

    CHECK(vm.moveNumberAt({7, 7}) == 1);
    CHECK(vm.moveNumberAt({8, 7}) == 2);
    CHECK(vm.moveNumberAt({3, 12}) == 3);
    // Must agree with the linear search it replaces, for every stone.
    for (const auto &[pos, stone] : vm.stones) {
        (void)stone;
        int expect = 0;
        for (size_t i = 0; i < vm.moveHistory.size(); ++i)
            if (vm.moveHistory[i] == pos) { expect = static_cast<int>(i) + 1; break; }
        CHECK(vm.moveNumberAt(pos) == expect);
    }
}

TEST_CASE("UX-07: moveNumberAt is 0 for empty / out-of-range cells and tracks undo") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.makeMove({7, 7});
    gs.makeMove({8, 8});
    vm.update();

    CHECK(vm.moveNumberAt({0, 0}) == 0);
    CHECK(vm.moveNumberAt({-1, 3}) == 0);
    CHECK(vm.moveNumberAt({15, 3}) == 0);
    CHECK(vm.moveNumberAt(Coord{}) == 0);

    gs.undoMove();
    vm.update();
    CHECK(vm.moveNumberAt({7, 7}) == 1);
    CHECK(vm.moveNumberAt({8, 8}) == 0);   // rebuilt, not stale
}

TEST_CASE("UX-07: moveNumberAt survives a board-size change") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.makeMove({14, 14});
    vm.update();
    CHECK(vm.moveNumberAt({14, 14}) == 1);

    GameState small(9);
    BoardViewModel vm2(small);
    small.makeMove({8, 8});
    vm2.update();
    CHECK(vm2.moveNumberAt({8, 8}) == 1);
    CHECK(vm2.moveNumberAt({9, 9}) == 0);
}

TEST_CASE("UX-07: database marker isBest flags the highest-value entry only") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.addDatabaseEntry(entry({3, 3}, 50));
    gs.addDatabaseEntry(entry({5, 5}, 300));
    gs.addDatabaseEntry(entry({9, 9}, -120));
    vm.update();

    REQUIRE(vm.databaseMarkers.size() == 3);
    for (const auto &m : vm.databaseMarkers)
        CHECK(m.isBest == (m.pos == Coord{5, 5}));
}

TEST_CASE("UX-07: database isBest ties flag every tied entry; lone entry is best") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.addDatabaseEntry(entry({3, 3}, 100));
    vm.update();
    REQUIRE(vm.databaseMarkers.size() == 1);
    CHECK(vm.databaseMarkers[0].isBest);

    gs.addDatabaseEntry(entry({4, 4}, 100));
    gs.addDatabaseEntry(entry({6, 6}, -5));
    vm.update();
    REQUIRE(vm.databaseMarkers.size() == 3);
    for (const auto &m : vm.databaseMarkers)
        CHECK(m.isBest == (m.pos != Coord{6, 6}));
}

TEST_CASE("UX-07: database isBest handles all-negative values and showDatabase=off") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.addDatabaseEntry(entry({3, 3}, -400));
    gs.addDatabaseEntry(entry({4, 4}, -50));
    vm.update();
    REQUIRE(vm.databaseMarkers.size() == 2);
    for (const auto &m : vm.databaseMarkers)
        CHECK(m.isBest == (m.pos == Coord{4, 4}));

    ViewConfig cfg = gs.viewConfig();
    cfg.showDatabase = false;
    gs.setViewConfig(cfg);
    vm.update();
    CHECK(vm.databaseMarkers.empty());
}

TEST_CASE("UX-07: variant markers carry the branch count of the current node") {
    GameState gs(15);
    BoardViewModel vm(gs);
    const Coord a{7, 7}, c{3, 3}, d{11, 3}, e{11, 11};

    gs.makeMove(a);
    gs.makeMove(c);
    gs.undoMove();            // back at A with one child (C)
    vm.update();
    REQUIRE(vm.variantMarkers.size() == 1);
    CHECK(vm.variantMarkers[0].pos == c);
    CHECK(vm.variantMarkers[0].branchCount == 1);

    gs.makeMove(d);           // second branch under A
    gs.undoMove();
    gs.makeMove(e);           // third
    gs.undoMove();
    vm.update();
    REQUIRE(vm.variantMarkers.size() == 3);
    // Insertion order: index 0 is the first-added (main) continuation.
    CHECK(vm.variantMarkers[0].pos == c);
    CHECK(vm.variantMarkers[1].pos == d);
    CHECK(vm.variantMarkers[2].pos == e);
    for (const auto &m : vm.variantMarkers)
        CHECK(m.branchCount == 3);
}

TEST_CASE("UX-07: a leaf node has no variant markers") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.makeMove({7, 7});
    vm.update();
    CHECK(vm.variantMarkers.empty());
}
