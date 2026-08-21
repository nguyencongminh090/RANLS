// Regression tests for UI-03: Renju forbidden-point detection (RenjuRule,
// src/model/renju_rule.h/.cpp). Pins the double-four, double-three, overline,
// and "five overrides forbidden" shapes against concrete board layouts so a
// future change to the detection algorithm can't silently regress any of
// them. These are hand-verified against standard Renju rule definitions
// (double-three/double-four/overline), cross-checked against the Rapfi
// engine's own documented semantics (docs/protocol.md §7.3: "every empty
// cell for which checkForbiddenPoint() holds (Renju-only concept — 3-3,
// 4-4, overline)").

#include "vendor/doctest.h"

#include "model/renju_rule.h"

namespace {
Coord at(int x, int y) { return Coord{x, y}; }
}

TEST_CASE("RenjuRule: isolated stone is not forbidden") {
    BoardState board(15);
    board.placeStone(at(7, 7), Stone::Black);
    CHECK_FALSE(RenjuRule::isForbidden(board, at(8, 8)));
}

TEST_CASE("RenjuRule: exact five always overrides forbidden") {
    BoardState board(15);
    // Four in a row; the 5th stone completes an exact five.
    board.placeStone(at(2, 5), Stone::Black);
    board.placeStone(at(3, 5), Stone::Black);
    board.placeStone(at(4, 5), Stone::Black);
    board.placeStone(at(5, 5), Stone::Black);

    CHECK_FALSE(RenjuRule::isForbidden(board, at(6, 5)));
}

TEST_CASE("RenjuRule: overline (6-in-a-row) is forbidden") {
    BoardState board(15);
    // Five already on the board (an artificial/analysis-only setup — see
    // renju_rule.h's doc comment: this module supports deliberately building
    // illegal positions). Placing a 6th in line must be flagged forbidden,
    // not accepted as a "five".
    board.placeStone(at(1, 5), Stone::Black);
    board.placeStone(at(2, 5), Stone::Black);
    board.placeStone(at(3, 5), Stone::Black);
    board.placeStone(at(4, 5), Stone::Black);
    board.placeStone(at(5, 5), Stone::Black);

    CHECK(RenjuRule::isForbidden(board, at(6, 5)));
}

TEST_CASE("RenjuRule: double-four (4-4) is forbidden") {
    BoardState board(15);
    // Horizontal three open on both ends, sharing the corner point (8,5)
    // with a vertical three open on both ends below it.
    board.placeStone(at(5, 5), Stone::Black);
    board.placeStone(at(6, 5), Stone::Black);
    board.placeStone(at(7, 5), Stone::Black);

    board.placeStone(at(8, 6), Stone::Black);
    board.placeStone(at(8, 7), Stone::Black);
    board.placeStone(at(8, 8), Stone::Black);

    Coord pos = at(8, 5);
    REQUIRE(board.stoneAt(pos) == Stone::Empty);
    CHECK(RenjuRule::isForbidden(board, pos));
}

TEST_CASE("RenjuRule: a single four (not double) is NOT forbidden") {
    BoardState board(15);
    // Same horizontal three as above, but no crossing vertical four.
    board.placeStone(at(5, 5), Stone::Black);
    board.placeStone(at(6, 5), Stone::Black);
    board.placeStone(at(7, 5), Stone::Black);

    Coord pos = at(8, 5);
    CHECK_FALSE(RenjuRule::isForbidden(board, pos));
}

TEST_CASE("RenjuRule: double-three (3-3) is forbidden") {
    BoardState board(15);
    // Horizontal two (+pos = open three) crossing a vertical two (+pos =
    // open three) at (7,5). Neither line has a 3rd pre-existing stone, so
    // neither is already a four -- this exercises the three-detection path
    // specifically (not double-four).
    board.placeStone(at(5, 5), Stone::Black);
    board.placeStone(at(6, 5), Stone::Black);

    board.placeStone(at(7, 6), Stone::Black);
    board.placeStone(at(7, 7), Stone::Black);

    Coord pos = at(7, 5);
    REQUIRE(board.stoneAt(pos) == Stone::Empty);
    CHECK(RenjuRule::isForbidden(board, pos));
}

TEST_CASE("RenjuRule: a single open three (not double) is NOT forbidden") {
    BoardState board(15);
    board.placeStone(at(5, 5), Stone::Black);
    board.placeStone(at(6, 5), Stone::Black);

    Coord pos = at(7, 5);
    CHECK_FALSE(RenjuRule::isForbidden(board, pos));
}

TEST_CASE("RenjuRule: forbiddenPoints() collects the forbidden coordinate") {
    BoardState board(15);
    board.placeStone(at(5, 5), Stone::Black);
    board.placeStone(at(6, 5), Stone::Black);
    board.placeStone(at(7, 5), Stone::Black);
    board.placeStone(at(8, 6), Stone::Black);
    board.placeStone(at(8, 7), Stone::Black);
    board.placeStone(at(8, 8), Stone::Black);

    auto points = RenjuRule::forbiddenPoints(board);
    bool found = false;
    for (auto &c : points) {
        if (c == at(8, 5)) found = true;
    }
    CHECK(found);
}
