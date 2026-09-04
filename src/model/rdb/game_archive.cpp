#include "game_archive.h"

#include "game_graph_cbor.h"
#include "game_graph_convert.h"
#include "rdb_container.h"

#include "../game_io.h"
#include "../game_state.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace rdb {

namespace {

std::string lowerExt(const std::filesystem::path &path)
{
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

constexpr uint8_t kDeflateCodec = 2; // rdb::DeflateCodec::id()

} // namespace

// ── RdbArchive ─────────────────────────────────────────────────────────────

std::optional<GameGraph> RdbArchive::load(const std::filesystem::path &path,
                                          std::string                 *error)
{
    auto payload = readContainer(path, error);
    if (!payload)
        return std::nullopt;

    auto graph = decodeCbor(*payload);
    if (!graph) {
        if (error)
            *error = "Not a valid .rdb game: the payload could not be decoded ("
                     + path.string() + ")";
        return std::nullopt;
    }
    return graph;
}

bool RdbArchive::save(const std::filesystem::path &path, const GameGraph &graph,
                      std::string *error)
{
    const std::string payload = encodeCbor(graph);
    return writeContainer(path, kDeflateCodec, payload, error);
}

// ── YxgameReader ───────────────────────────────────────────────────────────

std::optional<GameGraph> YxgameReader::load(const std::filesystem::path &path,
                                            std::string                 *error)
{
    std::string               ioErr;
    auto loaded = GameIO::loadGame(path, &ioErr);
    if (!loaded) {
        if (error)
            *error = ioErr;
        return std::nullopt;
    }

    GameGraph g;
    g.schema    = kSchemaVersion;
    g.board     = static_cast<uint8_t>(loaded->boardSize);
    g.rule      = static_cast<uint8_t>(loaded->rule);
    g.generator = "yxgame-import";

    GraphNode root; // nodes[0] — sentinel: no parent, no move, no analysis
    root.hasParent = false;
    g.nodes.push_back(std::move(root));

    for (const Coord &mv : loaded->moves) {
        GraphNode n;
        n.parent    = static_cast<uint32_t>(g.nodes.size() - 1); // linear chain
        n.hasParent = true;
        n.move      = Move{mv.x, mv.y};
        // Deliberately no analysis => every eval is NaN, exactly like a fresh
        // game. RDB-02 does not synthesise evals for legacy imports.
        g.nodes.push_back(std::move(n));
    }
    return g;
}

// ── Factory ────────────────────────────────────────────────────────────────

std::unique_ptr<IGameArchiveReader>
archiveReaderFor(const std::filesystem::path &path)
{
    if (lowerExt(path) == ".yxgame")
        return std::make_unique<YxgameReader>();
    // .rdb, no extension, or anything unknown: try the binary reader — it
    // fails cleanly on a bad magic. Never guess `.yxgame`.
    return std::make_unique<RdbArchive>();
}

std::unique_ptr<IGameArchiveWriter>
archiveWriterFor(const std::filesystem::path &path)
{
    if (lowerExt(path) == ".rdb")
        return std::make_unique<RdbArchive>();
    return nullptr; // saving any other format is a caller-visible error
}

// ── applyGameGraphToState ──────────────────────────────────────────────────

bool applyGameGraphToState(GameState &gs, const GameGraph &g, std::string *error)
{
    auto fail = [&](std::string msg) {
        if (error)
            *error = std::move(msg);
        return false;
    };

    if (g.schema > kSchemaVersion)
        return fail("This .rdb was written by a newer version of RANLS (schema "
                    + std::to_string(g.schema) + " > "
                    + std::to_string(kSchemaVersion) + ").");
    if (g.board < 5 || g.board > MAX_BOARD_SIZE)
        return fail("Board size " + std::to_string(g.board) + " is out of range.");
    if (g.rule > 2)
        return fail("Rule " + std::to_string(g.rule) + " is out of range.");
    if (g.nodes.empty() || g.nodes[0].hasParent || g.nodes[0].move.has_value())
        return fail("The game file is missing its root node.");

    // Validate EVERY back-reference and EVERY move coord before touching `gs`.
    // (RDB-01's applyGameGraph does not range-check coords — a hand-edited or
    // corrupt .rdb could carry an off-board move; reject it cleanly here.)
    for (size_t i = 1; i < g.nodes.size(); ++i) {
        const GraphNode &n = g.nodes[i];
        if (!n.hasParent || n.parent >= i)
            return fail("Node " + std::to_string(i)
                        + " has an invalid parent reference.");
        if (!n.move.has_value())
            return fail("Node " + std::to_string(i) + " has no move.");
        const Coord c{n.move->x, n.move->y};
        if (!c.isValid(g.board))
            return fail("Node " + std::to_string(i) + " has an off-board move ("
                        + std::to_string(c.x) + "," + std::to_string(c.y) + ").");
    }

    // ── Mutate: from here everything succeeds. ──
    gs.newGame(g.board);
    gs.setRule(static_cast<GameRule>(g.rule));

    VariationTree             &tree = gs.tree();
    std::vector<TreeNode *>    tnodes(g.nodes.size(), nullptr);
    tnodes[0] = tree.root();

    for (size_t i = 1; i < g.nodes.size(); ++i) {
        const GraphNode &n      = g.nodes[i];
        TreeNode        *parent = tnodes[n.parent];
        TreeNode        *child  = tree.addMove(parent, Coord{n.move->x, n.move->y});

        child->comment = n.comment;
        // RDB-03: restore the full per-node analysis (winrate/depth/nodes +
        // evalText/pv/glyph/engineRef/analyzedUtc), validating winrate ∈ [0,1]
        // and every pv coord against the board — never aborting on a bad value.
        applyNodeAnalysis(n, g.board, *child);
        tnodes[i] = child;
    }

    // Replay the mainline (first-child chain from the root) through makeMove()
    // so history_/board_ advance to the mainline tip. addMove() returns the
    // already-present child, so this does not duplicate nodes. Branch nodes
    // were added to the tree above and are left as tree-only.
    for (const TreeNode *walk = tree.root(); !walk->children.empty();) {
        const TreeNode *next = walk->children.front().get();
        if (!gs.makeMove(next->move))
            break; // defensive: coords were validated, should not happen
        walk = next;
    }

    gs.signal_tree_updated.emit();
    gs.signal_board_changed.emit();
    return true;
}

} // namespace rdb
