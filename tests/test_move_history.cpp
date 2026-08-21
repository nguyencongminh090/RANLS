// Regression tests for MoveHistory's cursor invariants.
//
// MoveHistory has three overlapping accessors that callers mix (see
// src/model/game_state.cpp and src/model/board_view_model.cpp):
//   - moveCount()    -> cursor_          (moves "played" up to the cursor)
//   - totalMoves()   -> moves_.size()    (moves recorded, including redo-able ones)
//   - currentIndex() -> cursor_ - 1      (0-based index of the last played move)
//
// These tests pin the exact relationship between the three across the
// clear/add/undo/redo lifecycle so a future edit that breaks the invariant
// fails loudly here instead of surfacing as a silent off-by-one in the UI.

#include "vendor/doctest.h"

#include "model/move_history.h"

namespace {

Coord at(int x, int y) { return Coord{x, y}; }

} // namespace

TEST_CASE("MoveHistory: empty history") {
    MoveHistory h;
    CHECK(h.moveCount() == 0);
    CHECK(h.totalMoves() == 0);
    CHECK(h.currentIndex() == -1);
    CHECK_FALSE(h.canUndo());
    CHECK_FALSE(h.canRedo());
}

TEST_CASE("MoveHistory: addMove keeps moveCount == totalMoves == currentIndex + 1") {
    MoveHistory h;
    h.addMove(at(0, 0));
    CHECK(h.moveCount() == 1);
    CHECK(h.totalMoves() == 1);
    CHECK(h.currentIndex() == 0);

    h.addMove(at(1, 0));
    h.addMove(at(2, 0));
    CHECK(h.moveCount() == 3);
    CHECK(h.totalMoves() == 3);
    CHECK(h.currentIndex() == 2);
    CHECK(h.moves()[h.currentIndex()] == at(2, 0));
}

TEST_CASE("MoveHistory: undo decrements moveCount/currentIndex but not totalMoves") {
    MoveHistory h;
    h.addMove(at(0, 0));
    h.addMove(at(1, 0));
    h.addMove(at(2, 0));

    Coord undone = h.undoMove();
    CHECK(undone == at(2, 0));
    CHECK(h.moveCount() == 2);
    CHECK(h.totalMoves() == 3); // redo-able move still recorded
    CHECK(h.currentIndex() == 1);
    CHECK(h.canRedo());
}

TEST_CASE("MoveHistory: redo restores moveCount/currentIndex, totalMoves unchanged") {
    MoveHistory h;
    h.addMove(at(0, 0));
    h.addMove(at(1, 0));
    h.undoMove();

    Coord redone = h.redoMove();
    CHECK(redone == at(1, 0));
    CHECK(h.moveCount() == 2);
    CHECK(h.totalMoves() == 2);
    CHECK(h.currentIndex() == 1);
    CHECK_FALSE(h.canRedo());
}

TEST_CASE("MoveHistory: addMove after undo truncates redo history (totalMoves shrinks)") {
    MoveHistory h;
    h.addMove(at(0, 0));
    h.addMove(at(1, 0));
    h.addMove(at(2, 0));
    h.undoMove(); // cursor back to 2, moves_ still size 3

    h.addMove(at(9, 9)); // should truncate the redo-able move at(2,0)
    CHECK(h.moveCount() == 3);
    CHECK(h.totalMoves() == 3); // truncated then re-appended, net same size
    CHECK(h.currentIndex() == 2);
    CHECK(h.moves()[h.currentIndex()] == at(9, 9));
    CHECK_FALSE(h.canRedo());
}

TEST_CASE("MoveHistory: undo on empty history is a no-op") {
    MoveHistory h;
    Coord undone = h.undoMove();
    CHECK(undone == Coord{});
    CHECK(h.moveCount() == 0);
    CHECK(h.currentIndex() == -1);
}

TEST_CASE("MoveHistory: redo with nothing to redo is a no-op") {
    MoveHistory h;
    h.addMove(at(0, 0));
    Coord redone = h.redoMove();
    CHECK(redone == Coord{});
    CHECK(h.moveCount() == 1);
    CHECK(h.currentIndex() == 0);
}

TEST_CASE("MoveHistory: clear resets moveCount, totalMoves, and currentIndex together") {
    MoveHistory h;
    h.addMove(at(0, 0));
    h.addMove(at(1, 0));
    h.clear();
    CHECK(h.moveCount() == 0);
    CHECK(h.totalMoves() == 0);
    CHECK(h.currentIndex() == -1);
}
