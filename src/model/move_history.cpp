#include "move_history.h"

void MoveHistory::clear()
{
    moves_.clear();
    cursor_ = 0;
}

void MoveHistory::addMove(Coord pos)
{
    // Truncate any redo history beyond cursor.
    if (cursor_ < static_cast<int>(moves_.size())) {
        moves_.resize(cursor_);
    }
    moves_.push_back(pos);
    cursor_++;
}

Coord MoveHistory::undoMove()
{
    if (!canUndo())
        return Coord{};

    cursor_--;
    return moves_[cursor_];
}

Coord MoveHistory::redoMove()
{
    if (!canRedo())
        return Coord{};

    Coord pos = moves_[cursor_];
    cursor_++;
    return pos;
}

bool MoveHistory::canUndo() const
{
    return cursor_ > 0;
}

bool MoveHistory::canRedo() const
{
    return cursor_ < static_cast<int>(moves_.size());
}
