#pragma once

// ARCH-05: the display-free half of Save Game / Load Game, extracted from
// MainWindow's dialog callbacks so it is unit-testable without a window.
//
// Facade over the RDB archive layer: save() defaults the `.rdb` extension,
// picks a writer, builds the GraphMeta (incl. the RDB-03 display-only engine
// entry from EngineConfig) and writes the whole variation tree; load() picks a
// reader, decodes the GameGraph and applies it to a GameState. File choosers,
// error dialogs, discard confirmation and the engine resync stay with the
// caller. Depends only on model/ and model/rdb/.

#include <filesystem>
#include <string>

class GameState;

namespace GameFileService {

struct Result {
    bool        ok = false;
    std::string error; ///< User-facing text; empty when ok.
};

/// Save `gs`'s whole variation tree. A path with no extension gets `.rdb`
/// appended. Any other non-`.rdb` target fails with
/// "RANLS only saves games in the .rdb format." and writes nothing.
/// `generator` is stored in the file header (the app display name).
Result save(const GameState &gs, const std::filesystem::path &path,
            const std::string &generator);

/// Load `path` (reader chosen by extension) and apply it to `gs`. On any
/// failure `error` is set and `gs` is left untouched. The caller must
/// resync the engine (sendConfig) after a successful load.
Result load(GameState &gs, const std::filesystem::path &path);

} // namespace GameFileService
