#pragma once

#include <array>
#include <cstdint>

/// Maximum supported board size.
constexpr int MAX_BOARD_SIZE = 22;

/// Default Gomoku board size.
constexpr int DEFAULT_BOARD_SIZE = 15;

/// Stone color on the board.
enum class Stone : uint8_t {
    Empty = 0,
    Black = 1,
    White = 2,
};

/// Gomoku game rules. Defined here (not game_state.h) so BoardState::checkWin()
/// can be rule-aware without a circular include; game_state.h includes this
/// header and re-exposes GameRule transitively, so existing `#include
/// "model/game_state.h"` call sites are unaffected.
enum class GameRule {
    Freestyle = 0,   ///< Freestyle Gomoku (rule 0)
    Standard  = 1,   ///< Standard Gomoku / exact-5 (rule 1)
    Renju     = 2    ///< Free Renju with forbidden moves (rule 2)
};

/// A single point on the board.
struct Coord {
    int x = -1; ///< Column (0-indexed, left to right)
    int y = -1; ///< Row    (0-indexed, top to bottom)

    bool isValid(int boardSize) const
    {
        return x >= 0 && y >= 0 && x < boardSize && y < boardSize;
    }

    bool operator==(const Coord &other) const = default;
    bool operator!=(const Coord &other) const = default;
    bool operator<(const Coord &other) const
    {
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
};

/// Static board grid.
/// Stores stone placement for a given board size.
class BoardState {
public:
    explicit BoardState(int size = DEFAULT_BOARD_SIZE);

    /// Reset the board to empty.
    void clear();

    /// Place a stone at (x, y). Returns false if the cell is occupied.
    bool placeStone(Coord pos, Stone stone);

    /// Remove the stone at (x, y). Returns the stone that was there.
    Stone removeStone(Coord pos);

    /// Get the stone at (x, y).
    Stone stoneAt(Coord pos) const;

    /// Current board size.
    int size() const { return size_; }

    /// Get the side to move (based on ply count).
    Stone sideToMove() const;

    /// Number of stones on the board.
    int plyCount() const { return plyCount_; }

    /// Check if placing a stone at pos would complete a win, given the active
    /// rule (the color checked is whatever stone is actually at `pos`).
    /// UI-03: rule-aware overline handling —
    ///   - Freestyle: any run of 5 or more in a row wins.
    ///   - Standard: only an EXACT run of 5 wins, for either color; a run of
    ///     6+ ("overline") does NOT win — the game continues. Previously this
    ///     function ignored `rule` entirely and treated every overline as a
    ///     win, which diverged from the engine's own `RULE 1` (Standard) win
    ///     condition (see game/wincheck.h / game/pattern.cpp in the Rapfi
    ///     engine: `CheckOverline = R == STANDARD || (R == RENJU && Black)`).
    ///   - Renju: White follows the Freestyle any-5-or-more rule; Black
    ///     follows the Standard exact-5 rule (an overline is not a win for
    ///     Black — under Renju, Black overline is actually a forbidden move
    ///     in the first place; see RenjuRule::isForbidden in renju_rule.h,
    ///     which decides legality, while checkWin() only decides win/no-win
    ///     for a move that was already allowed to be played).
    bool checkWin(Coord pos, GameRule rule) const;

private:
    int                                                  size_;
    int                                                  plyCount_ = 0;
    std::array<std::array<Stone, MAX_BOARD_SIZE>, MAX_BOARD_SIZE> grid_;
};
