#pragma once

#include "board_state.h"

#include <vector>

/// UI-03: Renju forbidden-move detection (domain logic).
///
/// Under Free Renju, Black (and only Black — White is unrestricted) may not
/// play a move that produces:
///   - an overline (6 or more stones in a row),
///   - a double-four (the move creates two or more distinct "four" threats
///     that are not already a winning five), or
///   - a double-three (the move creates two or more distinct "open three"
///     threats — a three that a single follow-up move turns into an open,
///     unstoppable four).
///
/// A move that completes an exact five-in-a-row is always a legal win and is
/// never forbidden, even if it would otherwise also match one of the shapes
/// above (five overrides forbidden — see docs/todo/UI-03-rule-not-visible-on-board.md
/// and the Rapfi engine's YXSHOWFORBID semantics, which this mirrors: "every
/// empty cell for which checkForbiddenPoint() holds (Renju-only concept —
/// 3-3, 4-4, overline)").
///
/// This module is display/indication support only (UI-03): it computes which
/// points WOULD be forbidden so the GUI can mark them, it does not reject or
/// block moves. GameState::makeMove() still allows placing on a forbidden
/// point (useful for analysis / deliberately setting up illegal positions).
///
/// Intentionally lives in src/model/ (not src/ui/BoardRenderer) — Renju
/// forbidden-move detection is domain logic; BoardRenderer only ever receives
/// already-computed coordinates to draw (see the software-architecture
/// convention: model owns domain logic, UI only renders it).
///
/// Known simplification: a "four" threat requires the single completing
/// point to yield an EXACT five (not an overline) — a completion that would
/// only produce an overline is not counted as a four here. This matches the
/// common simplified Renju double-four/-three implementation used by most
/// Gomocup-protocol clients and is precise for the standard shapes (straight
/// XXXX, and single-gap XX.XX / X.XXX / XXX.X patterns); it does not attempt
/// to resolve every exotic multi-line edge case a full Renju judge would.
namespace RenjuRule {

/// True if Black playing at `pos` would be a forbidden move under Free Renju.
/// `pos` must be empty; returns false for an out-of-bounds or occupied cell,
/// or for any point that would complete an exact five-in-a-row (a five always
/// wins and is never forbidden, regardless of any double-three/four shape it
/// also happens to match).
bool isForbidden(const BoardState &board, Coord pos);

/// All of Black's currently-forbidden points on `board` (every empty cell for
/// which isForbidden() holds). O(boardSize^2) — cheap enough to recompute on
/// every board/rule change; not intended to be called per-frame in a hot draw
/// loop without caching upstream (BoardViewModel::update() caches the result
/// once per board-changed/rule-changed event).
std::vector<Coord> forbiddenPoints(const BoardState &board);

} // namespace RenjuRule
