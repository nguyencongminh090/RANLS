#include "game_file_service.h"

#include "game_state.h"
#include "i18n/i18n.h"
#include "rdb/game_archive.h"
#include "rdb/game_graph_convert.h"

#include <utility>

namespace GameFileService {

Result save(const GameState &gs, const std::filesystem::path &path,
            const std::string &generator)
{
    // A path with no extension defaults to `.rdb`.
    std::filesystem::path fsPath(path);
    if (fsPath.extension().empty())
        fsPath.replace_extension(".rdb");

    auto writer = rdb::archiveWriterFor(fsPath);
    if (!writer)
        return {false, i18n::tr("RANLS only saves games in the .rdb format.")};

    rdb::GraphMeta meta;
    meta.generator = generator;
    // RDB-03: one display-only engine entry from the current EngineConfig.
    // Referenced by per-node analysis (engineRef); never affects load
    // behaviour, and a missing/empty list must not fail a load.
    {
        const auto &ec = gs.engineConfig();
        if (!ec.enginePath.empty()) {
            rdb::EngineInfo ei;
            ei.id     = 0;
            ei.name   = std::filesystem::path(ec.enginePath).filename().string();
            ei.params = "threads=" + std::to_string(ec.threads)
                        + " hash=" + std::to_string(ec.hashSizeMB) + "MB";
            meta.engines.push_back(std::move(ei));
        }
    }
    const auto graph = rdb::toGameGraph(gs.tree(), gs.boardSize(), gs.rule(), meta);

    std::string err;
    if (!writer->save(fsPath, graph, &err))
        return {false, err};
    return {true, {}};
}

Result load(GameState &gs, const std::filesystem::path &path)
{
    auto        reader = rdb::archiveReaderFor(path);
    std::string err;
    auto        graph = reader->load(path, &err);
    if (!graph)
        return {false, err};
    if (!rdb::applyGameGraphToState(gs, *graph, &err))
        return {false, err};
    return {true, {}};
}

} // namespace GameFileService
