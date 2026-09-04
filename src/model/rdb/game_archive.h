#pragma once

// RDB-02: the archive layer that sits between MainWindow's Save/Open slots and
// the RDB-01 substrate (container + CBOR + convert).
//
//   IGameArchiveReader::load(path)  -> std::optional<GameGraph>
//   IGameArchiveWriter::save(path, GameGraph) -> bool
//
//   RdbArchive   — reader + writer for the binary `.rdb` format (RDB-01
//                  container, DEFLATE codec, CBOR payload).
//   YxgameReader — reader only. Wraps the unchanged legacy `GameIO::loadGame`
//                  and maps its flat move list into a linear-chain GameGraph
//                  (node i's parent = i-1, nodes[0] a sentinel, NO analysis on
//                  any node => every eval is NaN, exactly like a fresh game).
//
// The factory picks an implementation by lowercased file extension:
//   .rdb    -> RdbArchive          (read + write)
//   .yxgame -> YxgameReader        (read only; there is NO `.yxgame` writer —
//                                   archiveWriterFor() returns nullptr and the
//                                   caller surfaces that as an error)
//   other   -> RdbArchive for read (it fails cleanly on a bad magic)
//
// Every failure path returns nullopt/false + a non-empty `*error` and never
// throws. gtkmm/glibmm-free so it stays in the model-only test target.

#include "game_graph.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

class GameState;

namespace rdb {

class IGameArchiveReader {
public:
    virtual ~IGameArchiveReader() = default;

    /// Load and decode `path` into a GameGraph. Returns nullopt + sets `*error`
    /// (non-empty) on any problem; never throws.
    virtual std::optional<GameGraph> load(const std::filesystem::path &path,
                                          std::string                 *error) = 0;
};

class IGameArchiveWriter {
public:
    virtual ~IGameArchiveWriter() = default;

    /// Encode + write `graph` to `path`. Returns false + sets `*error` on any
    /// problem; never throws. Atomic where the underlying container is.
    virtual bool save(const std::filesystem::path &path, const GameGraph &graph,
                      std::string *error) = 0;
};

/// Binary `.rdb` — RDB-01 container + DEFLATE + CBOR. Reader and writer.
class RdbArchive final : public IGameArchiveReader, public IGameArchiveWriter {
public:
    std::optional<GameGraph> load(const std::filesystem::path &path,
                                  std::string                 *error) override;
    bool                     save(const std::filesystem::path &path,
                                  const GameGraph &graph, std::string *error) override;
};

/// Legacy `.yxgame` — import only, via the unchanged `GameIO::loadGame`.
class YxgameReader final : public IGameArchiveReader {
public:
    std::optional<GameGraph> load(const std::filesystem::path &path,
                                  std::string                 *error) override;
};

/// Reader for `path`, chosen by lowercased extension. Never null (`.yxgame` =>
/// YxgameReader, everything else => RdbArchive).
std::unique_ptr<IGameArchiveReader>
archiveReaderFor(const std::filesystem::path &path);

/// Writer for `path`. Returns nullptr for any extension other than `.rdb` —
/// the caller must treat that as "this format cannot be saved".
std::unique_ptr<IGameArchiveWriter>
archiveWriterFor(const std::filesystem::path &path);

/// Apply a loaded GameGraph onto `gs` (the RDB-02 "applying a GameGraph on
/// load" helper — kept out of MainWindow so it is unit-testable).
///
/// Validates board size, rule, every parent back-reference AND every move coord
/// against `g.board` before mutating anything. On any problem returns false,
/// sets `*error`, and leaves `gs` untouched. On success:
///   1. gs.newGame(g.board)   2. gs.setRule(g.rule)
///   3. rebuild the whole variation tree from g.nodes (DFS pre-order + parent
///      indices) directly on gs.tree(); then replay the mainline first-child
///      chain through gs.makeMove() so history/board advance to the tip
///   4. emit signal_tree_updated + signal_board_changed
/// sendConfig() (engine resync) stays with the caller.
bool applyGameGraphToState(GameState &gs, const GameGraph &g,
                           std::string *error = nullptr);

} // namespace rdb
