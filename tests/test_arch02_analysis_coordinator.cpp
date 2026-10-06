// ARCH-02: AnalysisCoordinator unit tests (model + engine layers, no GTK).
//
// The coordinator's only platform seam is the injected `postIdle` scheduler;
// these tests use a MANUAL queue they drain explicitly — no GTK, no main loop
// for the scheduling itself. A real EngineController runs against the PORT-03
// mock engine (same approach as test_anlz06 / test_proto07); inbound engine
// lines are simulated via EngineProcess::signal_line_received.
//
// These mirror the ARCH-04 MainWindow characterization tests (which keep
// running through the friend probes) at the coordinator level.

#include "vendor/doctest.h"

#include "engine/analysis_coordinator.h"
#include "engine/engine_controller.h"
#include "engine/engine_process.h"
#include "model/game_state.h"
#include "model/settings_storage.h"

#include <glibmm.h>

#include <chrono>
#include <cstdio>
#include <deque>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

using State = EngineController::EngineState;

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

/// Deterministic stand-in for the Glib idle loop.
struct ManualScheduler {
    std::deque<std::function<void()>> q;
    AnalysisCoordinator::PostIdle fn()
    {
        return [this](std::function<void()> cb) { q.push_back(std::move(cb)); };
    }
    size_t pending() const { return q.size(); }
    /// Runs every callback queued at call time (and any queued meanwhile).
    void drain()
    {
        while (!q.empty()) {
            auto cb = std::move(q.front());
            q.pop_front();
            cb();
        }
    }
};

int countPrefix(const std::vector<std::string> &v, const std::string &p)
{
    int n = 0;
    for (const auto &s : v)
        if (s.rfind(p, 0) == 0) ++n;
    return n;
}

void feedCompletedAnalysis(EngineProcess &eng, int x, int y)
{
    eng.signal_line_received.emit("MESSAGE REALTIME BEST " + std::to_string(y) + ","
                                  + std::to_string(x));
    eng.signal_line_received.emit(std::to_string(y) + "," + std::to_string(x));
}

struct Fixture {
    GameState gs;
    EngineProcess proc;
    EngineController ctrl{gs, proc};
    ManualScheduler sched;
    std::vector<std::string> wire;
    std::unique_ptr<AnalysisCoordinator> co;

    explicit Fixture(bool startEngine = true)
    {
        proc.signal_line_sent.connect([this](const std::string &l) { wire.push_back(l); });
        co = std::make_unique<AnalysisCoordinator>(gs, ctrl, sched.fn());
        if (startEngine) {
            EngineConfig cfg = gs.engineConfig();
            cfg.enginePath = MOCK_ENGINE_PATH;
            gs.setEngineConfig(cfg);
            ctrl.startEngine();
            REQUIRE(ctrl.engineState() == State::Idle);
            REQUIRE(proc.isRunning());
        }
    }
    ~Fixture()
    {
        co.reset();
        ctrl.stopEngine([] {});
        pumpUntil([&] { return !proc.isRunning(); });
    }

    void setAnalyzeMode(bool on)
    {
        ViewConfig vc = gs.viewConfig();
        vc.analyzeMode = on;
        gs.setViewConfig(vc);
    }
    void setPlays(EnginePlaysSide s)
    {
        MatchConfig mc = gs.matchConfig();
        mc.enginePlays = s;
        gs.setMatchConfig(mc);
    }
    int searches() const { return countPrefix(wire, "YXNBEST"); }

    /// Two identical completed analysis-intent searches => analysisConverged().
    void converge()
    {
        ctrl.analyze();
        REQUIRE(ctrl.engineState() == State::Analyzing);
        feedCompletedAnalysis(proc, 7, 7);
        ctrl.analyze();
        REQUIRE(ctrl.engineState() == State::Analyzing);
        feedCompletedAnalysis(proc, 7, 7);
        REQUIRE(ctrl.engineState() == State::Idle);
        REQUIRE(ctrl.analysisConverged());
    }
};

}  // namespace

TEST_CASE("ARCH-02: a burst of restart requests coalesces into exactly one restart")
{
    Fixture f;
    f.setAnalyzeMode(true);
    const int before = f.searches();

    for (int i = 0; i < 5; ++i) f.co->scheduleAnalyzeModeRestart(true);
    CHECK(f.sched.pending() == 1);
    CHECK(f.co->analyzeModeScheduled());

    f.sched.drain();
    CHECK_FALSE(f.co->analyzeModeScheduled());
    CHECK(f.searches() == before + 1);
    CHECK(f.ctrl.engineState() == State::Analyzing);
}

TEST_CASE("ARCH-02: restart is inert while Analyze Mode is off")
{
    Fixture f;
    f.co->scheduleAnalyzeModeRestart(true);
    CHECK(f.sched.pending() == 0);
    CHECK_FALSE(f.co->analyzeModeScheduled());
}

TEST_CASE("ARCH-02: force latch is not downgraded by a later non-forced call")
{
    Fixture f;
    f.setAnalyzeMode(true);
    f.converge();
    const int before = f.searches();

    // Control: lone non-forced call is skipped (converged).
    f.co->scheduleAnalyzeModeRestart(false);
    f.sched.drain();
    CHECK(f.searches() == before);
    REQUIRE(f.ctrl.analysisConverged());

    // forced then non-forced before the callback runs => still forced.
    f.co->scheduleAnalyzeModeRestart(true);
    f.co->scheduleAnalyzeModeRestart(false);
    CHECK(f.co->analyzeModeForce());
    CHECK(f.sched.pending() == 1);
    f.sched.drain();
    CHECK_FALSE(f.co->analyzeModeForce());  // consumed
    CHECK(f.searches() == before + 1);
}

TEST_CASE("ARCH-02: converged result skips a routine restart, force bypasses it")
{
    Fixture f;
    f.setAnalyzeMode(true);
    f.converge();
    const int before = f.searches();

    f.co->scheduleAnalyzeModeRestart();
    f.sched.drain();
    CHECK(f.searches() == before);
    CHECK(f.ctrl.engineState() == State::Idle);

    f.co->scheduleAnalyzeModeRestart(true);
    f.sched.drain();
    CHECK(f.searches() == before + 1);
    CHECK(f.ctrl.engineState() == State::Analyzing);
}

TEST_CASE("ARCH-02: restart does nothing when the engine is not running or not Idle")
{
    {
        Fixture f(/*startEngine=*/false);
        f.setAnalyzeMode(true);
        f.co->scheduleAnalyzeModeRestart(true);
        f.sched.drain();
        CHECK(f.searches() == 0);
        CHECK(f.ctrl.engineState() == State::NotStarted);
    }
    {
        Fixture f;
        f.setAnalyzeMode(true);
        f.ctrl.analyze();  // now Analyzing, not Idle
        const int before = f.searches();
        f.co->scheduleAnalyzeModeRestart(true);
        f.sched.drain();
        CHECK(f.searches() == before);
    }
}

TEST_CASE("ARCH-02: Analyze Mode on blocks auto-move even on the engine's turn (ANLZ-05)")
{
    Fixture f;
    f.setPlays(EnginePlaysSide::Black);  // empty board: Black to move
    f.setAnalyzeMode(true);

    f.co->maybeStartAutoMove();
    f.sched.drain();
    CHECK(f.ctrl.engineState() == State::Idle);  // no move requested
}

TEST_CASE("ARCH-02: auto-move fires only on the engine's turn and only when Idle")
{
    Fixture f;

    // enginePlays Off => inert, nothing scheduled.
    f.co->maybeStartAutoMove();
    CHECK(f.sched.pending() == 0);

    // Not the engine's turn (White assigned, Black to move).
    f.setPlays(EnginePlaysSide::White);
    f.co->maybeStartAutoMove();
    CHECK(f.sched.pending() == 1);
    f.co->maybeStartAutoMove();  // coalesced
    CHECK(f.sched.pending() == 1);
    f.sched.drain();
    CHECK(f.ctrl.engineState() == State::Idle);
    CHECK_FALSE(f.co->autoMoveScheduled());

    // Engine's turn + Idle => requests a move.
    f.setPlays(EnginePlaysSide::Black);
    f.co->maybeStartAutoMove();
    f.sched.drain();
    CHECK(f.ctrl.engineState() == State::Analyzing);

    // Engine's turn but not Idle (already searching) => no second request.
    const auto lines = f.wire.size();
    f.co->maybeStartAutoMove();
    f.sched.drain();
    CHECK(f.wire.size() == lines);
}

TEST_CASE("ARCH-02: auto-move needs a running engine")
{
    Fixture f(/*startEngine=*/false);
    f.setPlays(EnginePlaysSide::Black);
    f.co->maybeStartAutoMove();
    f.sched.drain();
    CHECK(f.ctrl.engineState() == State::NotStarted);
    CHECK(f.wire.empty());
}

TEST_CASE("ARCH-02: Analyze Mode off stops the search and leaves enginePlays untouched")
{
    Fixture f;
    f.setPlays(EnginePlaysSide::White);
    f.setAnalyzeMode(true);
    f.ctrl.analyze();
    REQUIRE(f.ctrl.engineState() == State::Analyzing);

    f.setAnalyzeMode(false);
    f.co->onAnalyzeModeToggled(false);
    CHECK(f.ctrl.engineState() == State::Idle);
    CHECK(f.gs.matchConfig().enginePlays == EnginePlaysSide::White);
    CHECK(f.sched.pending() == 0);
    CHECK(f.proc.isRunning());  // process left running

    // And turning it on schedules a forced restart.
    f.setAnalyzeMode(true);
    f.co->onAnalyzeModeToggled(true);
    CHECK(f.sched.pending() == 1);
    CHECK(f.co->analyzeModeForce());
}

TEST_CASE("ARCH-02: ENG-02 revert sets Off, notifies, and never persists")
{
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
    {
        Fixture f(/*startEngine=*/false);
        int notified = 0;
        f.co->signal_engine_plays_reverted.connect([&] { ++notified; });

        // No-op when already Off: no notification.
        f.co->revertEnginePlaysToOff();
        CHECK(notified == 0);

        f.setPlays(EnginePlaysSide::Black);
        f.co->revertEnginePlaysToOff();
        CHECK(f.gs.matchConfig().enginePlays == EnginePlaysSide::Off);
        CHECK(notified == 1);

        // If-engine's-turn variant: Black assigned & Black to move => reverts;
        // White assigned & Black to move => untouched.
        f.setPlays(EnginePlaysSide::White);
        f.co->revertEnginePlaysIfEnginesTurn();
        CHECK(f.gs.matchConfig().enginePlays == EnginePlaysSide::White);
        CHECK(notified == 1);
        f.setPlays(EnginePlaysSide::Black);
        f.co->revertEnginePlaysIfEnginesTurn();
        CHECK(f.gs.matchConfig().enginePlays == EnginePlaysSide::Off);
        CHECK(notified == 2);

        // Manual Stop reverts too.
        f.setPlays(EnginePlaysSide::White);
        f.co->stopAnalysis();
        CHECK(f.gs.matchConfig().enginePlays == EnginePlaysSide::Off);
        CHECK(notified == 3);

        // setEnginePlays reports change but does not persist either.
        CHECK(f.co->setEnginePlays(EnginePlaysSide::Black));
        CHECK_FALSE(f.co->setEnginePlays(EnginePlaysSide::Black));
    }
    CHECK_FALSE(std::ifstream(SettingsStorage::settingsFilePath()).good());
}

TEST_CASE("ARCH-02: a callback that runs after the coordinator is destroyed is a no-op")
{
    Fixture f;
    f.setAnalyzeMode(true);
    f.setPlays(EnginePlaysSide::Black);
    f.co->scheduleAnalyzeModeRestart(true);
    f.co->maybeStartAutoMove();
    REQUIRE(f.sched.pending() == 2);
    const auto lines = f.wire.size();

    f.co.reset();
    f.sched.drain();  // must not touch the dead coordinator
    CHECK(f.wire.size() == lines);
    CHECK(f.ctrl.engineState() == State::Idle);
}
