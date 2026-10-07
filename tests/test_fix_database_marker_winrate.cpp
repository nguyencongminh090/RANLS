// Regression test: a database marker's heat colour (`Marker::eval`) must follow
// the engine's own display label, as the original Yixin-Board does (main.c
// ~766/823: "W.." -> 100, "L.." -> 0, "NN%" -> NN), not be re-derived from the
// raw record value with a hardcoded sigmoid(value/200).
//
// Rapfi labels an exact-bound record with clamp(int(valueToWinRate(-value)*100)):
// the engine's scale (≈116 for yixin-net, not 200) AND the opposite sign to the
// raw `value`. The old conversion got both wrong, so the colour disagreed with
// the "NN%" text drawn on the same marker.
//
// This transcript IS the permanent fixture -- do not delete it.

#include "vendor/doctest.h"

#include "model/board_view_model.h"
#include "model/game_state.h"

namespace {

DatabaseEntry entry(Coord pos, int value, const char *label) {
    DatabaseEntry e;
    e.pos   = pos;
    e.value = value;
    e.label = label;
    return e;
}

double evalAt(BoardViewModel &vm, Coord c) {
    for (const auto &m : vm.databaseMarkers)
        if (m.pos == c) return m.eval;
    return -999.0;
}

}  // namespace

TEST_CASE("database marker eval follows the engine's NN% label, not sigmoid(value/200)") {
    GameState gs(15);
    BoardViewModel vm(gs);
    // value 221 -> engine label "13%" (scale 116.37, sign flipped); old code gave 0.7512.
    gs.addDatabaseEntry(entry({3, 3}, 221, "13%"));
    gs.addDatabaseEntry(entry({5, 5}, -221, "87%"));
    vm.update();
    CHECK(evalAt(vm, {3, 3}) == doctest::Approx(0.13));
    CHECK(evalAt(vm, {5, 5}) == doctest::Approx(0.87));
}

TEST_CASE("database marker eval: W/L labels are 1.0/0.0; unscored labels have no heat colour") {
    GameState gs(15);
    BoardViewModel vm(gs);
    gs.addDatabaseEntry(entry({3, 3}, 30000, "W5"));
    gs.addDatabaseEntry(entry({4, 4}, -30000, "L3"));
    gs.addDatabaseEntry(entry({5, 5}, 100, "D"));
    gs.addDatabaseEntry(entry({6, 6}, 100, ""));      // non-exact bound: engine prints no label
    vm.update();
    CHECK(evalAt(vm, {3, 3}) == doctest::Approx(1.0));
    CHECK(evalAt(vm, {4, 4}) == doctest::Approx(0.0));
    CHECK(evalAt(vm, {5, 5}) == doctest::Approx(-1.0));
    CHECK(evalAt(vm, {6, 6}) == doctest::Approx(-1.0));
}
