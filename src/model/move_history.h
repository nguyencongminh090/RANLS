#pragma once

#include "board_state.h"

#include <vector>

/// Linear move history for the current game line.
/// Supports undo/redo by maintaining a cursor into the move list.
class MoveHistory {
public:
    /// Clear all moves.
    void clear();

    /// Add a move at the current cursor position (truncates any redo history).
    void addMove(Coord pos);

    /// Undo the last move. Returns the undone move, or invalid Coord if empty.
    Coord undoMove();

    /// Redo a previously undone move. Returns the move, or invalid Coord.
    Coord redoMove();

    /// Can undo?
    bool canUndo() const;

    /// Can redo?
    bool canRedo() const;

    /// All moves up to the current cursor.
    const std::vector<Coord> &moves() const { return moves_; }

    /// Current move index (0-based). -1 if no moves.
    int currentIndex() const { return cursor_ - 1; }

    /// Total moves recorded (including redo-able moves).
    int totalMoves() const { return static_cast<int>(moves_.size()); }

    /// Number of moves up to cursor.
    int moveCount() const { return cursor_; }

private:
    std::vector<Coord> moves_;
    int                cursor_ = 0;
};
