// Regression tests for ENG-01 — engine state honesty and blocking stop.
//
// Covers, per docs/todo/ENG-01-engine-state-honesty-and-blocking-stop.md's
// verification checklist:
//   (a) startEngine() with a bad path does NOT transition to a running state.
//   (b) state-enum transitions happen only on real events (no spurious
//       emits, and a real process death produces exactly one Crashed
//       transition — not a duplicate one per stdout+stderr EOF).
//   (c) the running_-flag de-dup guard in EngineProcess still prevents a
//       double signal_process_died when both stdout and stderr hit EOF.
//
// These tests spawn short-lived real subprocesses (/bin/true, a
// non-existent path) and pump the default GLib main context manually to let
// EngineProcess's async Gio reads/wait_async complete. That manual pumping
// (and the g_usleep polling wrapped around it) is test-harness plumbing
// only — it is exactly what production code must NOT do on the main thread,
// which is the bug this task fixes (see EngineProcess::stop() vs.
// EngineProcess::stopAsync() in src/engine/engine_process.cpp).

#include "vendor/doctest.h"

#include "engine/engine_controller.h"
#include "engine/engine_process.h"
#include "model/game_state.h"

#include <glibmm.h>

#include <algorithm>
#include <chrono>
#include <vector>

namespace {

/// Pump the default GLib main context until `done()` is true or `timeoutMs`
/// elapses. Returns true if `done()` became true before the timeout.
bool pumpUntil(const std::function<bool()> &done, int timeoutMs = 5000)
{
    auto *ctx = g_main_context_default();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!done()) {
        g_main_context_iteration(ctx, FALSE);
        if (std::chrono::steady_clock::now() >= deadline) return false;
        g_usleep(1000); // 1ms — just yield; not a production code path.
    }
    return true;
}

} // namespace

// ─── (a) Bad path must not produce a running state ─────────────────────────
TEST_CASE("EngineController::startEngine with a bad path stays NotStarted") {
    GameState gs;
    EngineProcess proc;
    EngineController ctrl(gs, proc);

    EngineConfig cfg = gs.engineConfig();
    cfg.enginePath = "/nonexistent/path/definitely-not-an-engine-binary-eng01";
    gs.setEngineConfig(cfg);

    std::vector<EngineController::EngineState> transitions;
    ctrl.signal_state_changed.connect(
        [&](EngineController::EngineState s) { transitions.push_back(s); });

    ctrl.startEngine();

    CHECK(ctrl.engineState() == EngineController::EngineState::NotStarted);
    CHECK_FALSE(ctrl.isStarted());
    // Must never have claimed Idle (the old "started_ = true" bug).
    CHECK(std::find(transitions.begin(), transitions.end(),
                     EngineController::EngineState::Idle) == transitions.end());
}

// ─── (b) No spurious transitions on a no-op stop ───────────────────────────
TEST_CASE("EngineController::stopEngine on a never-started engine emits nothing") {
    GameState gs;
    EngineProcess proc;
    EngineController ctrl(gs, proc);

    int emitCount = 0;
    ctrl.signal_state_changed.connect([&](EngineController::EngineState) { ++emitCount; });

    bool completed = false;
    ctrl.stopEngine([&]() { completed = true; });

    // NotStarted -> stopEngine() must be a synchronous, signal-free no-op
    // (this is the "stopEngine emits unconditionally" bug from the todo).
    CHECK(completed);
    CHECK(emitCount == 0);
    CHECK(ctrl.engineState() == EngineController::EngineState::NotStarted);
}

// ─── (b)+(c) Real process death: exactly one Crashed transition ───────────
TEST_CASE("EngineController reaches Crashed exactly once when the process dies unexpectedly") {
    GameState gs;
    EngineProcess proc;
    EngineController ctrl(gs, proc);

    EngineConfig cfg = gs.engineConfig();
    cfg.enginePath = "/bin/true"; // exits ~immediately with no args/output.
    gs.setEngineConfig(cfg);

    std::vector<EngineController::EngineState> transitions;
    ctrl.signal_state_changed.connect(
        [&](EngineController::EngineState s) { transitions.push_back(s); });

    ctrl.startEngine();
    REQUIRE(ctrl.engineState() == EngineController::EngineState::Idle);

    bool reachedCrashed = pumpUntil([&]() {
        return ctrl.engineState() == EngineController::EngineState::Crashed;
    });

    REQUIRE(reachedCrashed);

    int crashedCount = static_cast<int>(std::count(
        transitions.begin(), transitions.end(), EngineController::EngineState::Crashed));
    // If the running_-flag de-dup guard in EngineProcess::readStdout()/
    // readStderr() regressed, both EOFs would each fire signal_process_died
    // and this would be 2.
    CHECK(crashedCount == 1);
}

// ─── (c) EngineProcess-level: stdout+stderr EOF de-dup, directly ──────────
TEST_CASE("EngineProcess de-dups signal_process_died across stdout+stderr EOF") {
    EngineProcess proc;
    bool started = proc.start("/bin/true");
    REQUIRE(started);

    int diedCount = 0;
    proc.signal_process_died.connect([&]() { ++diedCount; });

    bool died = pumpUntil([&]() { return !proc.isRunning(); });

    REQUIRE(died);
    CHECK(diedCount == 1);
}
