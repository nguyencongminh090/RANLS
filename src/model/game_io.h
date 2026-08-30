#pragma once

// IO-01: plain-text serialization of the current game line (board size, rule,
// move sequence) to/from a user-chosen file. Deliberately free of gtkmm/glibmm
// so it stays unit-testable in tests/CMakeLists.txt (model/engine only — see
// that file's header comment). Mirrors the hand-rolled key=value style of
// src/model/settings_storage.cpp; no JSON dependency.
//
// Out of scope (per docs/todo/IO-01-load-save-game.md): recent-files list,
// auto-save, format migration. The version field below is checked-and-rejected
// only — there is no upgrade path.

#include "board_state.h" // Coord, GameRule

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace GameIO {

/// On-disk format version. A file whose yxgame_version differs is rejected
/// (no migration — see header note).
inline constexpr int kFormatVersion = 1;

/// Parsed result of loadGame(): the current game line only (no variation tree).
struct LoadedGame {
    int                boardSize = DEFAULT_BOARD_SIZE;
    GameRule           rule      = GameRule::Freestyle;
    std::vector<Coord> moves;
};

/// Serialize a game to `path`. `moves` must be in play order. Returns false on
/// any write failure (and sets `*error` if non-null). Does not validate the
/// moves against each other — the caller owns a consistent GameState.
bool saveGame(const std::filesystem::path &path,
              int                           boardSize,
              GameRule                      rule,
              const std::vector<Coord>     &moves,
              std::string                  *error = nullptr);

/// Parse a file written by saveGame(). Returns std::nullopt on any problem
/// (missing/unreadable file, wrong/absent version, missing or out-of-range
/// board size / rule, malformed or out-of-range / duplicated move) and sets
/// `*error` if non-null. Never throws, never partially succeeds.
std::optional<LoadedGame> loadGame(const std::filesystem::path &path,
                                   std::string                 *error = nullptr);

} // namespace GameIO
