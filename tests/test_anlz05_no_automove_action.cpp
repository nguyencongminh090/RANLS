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
#include <fstream>
#include <iterator>
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
    // ARCH-04: read-only views of the idle-coalescing latches (characterization).
    bool autoMoveScheduled() const     { return w.autoMoveScheduled_; }
    bool analyzeModeScheduled() const  { return w.analyzeModeScheduled_; }
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

// ARCH-04: fixed-window pump, used to prove a NEGATIVE (nothing is ever sent).
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

std::string slurp(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool fileExists(const std::string &path) { return std::ifstream(path).good(); }

void setPlays(RanlsAnlz05Probe &p, EnginePlaysSide side)
{
    MatchConfig mc = p.gs().matchConfig();
    mc.enginePlays = side;
    p.gs().setMatchConfig(mc);
}

void setAnalyzeMode(RanlsAnlz05Probe &p, bool on)
{
    ViewConfig vc = p.gs().viewConfig();
    vc.analyzeMode = on;
    p.gs().setViewConfig(vc);
}

// Starts the mock engine and returns once it is Idle + running.
void startIdleEngine(RanlsAnlz05Probe &p)
{
    EngineConfig ec = p.gs().engineConfig();
    ec.enginePath = MOCK_ENGINE_PATH;
    p.gs().setEngineConfig(ec);
    p.ctrl().startEngine();
    REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Idle);
    REQUIRE(p.eng().isRunning());
}

void shutdownEngine(RanlsAnlz05Probe &p)
{
    p.ctrl().stopEngine([] {});
    pumpUntil([&] { return !p.eng().isRunning(); });
}

bool sent(const std::vector<std::string> &v, const std::string &needle)
{
    for (const auto &s : v)
        if (s.rfind(needle, 0) == 0) return true;
    return false;
}

// Stand-in "engine": PORT-03 mock_engine (portable /bin/cat analogue) stays alive reading stdin. The board is kept empty
// throughout, so the only lines that ever go out are keyword commands (never
// coordinate-shaped) — cat echoes them back harmlessly, none parse as a move.
constexpr const char *kFakeEngine = MOCK_ENGINE_PATH; // PORT-03: portable cat-like stand-in

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

        // PROTO-04: stopAnalysis() interrupting Scenario A's live YXNBEST search
        // arms pendingStopFlush_ — from here every outbound command defers until
        // the aborted search's trailing coordinate line lands (ANLZ-06: YXNBEST
        // still emits one after STOP). A real engine sends it (on this empty
        // board, its default move); the protocol-blind mock_engine never emits a
        // coordinate-shaped line, so feed the trailing coordinate the way
        // test_anlz06 / test_proto04 do — otherwise the deferred auto-move BEGIN
        // below stays queued forever and pumpUntil times out.
        p.eng().signal_line_received.emit("7,7");

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

// ── ARCH-04: characterization tests (no behaviour change; pin current rules) ─

TEST_CASE("ARCH-04: auto-move fires only on the engine's turn AND when the engine is Idle")
{
    if (!gtkReady()) return;
    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    MainWindow window;
    RanlsAnlz05Probe p{window};
    startIdleEngine(p);

    std::vector<std::string> wire;
    p.eng().signal_line_sent.connect([&](const std::string &l) { wire.push_back(l); });

    // Empty board: Black to move.
    // 1. enginePlays Off -> never.
    setPlays(p, EnginePlaysSide::Off);
    p.maybeStartAutoMove();
    CHECK_FALSE(p.autoMoveScheduled());  // Off bails before even scheduling
    pumpFor(100);
    CHECK(countPrefix(wire, "BEGIN") == 0);

    // 2. engine plays White, but it is Black's turn -> not the engine's turn.
    setPlays(p, EnginePlaysSide::White);
    p.maybeStartAutoMove();
    pumpFor(100);
    CHECK(countPrefix(wire, "BEGIN") == 0);
    CHECK(countPrefix(wire, "BOARD") == 0);

    // 3. engine's turn, but the engine is busy (Analyzing, not Idle) -> no move.
    setPlays(p, EnginePlaysSide::Black);
    p.ctrl().analyze();
    REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Analyzing);
    wire.clear();
    p.maybeStartAutoMove();
    pumpFor(100);
    CHECK(countPrefix(wire, "BEGIN") == 0);
    CHECK(countPrefix(wire, "BOARD") == 0);
    CHECK(p.ctrl().engineState() == EngineController::EngineState::Analyzing);

    // 4. engine's turn AND Idle -> exactly one move request.
    p.ctrl().stopAnalysis();
    REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Idle);
    p.eng().signal_line_received.emit("7,7");  // PROTO-04 trailing coordinate (see above)
    wire.clear();
    p.maybeStartAutoMove();
    REQUIRE(pumpUntil([&] { return countPrefix(wire, "BEGIN") >= 1; }));
    CHECK(countPrefix(wire, "BEGIN") == 1);
    CHECK(p.ctrl().engineState() != EngineController::EngineState::Idle);

    shutdownEngine(p);
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

TEST_CASE("ARCH-04: a burst of signal_board_changed yields one auto-move check")
{
    if (!gtkReady()) return;
    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    MainWindow window;
    RanlsAnlz05Probe p{window};
    startIdleEngine(p);

    std::vector<std::string> wire;
    p.eng().signal_line_sent.connect([&](const std::string &l) { wire.push_back(l); });

    setPlays(p, EnginePlaysSide::Black);  // engine's turn on the empty board
    REQUIRE_FALSE(p.autoMoveScheduled());

    // A synchronous burst, as a game load / undoAll produces. The scheduler
    // latch (autoMoveScheduled_) is the only direct observable for "one idle
    // callback queued": the wire count alone would also read 1 if a 2nd
    // callback ran, because it would bail on the non-Idle state.
    for (int i = 0; i < 5; ++i) p.gs().signal_board_changed.emit();
    CHECK(p.autoMoveScheduled());

    REQUIRE(pumpUntil([&] { return countPrefix(wire, "BEGIN") >= 1; }));
    pumpFor(100);
    CHECK(countPrefix(wire, "BEGIN") == 1);
    CHECK_FALSE(p.autoMoveScheduled());   // the one callback ran and cleared the latch

    shutdownEngine(p);
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

TEST_CASE("ARCH-04: toggling Analyze Mode off stops the search and leaves enginePlays untouched")
{
    if (!gtkReady()) return;
    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    MainWindow window;
    RanlsAnlz05Probe p{window};
    startIdleEngine(p);

    std::vector<std::string> wire;
    p.eng().signal_line_sent.connect([&](const std::string &l) { wire.push_back(l); });

    // Engine is assigned Black (the engine's turn) with Analyze Mode on and a
    // search in flight. Set the model directly so the toggle below is a real
    // on->off change and nothing else is scheduled.
    setPlays(p, EnginePlaysSide::Black);
    setAnalyzeMode(p, true);
    p.ctrl().analyze();
    REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Analyzing);
    wire.clear();

    auto action = std::dynamic_pointer_cast<Gio::SimpleAction>(
        window.lookup_action("analyze-mode"));
    REQUIRE(action);
    action->change_state(Glib::Variant<bool>::create(false));

    CHECK_FALSE(p.gs().viewConfig().analyzeMode);
    CHECK(countPrefix(wire, "STOP") == 1);                       // stopAnalysis() ran
    CHECK(p.ctrl().engineState() == EngineController::EngineState::Idle);
    // Orthogonal to ENG-02: revertEnginePlaysToOff() was NOT called (it would
    // have flipped this to Off).
    CHECK(p.gs().matchConfig().enginePlays == EnginePlaysSide::Black);

    shutdownEngine(p);
    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}

TEST_CASE("ARCH-04: the ENG-02 revert sets enginePlays Off in GameState and is not persisted")
{
    if (!gtkReady()) return;
    const std::string settingsPath = SettingsStorage::settingsFilePath().string();
    std::remove(settingsPath.c_str());

    MainWindow window;
    RanlsAnlz05Probe p{window};
    startIdleEngine(p);

    auto enginePlaysAction = std::dynamic_pointer_cast<Gio::SimpleAction>(
        window.lookup_action("engine-plays"));
    auto stopAction    = std::dynamic_pointer_cast<Gio::SimpleAction>(window.lookup_action("stop"));
    auto analyzeAction = std::dynamic_pointer_cast<Gio::SimpleAction>(window.lookup_action("analyze"));
    REQUIRE(enginePlaysAction);
    REQUIRE(stopAction);
    REQUIRE(analyzeAction);

    // Positive control: the user-chosen path (onSetEnginePlays) DOES persist, so
    // "file unchanged" below is a meaningful observation, not a dead probe.
    // (Black to move on the empty board; choosing White is not the engine's turn,
    // so no auto-move interferes.)
    REQUIRE_FALSE(fileExists(settingsPath));
    enginePlaysAction->activate(Glib::Variant<Glib::ustring>::create("white"));
    REQUIRE(p.gs().matchConfig().enginePlays == EnginePlaysSide::White);
    REQUIRE(fileExists(settingsPath));
    const std::string persisted = slurp(settingsPath);
    REQUIRE(persisted.find("engine_plays=2") != std::string::npos);  // White == 2

    // Now arm the engine's turn directly in the model (Black to move), without
    // touching the file, and snapshot it.
    setPlays(p, EnginePlaysSide::Black);
    REQUIRE(isEnginesTurn(p.gs().matchConfig().enginePlays, p.gs().board().sideToMove()));
    REQUIRE(slurp(settingsPath) == persisted);

    // (i) Stop -> onStopAnalysis() -> revertEnginePlaysToOff().
    stopAction->activate();
    CHECK(p.gs().matchConfig().enginePlays == EnginePlaysSide::Off);
    CHECK(slurp(settingsPath) == persisted);   // transient: file NOT rewritten

    // (ii) Analyze on the engine's turn -> onStartAnalysis() -> same revert.
    p.ctrl().stopAnalysis();
    p.eng().signal_line_received.emit("7,7");  // flush any PROTO-04 deferral
    setPlays(p, EnginePlaysSide::Black);
    REQUIRE(slurp(settingsPath) == persisted);
    analyzeAction->activate();
    CHECK(p.gs().matchConfig().enginePlays == EnginePlaysSide::Off);
    CHECK(slurp(settingsPath) == persisted);   // still not persisted

    // Nothing armed -> revert is a no-op (stays Off, file still untouched).
    stopAction->activate();
    CHECK(p.gs().matchConfig().enginePlays == EnginePlaysSide::Off);
    CHECK(slurp(settingsPath) == persisted);

    shutdownEngine(p);
    std::remove(settingsPath.c_str());
}
