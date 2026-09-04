// Widget-level regression guard for ANLZ-07, against a REAL MainWindow.
//
// ANLZ-06 correctly stopped an Analyze-Mode search's terminal coordinate from
// being played, but left scheduleAnalyzeModeRestart() re-arming a brand new
// YXBOARD+YXNBEST round-trip on EVERY transition to Idle, with no check that
// the position or the search's result had actually changed since the last
// run. Once a search converges quickly and stably (a forced mate found
// early, a small/solved position), this busy-loops at native CPU speed
// forever: STOP -> full board redump -> search -> discard -> STOP -> ...
//
// The fix: EngineController::analysisConverged() compares the just-completed
// analysis-intent search's result (best move + eval text) to the previous
// completed result FOR THE SAME POSITION (keyed on currentPath()).
// scheduleAnalyzeModeRestart() skips re-arming when it reports converged,
// unless `force` is set (a genuine position change, or the user explicitly
// toggling Analyze Mode off/on) — see main_window.cpp's doc comments.
//
// This suite drives a REAL MainWindow + a real (but inert) /bin/cat "engine"
// process, exactly like test_anlz05_no_automove_action.cpp's wire spy, and
// injects inbound engine lines directly on EngineProcess::signal_line_received
// (a public signal), exactly like test_anlz06_search_intent_gate.cpp — /bin/cat
// never produces a MESSAGE or coordinate line on its own, so nothing it
// echoes back interferes with the assertions below.
//
// See docs/fix-log/2026-09-04-anlz07-analyze-mode-restart-busy-loop.md.

#include "vendor/doctest.h"

#include <gtkmm.h>
#include <giomm.h>

#include "main_window.h"
#include "model/config.h"
#include "model/settings_storage.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

// Test-only accessor — MainWindow declares `friend struct RanlsAnlz07Probe`.
struct RanlsAnlz07Probe {
    MainWindow &w;
    GameState        &gs()   { return w.gameState_; }
    EngineProcess    &eng()  { return w.engine_; }
    EngineController &ctrl() { return w.controller_; }
    void scheduleAnalyzeModeRestart(bool force) { w.scheduleAnalyzeModeRestart(force); }
};

namespace {

Coord at(int x, int y) { return Coord{x, y}; }

bool gtkReady() { return gtk_init_check(); }

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

// Pumps the main loop for a short, fixed window instead of waiting for a
// condition — used to prove a NEGATIVE (no further request is ever sent),
// which pumpUntil's condition form can't express.
void pumpFor(int ms)
{
    auto *ctx = g_main_context_default();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < deadline) {
        g_main_context_iteration(ctx, FALSE);
        g_usleep(1000);
    }
}

int countPrefix(const std::vector<std::string> &v, const std::string &needle)
{
    int n = 0;
    for (const auto &s : v)
        if (s.rfind(needle, 0) == 0) ++n;
    return n;
}

// Stand-in "engine": /bin/cat stays alive reading stdin as long as its pipe is
// open. Real analysis-completion traffic is simulated separately by emitting
// directly on EngineProcess::signal_line_received; cat itself never produces
// a MESSAGE or coordinate-shaped line, so nothing it echoes back interferes.
constexpr const char *kFakeEngine = "/bin/cat";

// Feeds one completed analysis-intent search's result: a MESSAGE line that
// sets EngineStatus::bestMove (parsed while the search is still in flight,
// exactly like a real engine's periodic REALTIME report), then the bare
// coordinate line that signals search completion (YXNBEST always emits one).
void feedCompletedAnalysis(EngineProcess &eng, Coord bestMove)
{
    std::string msg = "MESSAGE REALTIME BEST " + std::to_string(bestMove.y) + ","
                     + std::to_string(bestMove.x);
    eng.signal_line_received.emit(msg);
    eng.signal_line_received.emit(std::to_string(bestMove.y) + "," + std::to_string(bestMove.x));
}

}  // namespace

TEST_CASE("ANLZ-07: two identical completed analysis results skip the second restart")
{
    if (!gtkReady()) return;

    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    MainWindow window;
    RanlsAnlz07Probe p{window};

    EngineConfig ec = p.gs().engineConfig();
    ec.enginePath = kFakeEngine;
    p.gs().setEngineConfig(ec);

    p.ctrl().startEngine();
    REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Idle);
    REQUIRE(p.eng().isRunning());

    ViewConfig vc = p.gs().viewConfig();
    vc.analyzeMode = true;
    p.gs().setViewConfig(vc);

    std::vector<std::string> wire;
    p.eng().signal_line_sent.connect([&](const std::string &l) { wire.push_back(l); });

    // Kick off the first search (mirrors onToggleAnalyzeMode(true)'s forced kick).
    p.scheduleAnalyzeModeRestart(/*force=*/true);
    REQUIRE(pumpUntil([&] { return countPrefix(wire, "YXNBEST") == 1; }));
    REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Analyzing);

    // Search #1 completes with best move (7,7). No previous result exists yet
    // for this position, so the routine (unforced) Idle-triggered restart
    // must still fire — ANLZ-01's "every newly-visited position gets
    // analysed" guarantee.
    feedCompletedAnalysis(p.eng(), at(7, 7));
    REQUIRE(pumpUntil([&] { return countPrefix(wire, "YXNBEST") == 2; }));

    // Search #2 completes with the SAME best move (7,7) on the SAME position:
    // the result has converged. The routine restart must now be skipped.
    feedCompletedAnalysis(p.eng(), at(7, 7));
    REQUIRE(pumpUntil([&] { return p.ctrl().engineState() == EngineController::EngineState::Idle; }));
    CHECK(p.ctrl().analysisConverged());

    // Prove the negative: pump for a while longer and confirm no third
    // round-trip is ever sent — this is the busy-loop the fix eliminates.
    pumpFor(200);
    CHECK(countPrefix(wire, "YXNBEST") == 2);

    p.ctrl().stopEngine([] {});
    pumpUntil([&] { return !p.eng().isRunning(); });
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

TEST_CASE("ANLZ-07: a changed result on the same position keeps restarting")
{
    if (!gtkReady()) return;

    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    MainWindow window;
    RanlsAnlz07Probe p{window};

    EngineConfig ec = p.gs().engineConfig();
    ec.enginePath = kFakeEngine;
    p.gs().setEngineConfig(ec);

    p.ctrl().startEngine();
    REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Idle);

    ViewConfig vc = p.gs().viewConfig();
    vc.analyzeMode = true;
    p.gs().setViewConfig(vc);

    std::vector<std::string> wire;
    p.eng().signal_line_sent.connect([&](const std::string &l) { wire.push_back(l); });

    p.scheduleAnalyzeModeRestart(/*force=*/true);
    REQUIRE(pumpUntil([&] { return countPrefix(wire, "YXNBEST") == 1; }));

    // Search #1 completes: (7,7). First-ever result — restarts (search #2).
    feedCompletedAnalysis(p.eng(), at(7, 7));
    REQUIRE(pumpUntil([&] { return countPrefix(wire, "YXNBEST") == 2; }));
    CHECK_FALSE(p.ctrl().analysisConverged());

    // Search #2 completes with a DIFFERENT best move: (8, 8) — still
    // converging (e.g. deepening progress), must restart again (search #3).
    feedCompletedAnalysis(p.eng(), at(8, 8));
    REQUIRE(pumpUntil([&] { return countPrefix(wire, "YXNBEST") == 3; }));
    CHECK_FALSE(p.ctrl().analysisConverged());

    p.ctrl().stopEngine([] {});
    pumpUntil([&] { return !p.eng().isRunning(); });
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

TEST_CASE("ANLZ-07: a real position change always restarts, even matching a different position's cached result")
{
    if (!gtkReady()) return;

    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    MainWindow window;
    RanlsAnlz07Probe p{window};

    EngineConfig ec = p.gs().engineConfig();
    ec.enginePath = kFakeEngine;
    p.gs().setEngineConfig(ec);

    p.ctrl().startEngine();
    REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Idle);

    ViewConfig vc = p.gs().viewConfig();
    vc.analyzeMode = true;
    p.gs().setViewConfig(vc);

    std::vector<std::string> wire;
    p.eng().signal_line_sent.connect([&](const std::string &l) { wire.push_back(l); });

    // Converge the search on the empty-board position (P = []): two identical
    // completions, exactly like the first test.
    p.scheduleAnalyzeModeRestart(/*force=*/true);
    REQUIRE(pumpUntil([&] { return countPrefix(wire, "YXNBEST") == 1; }));

    feedCompletedAnalysis(p.eng(), at(7, 7));
    REQUIRE(pumpUntil([&] { return countPrefix(wire, "YXNBEST") == 2; }));

    feedCompletedAnalysis(p.eng(), at(7, 7));
    REQUIRE(pumpUntil([&] { return p.ctrl().engineState() == EngineController::EngineState::Idle; }));
    REQUIRE(p.ctrl().analysisConverged());
    pumpFor(150);
    REQUIRE(countPrefix(wire, "YXNBEST") == 2);  // converged, settled — no 3rd

    // A genuine position change: play a move (Q = [(0,0)]). This fires
    // signal_board_changed -> scheduleAnalyzeModeRestart(force=true) — must
    // restart despite the cached "converged" state, which was for P, not Q.
    REQUIRE(p.gs().makeMove(at(0, 0)));
    REQUIRE(pumpUntil([&] { return countPrefix(wire, "YXNBEST") == 3; }));

    // The new position's search happens to complete with the SAME best move
    // value (7,7) that was cached as "converged" for the OLD position P.
    // Position-keying must not let that leak across positions: since this is
    // the first completion for Q, it must still restart (search #4), not be
    // silently swallowed as "unchanged".
    feedCompletedAnalysis(p.eng(), at(7, 7));
    REQUIRE(pumpUntil([&] { return countPrefix(wire, "YXNBEST") == 4; }));

    p.ctrl().stopEngine([] {});
    pumpUntil([&] { return !p.eng().isRunning(); });
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}
