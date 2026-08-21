#include "board_state.h"

BoardState::BoardState(int size)
    : size_(size)
{
    clear();
}

void BoardState::clear()
{
    plyCount_ = 0;
    for (auto &row : grid_)
        row.fill(Stone::Empty);
}

bool BoardState::placeStone(Coord pos, Stone stone)
{
    if (!pos.isValid(size_))
        return false;
    if (grid_[pos.y][pos.x] != Stone::Empty)
        return false;

    grid_[pos.y][pos.x] = stone;
    plyCount_++;
    return true;
}

Stone BoardState::removeStone(Coord pos)
{
    if (!pos.isValid(size_))
        return Stone::Empty;

    Stone s = grid_[pos.y][pos.x];
    grid_[pos.y][pos.x] = Stone::Empty;
    if (s != Stone::Empty)
        plyCount_--;
    return s;
}

Stone BoardState::stoneAt(Coord pos) const
{
    if (!pos.isValid(size_))
        return Stone::Empty;
    return grid_[pos.y][pos.x];
}

Stone BoardState::sideToMove() const
{
    return (plyCount_ % 2 == 0) ? Stone::Black : Stone::White;
}

bool BoardState::checkWin(Coord pos, GameRule rule) const
{
    Stone s = stoneAt(pos);
    if (s == Stone::Empty)
        return false;

    // UI-03: overline (a run of 6+) counts as a win only under Freestyle, or
    // under Renju for White specifically -- Standard (either color) and
    // Renju-Black require an EXACT run of 5. See board_state.h's checkWin()
    // doc comment for the rationale/engine cross-reference.
    bool overlineWins = (rule == GameRule::Freestyle)
                      || (rule == GameRule::Renju && s == Stone::White);

    // 4 directions: horizontal, vertical, diagonal-down, diagonal-up.
    static constexpr int dx[] = {1, 0, 1, 1};
    static constexpr int dy[] = {0, 1, 1, -1};

    for (int d = 0; d < 4; ++d) {
        int count = 1;
        // Positive direction.
        for (int step = 1; step < 5; ++step) {
            Coord c{pos.x + dx[d] * step, pos.y + dy[d] * step};
            if (!c.isValid(size_) || stoneAt(c) != s)
                break;
            count++;
        }
        // Negative direction.
        for (int step = 1; step < 5; ++step) {
            Coord c{pos.x - dx[d] * step, pos.y - dy[d] * step};
            if (!c.isValid(size_) || stoneAt(c) != s)
                break;
            count++;
        }
        if (count == 5)
            return true;
        if (count > 5 && overlineWins)
            return true;
    }
    return false;
}
