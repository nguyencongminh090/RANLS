// Regression test for IO-01: GameIO::saveGame / GameIO::loadGame must
// round-trip the current game line (board size, rule, move sequence) and must
// reject corrupt / truncated / garbage input with a failure result rather than
// crashing or silently succeeding.
//
// GameIO is deliberately gtkmm-free (see src/model/game_io.h) so it is exercised
// directly here; the MainWindow wiring is UI code out of this binary's reach.

#include "vendor/doctest.h"

#include "model/game_io.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path tmpFile(const std::string &name)
{
    return std::filesystem::temp_directory_path() / ("yxb-io01-" + name);
}

void writeText(const std::filesystem::path &p, const std::string &contents)
{
    std::ofstream out(p, std::ios::trunc);
    out << contents;
}

} // namespace

TEST_CASE("GameIO: save then load round-trips board size, rule and moves") {
    auto path = tmpFile("roundtrip.yxg");
    std::vector<Coord> moves = {{7, 7}, {8, 8}, {7, 8}, {6, 9}};

    REQUIRE(GameIO::saveGame(path, 19, GameRule::Renju, moves));

    std::string err;
    auto loaded = GameIO::loadGame(path, &err);
    REQUIRE_MESSAGE(loaded.has_value(), err);
    CHECK(loaded->boardSize == 19);
    CHECK(loaded->rule == GameRule::Renju);
    REQUIRE(loaded->moves.size() == moves.size());
    for (size_t i = 0; i < moves.size(); ++i) {
        CHECK(loaded->moves[i].x == moves[i].x);
        CHECK(loaded->moves[i].y == moves[i].y);
    }

    std::remove(path.string().c_str());
}

TEST_CASE("GameIO: empty game (no moves) round-trips") {
    auto path = tmpFile("empty.yxg");
    REQUIRE(GameIO::saveGame(path, 15, GameRule::Freestyle, {}));

    auto loaded = GameIO::loadGame(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->boardSize == 15);
    CHECK(loaded->rule == GameRule::Freestyle);
    CHECK(loaded->moves.empty());

    std::remove(path.string().c_str());
}

TEST_CASE("GameIO: loadGame fails cleanly on a missing file") {
    std::string err;
    auto loaded = GameIO::loadGame(tmpFile("does-not-exist.yxg"), &err);
    CHECK_FALSE(loaded.has_value());
    CHECK_FALSE(err.empty());
}

TEST_CASE("GameIO: loadGame rejects garbage / corrupt / truncated input") {
    struct Case { const char *name; std::string body; };
    const Case cases[] = {
        {"garbage",          "the quick brown fox\n%%%not a config%%%\n"},
        {"no-version",       "board_size=15\nrule=0\nmove=7,7\n"},
        {"wrong-version",    "yxgame_version=99\nboard_size=15\nrule=0\n"},
        {"missing-boardsize","yxgame_version=1\nrule=0\nmove=7,7\n"},
        {"missing-rule",     "yxgame_version=1\nboard_size=15\n"},
        {"boardsize-oob",    "yxgame_version=1\nboard_size=99\nrule=0\n"},
        {"rule-oob",         "yxgame_version=1\nboard_size=15\nrule=7\n"},
        {"move-oob",         "yxgame_version=1\nboard_size=15\nrule=0\nmove=40,40\n"},
        {"move-malformed",   "yxgame_version=1\nboard_size=15\nrule=0\nmove=7\n"},
        {"move-nonnumeric",  "yxgame_version=1\nboard_size=15\nrule=0\nmove=a,b\n"},
        {"move-before-size", "yxgame_version=1\nmove=7,7\nboard_size=15\nrule=0\n"},
        {"duplicate-move",   "yxgame_version=1\nboard_size=15\nrule=0\nmove=7,7\nmove=7,7\n"},
        {"truncated",        "yxgame_version=1\nboard_siz"},
        {"line-no-equals",   "yxgame_version=1\nboard_size=15\nrule=0\nthis line has no equals\n"},
    };

    for (const auto &c : cases) {
        CAPTURE(c.name);
        auto path = tmpFile(std::string("bad-") + c.name);
        writeText(path, c.body);
        std::string err;
        auto loaded = GameIO::loadGame(path, &err);
        CHECK_FALSE(loaded.has_value());
        CHECK_FALSE(err.empty());
        std::remove(path.string().c_str());
    }
}
