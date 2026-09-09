// PROTO-07 — `YXANALZ` explicit root-move allow-list (protocol + controller).
//
// Two layers, no gtkmm (this file lives in the model/engine test target):
//
//   * GomocupProtocol::generateAnalyzeMovesRequest() — the wire block:
//     `YXBOARD` / `<path>,<color>` … / `DONE` / `YXANALZ` / `<move>` … /
//     `DONE`, every coordinate through the single coordToEngine() conversion
//     point (resolved design §2), colour alternating on the path exactly like
//     generateAnalyzeRequest().
//   * EngineController::analyzeMoves() — same EngineState / SearchIntent
//     bookkeeping as analyze(), so the search's trailing best-move coordinate
//     is discarded by the ANLZ-06 gate rather than played on the board
//     (resolved design §4/§5). That last point is the one PROTO-07 relies on
//     existing code for, so it is pinned here with an INBOUND coordinate line
//     fed the way test_anlz06_search_intent_gate.cpp does.
//
// See docs/todo/PROTO-07-yxanalz-root-move-allowlist.md.

#include "vendor/doctest.h"

#include "engine/engine_controller.h"
#include "engine/engine_process.h"
#include "engine/gomocup_protocol.h"
#include "model/game_state.h"

#include <glibmm.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace {

bool proto07PumpUntil(const std::function<bool()> &done, int timeoutMs = 3000)
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

// PORT-03 portable cat-like stand-in; never echoes a coordinate-shaped line,
// so inbound traffic below is simulated explicitly (same rationale as
// test_anlz06_search_intent_gate.cpp).
constexpr const char *kProto07FakeEngine = MOCK_ENGINE_PATH;

struct Proto07Fixture {
    GameState gs;
    EngineProcess proc;
    EngineController ctrl{gs, proc};
    int moveCount = 0;
    Coord lastMove{};
    std::vector<std::string> sent;

    Proto07Fixture()
    {
        ctrl.signal_engine_move.connect([this](Coord c) {
            ++moveCount;
            lastMove = c;
        });
        proc.signal_line_sent.connect([this](const std::string &l) { sent.push_back(l); });

        EngineConfig cfg = gs.engineConfig();
        cfg.enginePath = kProto07FakeEngine;
        gs.setEngineConfig(cfg);

        ctrl.startEngine();
        REQUIRE(ctrl.engineState() == EngineController::EngineState::Idle);
        REQUIRE(proc.isRunning());
        sent.clear();
    }

    ~Proto07Fixture()
    {
        ctrl.stopEngine([] {});
        proto07PumpUntil([&] { return !proc.isRunning(); });
    }
};

}  // namespace

// ─── Layer 1: the wire block ────────────────────────────────────────────────

TEST_CASE("PROTO-07: generateAnalyzeMovesRequest emits YXBOARD/DONE then YXANALZ/DONE")
{
    GomocupProtocol proto(15);
    // Path in display orientation; coordToEngine() writes "y,x".
    auto cmds = proto.generateAnalyzeMovesRequest({Coord{7, 7}, Coord{7, 8}},
                                                 {Coord{6, 9}, Coord{0, 14}});

    REQUIRE(cmds.size() == 8);
    CHECK(cmds[0] == "YXBOARD");
    CHECK(cmds[1] == "7,7,1");   // first stone is Black
    CHECK(cmds[2] == "8,7,2");   // second stone is White — colour alternates
    CHECK(cmds[3] == "DONE");
    CHECK(cmds[4] == "YXANALZ");
    CHECK(cmds[5] == "9,6");     // Coord{6,9}  -> "y,x"
    CHECK(cmds[6] == "14,0");    // Coord{0,14} -> "y,x"
    CHECK(cmds[7] == "DONE");

    // Structural invariants stated in the task's Scope, asserted as such.
    const auto yxboard = std::find(cmds.begin(), cmds.end(), std::string("YXBOARD"));
    const auto yxanalz = std::find(cmds.begin(), cmds.end(), std::string("YXANALZ"));
    REQUIRE(yxboard != cmds.end());
    REQUIRE(yxanalz != cmds.end());
    CHECK(yxboard < yxanalz);                  // position block precedes the list
    CHECK(*(yxanalz - 1) == "DONE");           // position block is DONE-terminated
    CHECK(cmds.back() == "DONE");              // move list is DONE-terminated
}

TEST_CASE("PROTO-07: generateAnalyzeMovesRequest on the empty board is a bare YXBOARD block")
{
    GomocupProtocol proto(15);
    auto cmds = proto.generateAnalyzeMovesRequest({}, {Coord{7, 7}});

    REQUIRE(cmds.size() == 5);
    CHECK(cmds[0] == "YXBOARD");
    CHECK(cmds[1] == "DONE");
    CHECK(cmds[2] == "YXANALZ");
    CHECK(cmds[3] == "7,7");
    CHECK(cmds[4] == "DONE");
}

TEST_CASE("PROTO-07: a longer path keeps alternating colours, like generateAnalyzeRequest")
{
    GomocupProtocol proto(15);
    const std::vector<Coord> path{Coord{7, 7}, Coord{7, 8}, Coord{8, 8}, Coord{6, 6}, Coord{9, 9}};
    auto cmds = proto.generateAnalyzeMovesRequest(path, {Coord{5, 5}});
    auto ref  = proto.generateAnalyzeRequest(path, 5);

    // The whole position block must be byte-identical to the YXNBEST path's;
    // only the trailing request differs.
    REQUIRE(cmds.size() >= path.size() + 2);
    for (size_t i = 0; i < path.size() + 2; ++i)
        CHECK(cmds[i] == ref[i]);

    CHECK(cmds[path.size() + 2] == "YXANALZ");
    CHECK(ref.back() == "YXNBEST 5");  // YXNBEST request path unchanged
}

TEST_CASE("PROTO-07: board size does not leak into coordinate conversion")
{
    // coordToEngine() is size-independent (plain "y,x"); pin it at a non-15
    // size so a future size-dependent flip cannot slip in unnoticed.
    GomocupProtocol proto(20);
    auto cmds = proto.generateAnalyzeMovesRequest({}, {Coord{19, 0}, Coord{0, 19}});
    REQUIRE(cmds.size() == 6);
    CHECK(cmds[3] == "0,19");
    CHECK(cmds[4] == "19,0");
}

// ─── Layer 2: controller state + the ANLZ-06 completion-coordinate gate ─────

TEST_CASE("PROTO-07: analyzeMoves() sets Analyzing + Analysis intent and sends the block")
{
    Proto07Fixture f;

    f.ctrl.analyzeMoves({Coord{7, 7}, Coord{7, 8}});

    CHECK(f.ctrl.engineState() == EngineController::EngineState::Analyzing);
    CHECK(f.gs.isAnalyzing());

    REQUIRE(f.sent.size() == 6);
    CHECK(f.sent[0] == "YXBOARD");
    CHECK(f.sent[1] == "DONE");
    CHECK(f.sent[2] == "YXANALZ");
    CHECK(f.sent[3] == "7,7");
    CHECK(f.sent[4] == "8,7");
    CHECK(f.sent[5] == "DONE");
}

TEST_CASE("PROTO-07: analyzeMoves()'s completion coordinate is discarded, not played (ANLZ-06)")
{
    Proto07Fixture f;

    f.ctrl.analyzeMoves({Coord{7, 7}, Coord{7, 8}});
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Analyzing);

    // The engine's stdout reader delivers the trailing best-move line exactly
    // like this. Resolved design §4: no stone is placed on YixinBoard's board.
    f.proc.signal_line_received.emit("7,7");

    CHECK(f.moveCount == 0);
    CHECK(f.gs.history().moveCount() == 0);
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);
    CHECK_FALSE(f.gs.isAnalyzing());
}

TEST_CASE("PROTO-07: an empty allow-list is never put on the wire")
{
    Proto07Fixture f;

    f.ctrl.analyzeMoves({});

    CHECK(f.sent.empty());
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);
    CHECK_FALSE(f.gs.isAnalyzing());
}

TEST_CASE("PROTO-07: analyzeMoves() while a search runs stops it first, then queues the new block")
{
    Proto07Fixture f;

    f.ctrl.analyze();
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Analyzing);
    f.sent.clear();

    f.ctrl.analyzeMoves({Coord{3, 3}});

    // STOP goes out immediately; PROTO-04 holds the YXANALZ block behind the
    // aborted search's trailing coordinate.
    REQUIRE_FALSE(f.sent.empty());
    CHECK(f.sent[0] == "STOP");
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);

    // The aborted search settles: its trailing coordinate flushes the queue.
    f.proc.signal_line_received.emit("7,7");

    CHECK(f.moveCount == 0);  // still analysis intent — no stone
    CHECK(f.gs.history().moveCount() == 0);
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Analyzing);
    REQUIRE(f.sent.size() >= 6);
    CHECK(f.sent.back() == "DONE");
    CHECK(f.sent[f.sent.size() - 2] == "3,3");
    CHECK(f.sent[f.sent.size() - 3] == "YXANALZ");
}

TEST_CASE("PROTO-07: no whitelist mirror state — a following analyze() is unchanged")
{
    Proto07Fixture f;

    f.ctrl.analyzeMoves({Coord{3, 3}});
    f.proc.signal_line_received.emit("3,3");  // search completes, discarded
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Idle);
    f.sent.clear();

    f.ctrl.analyze();

    REQUIRE_FALSE(f.sent.empty());
    CHECK(f.sent.front() == "YXBOARD");
    CHECK(f.sent.back().rfind("YXNBEST", 0) == 0);  // plain YXNBEST, no YXANALZ
    for (const auto &l : f.sent) CHECK(l != "YXANALZ");
}
