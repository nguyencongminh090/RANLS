// Regression tests for PROTO-04 (model / engine layer, no gtkmm).
//
// The user-reported symptom: an Engine Log full of
// "ERROR Unknown command: <coord>" lines following every yxquerydatabaseallt
// query, intermittently — the same outbound bytes sometimes accepted,
// sometimes rejected. Root cause was not a malformed command (PROTO-03's
// missing color field was real but insufficient — it explained a plausible
// defect, not the observed intermittency). The actual cause is a race:
// stopAnalysis() flips EngineController::state_ back to Idle SYNCHRONOUSLY
// the instant STOP is written to stdin, but the real engine subprocess's
// in-flight YXNBEST search is still asynchronously winding down and has not
// actually consumed STOP yet. The click handler that calls stopAnalysis()
// immediately follows it with gameState_.makeMove()'s signal_board_changed,
// which fires queryDatabase() (and, via Analyze Mode, a fresh analyze())
// right away — racing the aborted search's still-in-flight teardown on the
// wire. Depending on scheduling, that either lands cleanly or corrupts the
// engine's read position, matching the log's intermittent errors.
//
// Fix: EngineController::sendOrDefer() gates every outbound command batch
// (queryDatabase(), analyze(), requestEngineMove(), sendConfig()) behind a
// pendingStopFlush_ flag that stopAnalysis() arms only when it is actually
// interrupting a live Analysis-intent (YXNBEST) search — the one case
// documented (ANLZ-06) to still emit a trailing coordinate line despite
// STOP. Deferred commands flush, in order, only once that coordinate line
// confirms the wire is idle.
//
// See docs/fix-log/2026-09-05-proto-04-stop-flush-race.md.

#include "vendor/doctest.h"

#include "engine/engine_controller.h"
#include "engine/engine_process.h"
#include "model/game_state.h"

#include <glibmm.h>

#include <algorithm>
#include <chrono>
#include <vector>

namespace {

bool pumpUntil(const std::function<bool()> &done, int timeoutMs = 3000)
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

// Same stand-in as test_anlz06_search_intent_gate.cpp: /bin/cat stays alive
// reading stdin, never itself produces a coordinate-shaped line, so inbound
// traffic is simulated separately via EngineProcess::signal_line_received.
constexpr const char *kFakeEngine = "/bin/cat";

struct Fixture {
    GameState gs;
    EngineProcess proc;
    EngineController ctrl {gs, proc};
    std::vector<std::string> sent;

    Fixture()
    {
        proc.signal_line_sent.connect([this](const std::string &line) {
            sent.push_back(line);
        });

        EngineConfig cfg = gs.engineConfig();
        cfg.enginePath = kFakeEngine;
        gs.setEngineConfig(cfg);

        ctrl.startEngine();
        REQUIRE(ctrl.engineState() == EngineController::EngineState::Idle);
        REQUIRE(proc.isRunning());
    }

    ~Fixture()
    {
        ctrl.stopEngine([] {});
        pumpUntil([&] { return !proc.isRunning(); });
    }
};

} // namespace

TEST_CASE("PROTO-04: queryDatabase() sent right after stopAnalysis() is held, "
          "not raced onto the wire, while a YXNBEST search is still settling")
{
    Fixture f;

    f.ctrl.analyze(); // sends YXBOARD/DONE/YXNBEST — a live Analysis-intent search
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Analyzing);
    f.sent.clear();

    f.ctrl.stopAnalysis(); // sends STOP, flips state_ to Idle synchronously
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Idle);
    CHECK(f.sent == std::vector<std::string>{"STOP"});

    // The click handler's very next line, synchronously: queryDatabase().
    // Before the fix this went straight onto the wire here, racing the
    // aborted search's still-in-flight trailing coordinate.
    f.ctrl.queryDatabase();

    // Held: nothing past STOP has actually been written yet.
    CHECK(f.sent == std::vector<std::string>{"STOP"});

    // The aborted search's trailing coordinate now arrives (ANLZ-06: YXNBEST
    // still emits one even after STOP) — this is the wire-idle confirmation.
    f.proc.signal_line_received.emit("7,7");

    // Only now does the deferred query actually reach the wire, in order.
    REQUIRE(f.sent.size() > 1);
    CHECK(f.sent[1] == "yxquerydatabaseallt");
    CHECK(f.sent.back() == "DONE");
}

TEST_CASE("PROTO-04: queryDatabase() sends immediately when there was no live search to stop")
{
    Fixture f;

    // Idle engine, nothing running — stopAnalysis() is a no-op search-wise.
    f.ctrl.stopAnalysis();
    CHECK(f.sent == std::vector<std::string>{"STOP"});

    f.ctrl.queryDatabase();

    // No trailing coordinate to wait for — sent right away, matching the
    // log's occurrences where the previous search had already settled
    // before the click-driven stopAnalysis() ran.
    REQUIRE(f.sent.size() > 1);
    CHECK(f.sent[1] == "yxquerydatabaseallt");
}

TEST_CASE("PROTO-04: a Move-intent (requestEngineMove) stop does not wait for a "
          "coordinate that STOP genuinely suppresses")
{
    Fixture f;

    f.ctrl.requestEngineMove(); // Move-intent search — STOP suppresses its reply
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Analyzing);
    f.sent.clear();

    f.ctrl.stopAnalysis();
    CHECK(f.sent == std::vector<std::string>{"STOP"});

    // No coordinate is coming for a stopped Move-intent search — the next
    // command must not be held hostage waiting for one.
    f.ctrl.queryDatabase();
    REQUIRE(f.sent.size() > 1);
    CHECK(f.sent[1] == "yxquerydatabaseallt");
}

TEST_CASE("PROTO-04: multiple deferred commands flush in order")
{
    Fixture f;

    f.ctrl.analyze();
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Analyzing);
    f.sent.clear();

    f.ctrl.stopAnalysis();
    f.ctrl.queryDatabase();      // deferred #1
    f.ctrl.sendConfig();         // deferred #2 (New Game / rule change path)
    CHECK(f.sent == std::vector<std::string>{"STOP"});

    f.proc.signal_line_received.emit("7,7");

    // queryDatabase()'s block ("yxquerydatabaseallt" ... "DONE") must appear
    // in full before sendConfig()'s "START ..." line.
    auto queryPos  = std::find(f.sent.begin(), f.sent.end(), "yxquerydatabaseallt") - f.sent.begin();
    auto configPos = std::find_if(f.sent.begin(), f.sent.end(), [](const std::string &s) {
        return s.rfind("START", 0) == 0;
    }) - f.sent.begin();
    REQUIRE(static_cast<size_t>(queryPos) < f.sent.size());
    REQUIRE(static_cast<size_t>(configPos) < f.sent.size());
    CHECK(queryPos < configPos);
}

TEST_CASE("PROTO-04: a dead process drops the pending queue instead of hanging it forever")
{
    Fixture f;

    f.ctrl.analyze();
    REQUIRE(f.ctrl.engineState() == EngineController::EngineState::Analyzing);
    f.sent.clear();

    f.ctrl.stopAnalysis();
    f.ctrl.queryDatabase(); // deferred — waiting on a coordinate that will never come
    CHECK(f.sent == std::vector<std::string>{"STOP"});

    f.proc.signal_process_died.emit();
    CHECK(f.ctrl.engineState() == EngineController::EngineState::Crashed);

    // A stray coordinate line arriving after the crash (e.g. a half-flushed
    // pipe buffer) must not retroactively flush a queue that should already
    // have been dropped — if it weren't, this would send the deferred
    // queryDatabase() block onto a pipe the controller considers dead.
    f.proc.signal_line_received.emit("7,7");
    CHECK(f.sent == std::vector<std::string>{"STOP"});
}
