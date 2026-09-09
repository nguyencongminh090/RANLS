// PROTO-07 — the `!yxAnalz` console command (CommandDispatcher layer).
//
// Lives in the UI test target only because CommandDispatcher does (it is
// compiled there); none of these cases need a display server — they drive the
// dispatcher directly, like the dispatcher half of
// test_cons02_autocorrect_dispatch.cpp.
//
// What is pinned here: the three refusals that must send NOTHING (Analyze
// Mode ON, empty arg list, every listed point off-board/occupied), and the
// happy path's alphabetic-move-text → engine-orientation `y,x` conversion,
// observed on the real wire via EngineProcess::signal_line_sent.
//
// See docs/todo/PROTO-07-yxanalz-root-move-allowlist.md.

#include "vendor/doctest.h"

#include "command/command_dispatcher.h"
#include "engine/engine_controller.h"
#include "engine/engine_process.h"
#include "model/game_state.h"

#include <glibmm.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace {

constexpr const char *kProto07ConsoleEngine = MOCK_ENGINE_PATH;  // PORT-03 stand-in

bool proto07ConsolePump(const std::function<bool()> &done, int timeoutMs = 3000)
{
    auto *ctx = g_main_context_default();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!done()) {
        g_main_context_iteration(ctx, FALSE);
        if (std::chrono::steady_clock::now() >= deadline) return false;
        g_usleep(1000);
    }
    return true;
}

/// Dispatcher + a *running* stand-in engine, so the handler gets past its
/// "Engine not running" guard and every outbound line is observable.
struct ConsoleFixture {
    GameState gs;
    EngineProcess eng;
    EngineController ctrl{gs, eng};
    std::vector<std::string> out;
    std::vector<std::string> sent;

    CommandContext ctx()
    {
        return CommandContext{
            .gameState = gs,
            .engine = eng,
            .controller = ctrl,
            .print = [this](const std::string &l) { out.push_back(l); },
            .clearConsole = [] {},
        };
    }

    ConsoleFixture()
    {
        EngineConfig cfg = gs.engineConfig();
        cfg.enginePath = kProto07ConsoleEngine;
        gs.setEngineConfig(cfg);
        ctrl.startEngine();
        REQUIRE(eng.isRunning());
        eng.signal_line_sent.connect([this](const std::string &l) { sent.push_back(l); });
    }

    ~ConsoleFixture()
    {
        ctrl.stopEngine([] {});
        proto07ConsolePump([&] { return !eng.isRunning(); });
    }

    void setAnalyzeMode(bool on)
    {
        ViewConfig vc = gs.viewConfig();
        vc.analyzeMode = on;
        gs.setViewConfig(vc);
    }

    bool sentAnything() const
    {
        for (const auto &l : sent)
            if (l == "YXANALZ" || l == "YXBOARD") return true;
        return false;
    }
};

}  // namespace

TEST_CASE("PROTO-07: !yxAnalz is registered in the analysis group and shows in !help")
{
    ConsoleFixture f;
    CommandDispatcher disp(f.ctx());

    const auto names = disp.registeredNames();
    CHECK(std::find(names.begin(), names.end(), "yxanalz") != names.end());
    CHECK(disp.commandUsage("yxanalz") == "!yxAnalz <moveText...>");

    disp.printHelp();
    bool listed = false;
    for (const auto &l : f.out)
        if (l.find("!yxAnalz") != std::string::npos) listed = true;
    CHECK(listed);
}

TEST_CASE("PROTO-07: !yxAnalz sends YXBOARD/YXANALZ with alphabetic move text converted to y,x")
{
    ConsoleFixture f;
    CommandDispatcher disp(f.ctx());
    f.sent.clear();

    // 15x15 board: h3 -> Coord{7,12}, h2 -> Coord{7,13}, h9 -> Coord{7,6};
    // coordToEngine() writes "y,x".
    CHECK(disp.executeLine("!yxAnalz h3 h2 h9"));

    REQUIRE(f.sent.size() == 7);
    CHECK(f.sent[0] == "YXBOARD");
    CHECK(f.sent[1] == "DONE");
    CHECK(f.sent[2] == "YXANALZ");
    CHECK(f.sent[3] == "12,7");
    CHECK(f.sent[4] == "13,7");
    CHECK(f.sent[5] == "6,7");
    CHECK(f.sent[6] == "DONE");
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Analyzing);
}

TEST_CASE("PROTO-07: !yxAnalz is refused while Analyze Mode is ON, and sends nothing")
{
    ConsoleFixture f;
    CommandDispatcher disp(f.ctx());
    f.setAnalyzeMode(true);
    f.sent.clear();

    disp.executeLine("!yxAnalz h3 h2 h9");

    REQUIRE_FALSE(f.out.empty());
    CHECK(f.out.back().rfind("ERR: Turn off Analyze Mode first", 0) == 0);
    CHECK_FALSE(f.sentAnything());
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);
    CHECK_FALSE(f.gs.isAnalyzing());
}

TEST_CASE("PROTO-07: !yxAnalz with no arguments is refused, and sends nothing")
{
    ConsoleFixture f;
    CommandDispatcher disp(f.ctx());
    f.sent.clear();

    disp.executeLine("!yxAnalz");

    REQUIRE_FALSE(f.out.empty());
    CHECK(f.out.back().rfind("ERR: Usage: !yxAnalz", 0) == 0);
    CHECK_FALSE(f.sentAnything());
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);
}

TEST_CASE("PROTO-07: !yxAnalz with only off-board coordinates is refused, and sends nothing")
{
    ConsoleFixture f;
    CommandDispatcher disp(f.ctx());
    f.sent.clear();

    // Row 99 and file 'z' are both outside a 15x15 board — parseMovesText
    // drops them, leaving an empty list.
    disp.executeLine("!yxAnalz h99 z3");

    REQUIRE_FALSE(f.out.empty());
    CHECK(f.out.back().rfind("ERR: No valid analyze move", 0) == 0);
    CHECK_FALSE(f.sentAnything());
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);
}

TEST_CASE("PROTO-07: !yxAnalz with only occupied points is refused, and sends nothing")
{
    ConsoleFixture f;
    CommandDispatcher disp(f.ctx());

    REQUIRE(f.gs.makeMove(Coord{7, 12}));  // h3
    REQUIRE(f.gs.makeMove(Coord{7, 13}));  // h2
    f.sent.clear();

    disp.executeLine("!yxAnalz h3 h2");

    REQUIRE_FALSE(f.out.empty());
    CHECK(f.out.back().rfind("ERR: No valid analyze move", 0) == 0);
    CHECK_FALSE(f.sentAnything());
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);
}

TEST_CASE("PROTO-07: occupied points are skipped, the remaining list is still analyzed")
{
    ConsoleFixture f;
    CommandDispatcher disp(f.ctx());

    REQUIRE(f.gs.makeMove(Coord{7, 12}));  // h3 is now occupied
    f.sent.clear();

    disp.executeLine("!yxAnalz h3 h9");

    // The position block carries the played stone; only h9 is a root move.
    REQUIRE(f.sent.size() == 6);
    CHECK(f.sent[0] == "YXBOARD");
    CHECK(f.sent[1] == "12,7,1");
    CHECK(f.sent[2] == "DONE");
    CHECK(f.sent[3] == "YXANALZ");
    CHECK(f.sent[4] == "6,7");
    CHECK(f.sent[5] == "DONE");
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Analyzing);
}
