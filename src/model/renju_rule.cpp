#include "renju_rule.h"

namespace {

constexpr int kDirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

/// Stone lookup that treats every coordinate in `virtualBlack` as Black, on
/// top of whatever `board` actually holds. Lets the checks below simulate a
/// hypothetical move (or two) without copying/mutating the real board.
Stone stoneAtVirtual(const BoardState &board, Coord c, const std::vector<Coord> &virtualBlack)
{
    for (const auto &v : virtualBlack) {
        if (v == c) return Stone::Black;
    }
    return board.stoneAt(c);
}

/// The contiguous run of Black stones through `pos` along direction (dx,dy),
/// as seen through `virtualBlack`, plus whether each end of the run is
/// immediately followed by an in-bounds empty cell ("open").
struct RunInfo {
    int  length    = 1;
    bool frontOpen = false;
    bool backOpen  = false;
};

RunInfo runThrough(const BoardState &board, Coord pos, int dx, int dy,
                    const std::vector<Coord> &virtualBlack)
{
    int size = board.size();
    RunInfo info;

    int fx = pos.x, fy = pos.y;
    for (;;) {
        Coord n{fx + dx, fy + dy};
        if (!n.isValid(size) || stoneAtVirtual(board, n, virtualBlack) != Stone::Black) break;
        info.length++;
        fx = n.x;
        fy = n.y;
    }
    Coord front{fx + dx, fy + dy};
    info.frontOpen = front.isValid(size) && stoneAtVirtual(board, front, virtualBlack) == Stone::Empty;

    int bx = pos.x, by = pos.y;
    for (;;) {
        Coord n{bx - dx, by - dy};
        if (!n.isValid(size) || stoneAtVirtual(board, n, virtualBlack) != Stone::Black) break;
        info.length++;
        bx = n.x;
        by = n.y;
    }
    Coord back{bx - dx, by - dy};
    info.backOpen = back.isValid(size) && stoneAtVirtual(board, back, virtualBlack) == Stone::Empty;

    return info;
}

/// True if there exists a single empty cell g (within 4 steps of pos along
/// this direction) such that placing Black at both pos and g completes an
/// exact five-in-a-row through pos -- i.e. this direction gives the mover a
/// "four": an immediate five-completing threat at g.
bool directionHasFour(const BoardState &board, Coord pos, int dx, int dy)
{
    int size = board.size();
    for (int g = -4; g <= 4; ++g) {
        if (g == 0) continue;
        Coord gc{pos.x + dx * g, pos.y + dy * g};
        if (!gc.isValid(size) || board.stoneAt(gc) != Stone::Empty) continue;

        std::vector<Coord> vb{pos, gc};
        RunInfo r = runThrough(board, pos, dx, dy, vb);
        if (r.length == 5) return true;
    }
    return false;
}

/// True if there exists a single empty cell g such that placing Black at both
/// pos and g creates an OPEN four (exactly four in a row with both ends
/// still open) -- i.e. this direction gives the mover an "open three": one
/// more move away from an unstoppable double-ended four.
bool directionHasOpenThree(const BoardState &board, Coord pos, int dx, int dy)
{
    int size = board.size();
    for (int g = -4; g <= 4; ++g) {
        if (g == 0) continue;
        Coord gc{pos.x + dx * g, pos.y + dy * g};
        if (!gc.isValid(size) || board.stoneAt(gc) != Stone::Empty) continue;

        std::vector<Coord> vb{pos, gc};
        RunInfo r = runThrough(board, pos, dx, dy, vb);
        if (r.length == 4 && r.frontOpen && r.backOpen) return true;
    }
    return false;
}

} // namespace

namespace RenjuRule {

bool isForbidden(const BoardState &board, Coord pos)
{
    if (!pos.isValid(board.size()) || board.stoneAt(pos) != Stone::Empty) return false;

    std::vector<Coord> justPos{pos};

    // A move that completes an exact five always wins outright -- never
    // forbidden, regardless of any double-three/four shape it also matches.
    for (const auto &d : kDirs) {
        if (runThrough(board, pos, d[0], d[1], justPos).length == 5) return false;
    }

    // Overline (6+) in any direction is forbidden outright.
    for (const auto &d : kDirs) {
        if (runThrough(board, pos, d[0], d[1], justPos).length >= 6) return true;
    }

    // Double-four / double-three: count how many of the 4 lines through pos
    // each contribute a four or an open three (mutually exclusive per line --
    // a four-classified line is not also counted as a three).
    int fourDirs = 0, threeDirs = 0;
    for (const auto &d : kDirs) {
        if (directionHasFour(board, pos, d[0], d[1])) {
            fourDirs++;
        } else if (directionHasOpenThree(board, pos, d[0], d[1])) {
            threeDirs++;
        }
    }
    return fourDirs >= 2 || threeDirs >= 2;
}

std::vector<Coord> forbiddenPoints(const BoardState &board)
{
    std::vector<Coord> result;
    int size = board.size();
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            Coord c{x, y};
            if (board.stoneAt(c) != Stone::Empty) continue;
            if (isForbidden(board, c)) result.push_back(c);
        }
    }
    return result;
}

} // namespace RenjuRule
