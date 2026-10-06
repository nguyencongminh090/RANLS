// ARCH-05: GameFileService -- the display-free save/load logic extracted from
// MainWindow::onSaveGame / onLoadGame. gtkmm-free; lives in ranls-gui-tests.

#include "vendor/doctest.h"

#include "model/game_file_service.h"
#include "model/game_state.h"
#include "model/rdb/game_archive.h"
#include "model/variation_tree.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

// Unique temp dir, removed on scope exit.
struct TempDir {
    fs::path path;
    TempDir()
    {
        static std::atomic<int> counter{0};
        path = fs::temp_directory_path()
               / ("ranls-arch05-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-"
                  + std::to_string(counter++));
        fs::create_directories(path);
    }
    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

int countNodes(const TreeNode *n)
{
    int c = 0;
    for (const auto &ch : n->children)
        c += 1 + countNodes(ch.get());
    return c;
}

GameState makeGame()
{
    GameState gs(15);
    gs.newGame(15);
    gs.setRule(GameRule::Renju);
    REQUIRE(gs.makeMove({7, 7}));
    REQUIRE(gs.makeMove({8, 8}));
    REQUIRE(gs.makeMove({9, 9}));
    TreeNode *m1     = gs.tree().root()->children.front().get();
    TreeNode *branch = gs.tree().addMove(m1, Coord{5, 11});
    branch->comment  = "branch";
    return gs;
}

constexpr const char *kNonRdbError = "RANLS only saves games in the .rdb format.";

} // namespace

TEST_CASE("ARCH-05 save: engine entry written from EngineConfig")
{
    TempDir     dir;
    GameState   gs = makeGame();
    EngineConfig ec;
    ec.enginePath = "/some/dir/pbrain-rapfi";
    ec.threads    = 4;
    ec.hashSizeMB = 512;
    gs.setEngineConfig(ec);

    const auto path = dir.path / "g.rdb";
    auto       r    = GameFileService::save(gs, path, "TestApp");
    REQUIRE(r.ok);
    CHECK(r.error.empty());

    std::string err;
    auto graph = rdb::archiveReaderFor(path)->load(path, &err);
    REQUIRE(graph);
    CHECK(graph->generator == "TestApp");
    REQUIRE(graph->engines.size() == 1);
    CHECK(graph->engines[0].id == 0);
    CHECK(graph->engines[0].name == "pbrain-rapfi");
    CHECK(graph->engines[0].params == "threads=4 hash=512MB");
}

TEST_CASE("ARCH-05 save: no engine entry when enginePath is empty")
{
    TempDir      dir;
    GameState    gs = makeGame();
    EngineConfig ec;
    ec.enginePath.clear();
    gs.setEngineConfig(ec);

    const auto path = dir.path / "g.rdb";
    REQUIRE(GameFileService::save(gs, path, "TestApp").ok);

    std::string err;
    auto graph = rdb::archiveReaderFor(path)->load(path, &err);
    REQUIRE(graph);
    CHECK(graph->engines.empty());
}

TEST_CASE("ARCH-05 save: path without extension gets .rdb appended")
{
    TempDir   dir;
    GameState gs = makeGame();

    const auto bare = dir.path / "mygame";
    auto       r    = GameFileService::save(gs, bare, "TestApp");
    REQUIRE(r.ok);
    CHECK(fs::exists(dir.path / "mygame.rdb"));
    CHECK_FALSE(fs::exists(bare));
}

TEST_CASE("ARCH-05 save: non-.rdb target is an error and writes nothing")
{
    TempDir   dir;
    GameState gs = makeGame();

    for (const char *name : {"g.yxgame", "g.txt"}) {
        const auto path = dir.path / name;
        auto       r    = GameFileService::save(gs, path, "TestApp");
        CHECK_FALSE(r.ok);
        CHECK(r.error == kNonRdbError);
        CHECK_FALSE(fs::exists(path));
    }
    CHECK(fs::is_empty(dir.path));
}

TEST_CASE("ARCH-05 save: write failure surfaces the writer's error text")
{
    TempDir   dir;
    GameState gs = makeGame();

    // Parent directory does not exist.
    auto r = GameFileService::save(gs, dir.path / "nope" / "g.rdb", "TestApp");
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
}

TEST_CASE("ARCH-05 load: a saved game round-trips into a fresh GameState")
{
    TempDir   dir;
    GameState src = makeGame();
    const int total = countNodes(src.tree().root()); // 4
    const auto path = dir.path / "g.rdb";
    REQUIRE(GameFileService::save(src, path, "TestApp").ok);

    GameState dst(19);
    dst.newGame(19);
    auto r = GameFileService::load(dst, path);
    REQUIRE(r.ok);
    CHECK(r.error.empty());

    CHECK(dst.boardSize() == 15);
    CHECK(dst.rule() == GameRule::Renju);
    CHECK(countNodes(dst.tree().root()) == total);
    CHECK(dst.history().totalMoves() == src.history().totalMoves());
    bool foundBranch = false;
    for (const auto &ch : dst.tree().root()->children)
        for (const auto &g : ch->children)
            if (g->comment == "branch") foundBranch = true;
    CHECK(foundBranch);
}

TEST_CASE("ARCH-05 load: garbage and missing files error and leave state unchanged")
{
    TempDir dir;

    GameState gs(15);
    gs.newGame(15);
    REQUIRE(gs.makeMove({7, 7}));
    const int nodesBefore = countNodes(gs.tree().root());

    const auto garbage = dir.path / "bad.rdb";
    {
        std::ofstream f(garbage, std::ios::binary);
        f << "this is not an rdb archive";
    }
    auto r1 = GameFileService::load(gs, garbage);
    CHECK_FALSE(r1.ok);
    CHECK_FALSE(r1.error.empty());

    auto r2 = GameFileService::load(gs, dir.path / "missing.rdb");
    CHECK_FALSE(r2.ok);
    CHECK_FALSE(r2.error.empty());

    CHECK(gs.boardSize() == 15);
    CHECK(countNodes(gs.tree().root()) == nodesBefore);
    CHECK(gs.history().totalMoves() == 1);
}
