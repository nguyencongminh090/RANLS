// UX-08 model tests (GTK-free logic): BoardViewModel::tooltipFor().
// One case per tooltip branch: engine tag / best / lost, database with each
// bound value, hasComment true/false (never comment text), variants, Renju
// forbidden point (indication only), occupied / off-board -> empty, and the
// existing toggles (showDatabase, showSearchOverlay) hiding their info.

#include "vendor/doctest.h"

#include "model/board_view_model.h"
#include "model/game_state.h"

#include <string>

namespace {

bool has(const std::string &hay, const char *needle) {
    return hay.find(needle) != std::string::npos;
}

DatabaseEntry dbEntry(Coord pos, int value, int depth, int bound, bool comment, const char *label = "w3") {
    DatabaseEntry e;
    e.pos = pos; e.value = value; e.depth = depth; e.bound = bound;
    e.hasComment = comment; e.label = label;
    return e;
}

void setOverlay(GameState &gs, const AnalysisOverlay &ov) {
    gs.setAnalyzing(true);
    gs.setAnalysisOverlay(ov);
}

} // namespace

TEST_CASE("UX-08: empty cell with nothing on it has no tooltip") {
    GameState gs(15);
    BoardViewModel vm(gs);
    vm.update();
    CHECK(vm.tooltipFor({3, 3}).empty());
}

TEST_CASE("UX-08: occupied and off-board cells have no tooltip") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.makeMove({7, 7});
    gs.addDatabaseEntry(dbEntry({7, 7}, 50, 9, 0, true));   // stale entry under a stone
    vm.update();
    CHECK(vm.tooltipFor({7, 7}).empty());
    CHECK(vm.tooltipFor({-1, 3}).empty());
    CHECK(vm.tooltipFor({15, 3}).empty());
    CHECK(vm.tooltipFor(Coord{}).empty());
}

TEST_CASE("UX-08: engine tag shows tag text and tag depth") {
    GameState gs(15);
    BoardViewModel vm(gs);
    AnalysisOverlay ov;
    ov.cells[{4, 4}].tag = "62%";
    ov.cells[{4, 4}].tagDepth = 12;
    ov.cells[{4, 4}].tagWinrate = 0.62;
    setOverlay(gs, ov);
    vm.update();

    const std::string t = vm.tooltipFor({4, 4});
    CHECK(has(t, "62%"));
    CHECK(has(t, "depth 12"));
    CHECK_FALSE(has(t, "Best move"));
}

TEST_CASE("UX-08: best engine move says best move (tagged and untagged)") {
    GameState gs(15);
    BoardViewModel vm(gs);
    AnalysisOverlay ov;
    ov.bestMove = {5, 5};                       // no cell entry: untagged best
    ov.cells[{6, 6}].tag = "+M7";
    setOverlay(gs, ov);
    vm.update();
    CHECK(has(vm.tooltipFor({5, 5}), "Best move"));

    ov.bestMove = {6, 6};                       // now the tagged cell is best too
    setOverlay(gs, ov);
    vm.update();
    const std::string t = vm.tooltipFor({6, 6});
    CHECK(has(t, "+M7"));
    CHECK(has(t, "Best move"));
}

TEST_CASE("UX-08: lost engine move says losing move") {
    GameState gs(15);
    BoardViewModel vm(gs);
    AnalysisOverlay ov;
    ov.cells[{2, 9}].lost = true;
    setOverlay(gs, ov);
    vm.update();
    CHECK(has(vm.tooltipFor({2, 9}), "Losing move"));
}

TEST_CASE("UX-08: examined / examining dots add no tooltip") {
    GameState gs(15);
    BoardViewModel vm(gs);
    AnalysisOverlay ov;
    ov.cells[{1, 1}].pos = 1;
    ov.cells[{2, 2}].pos = 2;
    setOverlay(gs, ov);
    vm.update();
    REQUIRE(vm.searchOverlay.size() == 2);
    CHECK(vm.tooltipFor({1, 1}).empty());
    CHECK(vm.tooltipFor({2, 2}).empty());
}

TEST_CASE("UX-08: showSearchOverlay off hides engine info") {
    GameState gs(15);
    BoardViewModel vm(gs);
    AnalysisOverlay ov;
    ov.cells[{4, 4}].tag = "62%";
    ov.bestMove = {4, 4};
    setOverlay(gs, ov);
    ViewConfig vc = gs.viewConfig();
    vc.showSearchOverlay = false;
    gs.setViewConfig(vc);
    vm.update();
    CHECK(vm.tooltipFor({4, 4}).empty());
}

TEST_CASE("UX-08: engine mark disappears from the tooltip when analysis stops") {
    GameState gs(15);
    BoardViewModel vm(gs);
    AnalysisOverlay ov;
    ov.cells[{4, 4}].tag = "62%";
    setOverlay(gs, ov);
    vm.update();
    REQUIRE_FALSE(vm.tooltipFor({4, 4}).empty());
    gs.setAnalyzing(false);
    vm.update();
    CHECK(vm.tooltipFor({4, 4}).empty());
}

TEST_CASE("UX-08: database tooltip shows value, depth and each bound as text") {
    struct Row { int bound; const char *text; };
    for (const Row &r : {Row{0, "Exact"}, Row{1, "Alpha"}, Row{2, "Beta"}}) {
        GameState gs(15);
        BoardViewModel vm(gs);
        gs.addDatabaseEntry(dbEntry({3, 3}, 123, 17, r.bound, false));
        vm.update();
        const std::string t = vm.tooltipFor({3, 3});
        CHECK(has(t, "w3"));
        CHECK(has(t, "123"));
        CHECK(has(t, "depth 17"));
        CHECK(has(t, r.text));
    }
}

TEST_CASE("UX-08: database boardText wins over label; unknown bound is not mislabelled") {
    GameState gs(15);
    BoardViewModel vm(gs);
    DatabaseEntry e = dbEntry({3, 3}, 1, 2, 9, false, "L");
    e.boardText = "VCF5";
    gs.addDatabaseEntry(e);
    vm.update();
    const std::string t = vm.tooltipFor({3, 3});
    CHECK(has(t, "VCF5"));
    CHECK(has(t, "Unknown"));
    CHECK_FALSE(has(t, "Exact"));
}

TEST_CASE("UX-08: database best flag and has-comment flag (never comment text)") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.addDatabaseEntry(dbEntry({3, 3}, 200, 10, 0, true));
    gs.addDatabaseEntry(dbEntry({4, 4}, 100, 10, 0, false));
    vm.update();

    const std::string a = vm.tooltipFor({3, 3});
    CHECK(has(a, "Best database move"));
    CHECK(has(a, "Has comment"));

    const std::string b = vm.tooltipFor({4, 4});
    CHECK_FALSE(has(b, "Best database move"));
    CHECK_FALSE(has(b, "omment"));
}

TEST_CASE("UX-08: showDatabase off hides database info") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.addDatabaseEntry(dbEntry({3, 3}, 200, 10, 0, true));
    ViewConfig vc = gs.viewConfig();
    vc.showDatabase = false;
    gs.setViewConfig(vc);
    vm.update();
    CHECK(vm.tooltipFor({3, 3}).empty());
}

TEST_CASE("UX-08: database info does not depend on showSearchOverlay") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.addDatabaseEntry(dbEntry({3, 3}, 200, 10, 0, false));
    ViewConfig vc = gs.viewConfig();
    vc.showSearchOverlay = false;
    gs.setViewConfig(vc);
    vm.update();
    CHECK(has(vm.tooltipFor({3, 3}), "Database"));
}

TEST_CASE("UX-08: variant marker shows the variation count (singular and plural)") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.makeMove({7, 7});
    gs.makeMove({3, 3});
    gs.undoMove();                       // at (7,7); one child (3,3)
    vm.update();
    CHECK(has(vm.tooltipFor({3, 3}), "1 variation"));
    CHECK_FALSE(has(vm.tooltipFor({3, 3}), "variations"));

    gs.makeMove({11, 3});
    gs.undoMove();                       // two children
    vm.update();
    CHECK(has(vm.tooltipFor({3, 3}), "2 variations"));
    CHECK(has(vm.tooltipFor({11, 3}), "2 variations"));
}

TEST_CASE("UX-08: Renju forbidden point says forbidden but still playable") {
    GameState gs(15);
    gs.setRule(GameRule::Renju);
    BoardViewModel vm(gs);
    // Black double-four setup (as in test_ui03), White filler on column 0.
    const Coord black[] = {{5, 5}, {6, 5}, {7, 5}, {8, 6}, {8, 7}, {8, 8}};
    int w = 0;
    for (const Coord &b : black) {
        REQUIRE(gs.makeMove(b));
        REQUIRE(gs.makeMove({0, 2 * w++}));
    }
    vm.update();
    REQUIRE_FALSE(vm.forbiddenPoints.empty());
    const std::string t = vm.tooltipFor({8, 5});
    CHECK(has(t, "Forbidden for Black"));
    CHECK(has(t, "still playable"));
    CHECK(vm.tooltipFor({10, 10}).empty());
}

TEST_CASE("UX-08: marks on the same cell are all described, one per line") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.makeMove({7, 7});
    gs.makeMove({3, 3});
    gs.undoMove();
    gs.addDatabaseEntry(dbEntry({3, 3}, 80, 5, 1, false));
    AnalysisOverlay ov;
    ov.cells[{3, 3}].tag = "62%";
    setOverlay(gs, ov);
    vm.update();
    const std::string t = vm.tooltipFor({3, 3});
    CHECK(has(t, "62%"));
    CHECK(has(t, "Alpha"));
    CHECK(has(t, "1 variation"));
    CHECK(has(t, "\n"));
}
