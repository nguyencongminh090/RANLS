// Regression test for UI-03: BoardState::checkWin() must be rule-aware about
// overline (6-in-a-row), matching the Rapfi engine's own win condition
// (game/pattern.cpp: `CheckOverline = R == STANDARD || (R == RENJU && Black)`).
// Before this fix, checkWin() ignored the rule entirely and treated ANY run
// of 5-or-more as a win everywhere -- silently diverging from what the
// engine would call a win under Standard (and under Renju for Black).

#include "vendor/doctest.h"

#include "model/board_state.h"

namespace {
Coord at(int x, int y) { return Coord{x, y}; }

// A 6-in-a-row (overline) of `stone`, occupying x in [1,6] on row `y`.
BoardState makeOverline(Stone stone, int y = 5)
{
    BoardState board(15);
    for (int x = 1; x <= 6; ++x) board.placeStone(at(x, y), stone);
    return board;
}

// An exact 5-in-a-row of `stone`, occupying x in [1,5] on row `y`.
BoardState makeExactFive(Stone stone, int y = 6)
{
    BoardState board(15);
    for (int x = 1; x <= 5; ++x) board.placeStone(at(x, y), stone);
    return board;
}
} // namespace

TEST_CASE("checkWin: Freestyle -- overline still wins") {
    BoardState board = makeOverline(Stone::Black);
    CHECK(board.checkWin(at(3, 5), GameRule::Freestyle));
}

TEST_CASE("checkWin: Freestyle -- exact five wins") {
    BoardState board = makeExactFive(Stone::Black);
    CHECK(board.checkWin(at(3, 6), GameRule::Freestyle));
}

TEST_CASE("checkWin: Standard -- overline does NOT win (either color)") {
    BoardState black = makeOverline(Stone::Black);
    CHECK_FALSE(black.checkWin(at(3, 5), GameRule::Standard));

    BoardState white = makeOverline(Stone::White);
    CHECK_FALSE(white.checkWin(at(3, 5), GameRule::Standard));
}

TEST_CASE("checkWin: Standard -- exact five still wins") {
    BoardState board = makeExactFive(Stone::Black);
    CHECK(board.checkWin(at(3, 6), GameRule::Standard));
}

TEST_CASE("checkWin: Renju -- Black overline does NOT win, White overline DOES") {
    BoardState black = makeOverline(Stone::Black);
    CHECK_FALSE(black.checkWin(at(3, 5), GameRule::Renju));

    BoardState white = makeOverline(Stone::White);
    CHECK(white.checkWin(at(3, 5), GameRule::Renju));
}

TEST_CASE("checkWin: Renju -- exact five wins for either color") {
    BoardState black = makeExactFive(Stone::Black);
    CHECK(black.checkWin(at(3, 6), GameRule::Renju));

    BoardState white = makeExactFive(Stone::White);
    CHECK(white.checkWin(at(3, 6), GameRule::Renju));
}

TEST_CASE("checkWin: no stone at pos never wins") {
    BoardState board(15);
    CHECK_FALSE(board.checkWin(at(7, 7), GameRule::Freestyle));
}
