// Widget-level regression guard for ANLZ-05, against a REAL MainWindow.
//
// ANLZ-05 changes MainWindow orchestration only:
//   * maybeStartAutoMove()'s idle callback bails when viewConfig().analyzeMode
//     is on — the engine never auto-plays while Analyze Mode is on, not even on
//     its own assigned turn (planning.md Q6 reversed);
//   * scheduleAnalyzeModeRestart()'s idle callback no longer skips the
//     engine's-turn position — it analyses every position.
//
// This drives both idle callbacks (the ones signal_board_changed schedules)
// against a live-but-fake engine process and inspects the actual protocol
// lines sent on the wire:
//   * analyze()      → "YXBOARD" ... "YXNBEST N"   (GomocupProtocol)
//   * move request   → "BEGIN" (empty board) / "BOARD" ...
//
// Scenario A (Analyze Mode ON, engine's turn): only the analyze lines go out,
//   never BEGIN/BOARD — the Q6 reversal.
// Scenario B (Analyze Mode OFF, same setup): the auto-move BEGIN still goes out
//   — no regression to the "Engine plays <side>" path.
//
// Links gtkmm; self-skips with no display server (main() lives in
// test_ui07_pv_view_rows.cpp and probes gtk_init_check()).

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

// Test-only accessor — MainWindow declares `friend struct RanlsAnlz05Probe`.
struct RanlsAnlz05Probe {
    MainWindow &w;
    GameState        &gs()   { return w.gameState_; }
    EngineProcess    &eng()  { return w.engine_; }
    EngineController  &ctrl() { return w.controller_; }
    void scheduleAnalyzeModeRestart() { w.scheduleAnalyzeModeRestart(); }
    void maybeStartAutoMove()         { w.maybeStartAutoMove(); }
};

namespace {

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

bool sent(const std::vector<std::string> &v, const std::string &needle)
{
    for (const auto &s : v)
        if (s.rfind(needle, 0) == 0) return true;
    return false;
}

// Stand-in "engine": /bin/cat stays alive reading stdin. The board is kept empty
// throughout, so the only lines that ever go out are keyword commands (never
// coordinate-shaped) — cat echoes them back harmlessly, none parse as a move.
constexpr const char *kFakeEngine = "/bin/cat";

}  // namespace

TEST_CASE("ANLZ-05: Analyze Mode blocks auto-move and analyses the engine's-turn position")
{
    if (!gtkReady()) return;

    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    MainWindow window;
    RanlsAnlz05Probe p{window};

    EngineConfig ec = p.gs().engineConfig();
    ec.enginePath = kFakeEngine;
    p.gs().setEngineConfig(ec);

    p.ctrl().startEngine();
    REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Idle);
    REQUIRE(p.eng().isRunning());

    std::vector<std::string> wire;
    p.eng().signal_line_sent.connect([&](const std::string &l) { wire.push_back(l); });

    // Empty board → Black to move. Engine is assigned Black: it is the engine's turn.
    MatchConfig mc = p.gs().matchConfig();
    mc.enginePlays = EnginePlaysSide::Black;
    p.gs().setMatchConfig(mc);
    REQUIRE(isEnginesTurn(p.gs().matchConfig().enginePlays, p.gs().board().sideToMove()));

    // ── Scenario A: Analyze Mode ON ──────────────────────────────────────────
    {
        ViewConfig vc = p.gs().viewConfig();
        vc.analyzeMode = true;
        p.gs().setViewConfig(vc);

        wire.clear();
        p.scheduleAnalyzeModeRestart();   // both are wired to signal_board_changed
        p.maybeStartAutoMove();

        REQUIRE(pumpUntil([&] { return sent(wire, "YXNBEST"); }));

        CHECK(sent(wire, "YXBOARD"));     // analyze() ran on the engine's-turn position
        CHECK(p.gs().isAnalyzing());
        CHECK_FALSE(sent(wire, "BEGIN")); // ...and auto-move did NOT (Q6 reversed)
        CHECK_FALSE(sent(wire, "BOARD"));
    }

    // ── Scenario B: Analyze Mode OFF, same setup → auto-move still fires ──────
    {
        p.ctrl().stopAnalysis();
        REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Idle);

        ViewConfig vc = p.gs().viewConfig();
        vc.analyzeMode = false;
        p.gs().setViewConfig(vc);

        wire.clear();
        p.scheduleAnalyzeModeRestart();   // no-ops: analyzeMode off
        p.maybeStartAutoMove();

        REQUIRE(pumpUntil([&] { return sent(wire, "BEGIN"); }));

        CHECK(sent(wire, "BEGIN"));       // "Engine plays Black" auto-move path intact
        CHECK_FALSE(sent(wire, "YXNBEST"));
    }

    p.ctrl().stopEngine([] {});
    pumpUntil([&] { return !p.eng().isRunning(); });
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}
