// Regression test for ENG-03 — WM close button ("X") must route through the
// same graceful engine shutdown as the menu-Quit action, not GTK's default
// close.
//
// Covers docs/todo/ENG-03-orphaned-engine-on-crash-or-wm-close.md's
// acceptance criterion for the close-request handler:
//   * the first signal_close_request veto's the close and kicks off
//     controller_.stopEngine(...) (observed via EngineController's real
//     state transitions: Idle -> Stopping -> NotStarted) instead of letting
//     GTK close the window immediately;
//   * the close only actually happens once that async stop completes;
//   * a second signal_close_request while the first is still in flight is a
//     no-op (dedup via closeInFlight_ + stopEngine()'s own Stopping-state
//     completion chaining) — it must not double-send END/force_exit or fire
//     a second close.
//   * ENG-02 non-regression: the close path must never touch MatchConfig's
//     "Engine plays" setting — only revertEnginePlaysToOff() (wired to
//     manual-intervention paths, not this one) may change it.
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
#include <vector>

// Test-only accessor — MainWindow declares `friend struct RanlsEng03Probe`.
struct RanlsEng03Probe {
    MainWindow &w;
    GameState        &gs()   { return w.gameState_; }
    EngineProcess    &eng()  { return w.engine_; }
    EngineController &ctrl() { return w.controller_; }
    bool closeInFlight() const { return w.closeInFlight_; }
    // Drive the real signal_close_request handler exactly as GTK would when
    // the WM "X" is clicked, without a live WM. Glib::SignalProxy has no
    // emit() of its own — go through the underlying GObject signal
    // ("close-request", declared in GtkWindowClass) via g_signal_emit_by_name,
    // which invokes every connected handler (our lambda included) and
    // returns the accumulated gboolean result.
    bool emitCloseRequest() {
        gboolean result = FALSE;
        g_signal_emit_by_name(w.gobj(), "close-request", &result);
        return result;
    }
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

// Stand-in "engine": /bin/cat stays alive reading stdin until EOF/killed.
constexpr const char *kFakeEngine = "/bin/cat";

}  // namespace

TEST_CASE("ENG-03: WM close request routes through stopEngine and only closes on completion")
{
    if (!gtkReady()) return;

    std::remove(SettingsStorage::settingsFilePath().string().c_str());

    MainWindow window;
    RanlsEng03Probe p{window};

    EngineConfig ec = p.gs().engineConfig();
    ec.enginePath = kFakeEngine;
    p.gs().setEngineConfig(ec);

    p.ctrl().startEngine();
    REQUIRE(p.ctrl().engineState() == EngineController::EngineState::Idle);
    REQUIRE(p.eng().isRunning());

    // ENG-02 non-regression: record "Engine plays" before the close path
    // runs and assert it is untouched afterward — the close callback must
    // call close(), never onStopAnalysis()/revertEnginePlaysToOff().
    MatchConfig mc = p.gs().matchConfig();
    mc.enginePlays = EnginePlaysSide::Black;
    p.gs().setMatchConfig(mc);
    const EnginePlaysSide enginePlaysBefore = p.gs().matchConfig().enginePlays;

    std::vector<EngineController::EngineState> transitions;
    p.ctrl().signal_state_changed.connect(
        [&](EngineController::EngineState s) { transitions.push_back(s); });

    // First close request: must veto the close (return true) and kick off
    // the async stop — NOT close the window immediately.
    CHECK_FALSE(p.closeInFlight());
    bool vetoed = p.emitCloseRequest();
    CHECK(vetoed);
    CHECK(p.closeInFlight());

    // A second close request arriving while the stop is still in flight
    // (e.g. the user clicks X twice) must be inert: no extra state
    // transitions, no early close.
    bool secondVetoed = p.emitCloseRequest();
    CHECK_FALSE(secondVetoed); // closeInFlight_ guard: falls through to GTK.

    // The stop must actually be progressing (Stopping), then settle.
    REQUIRE(pumpUntil([&] { return p.ctrl().engineState() == EngineController::EngineState::NotStarted; }));

    // Real transitions observed on the engine: Stopping then NotStarted —
    // this is stopEngine()'s real async path, not a synchronous shortcut.
    CHECK(std::find(transitions.begin(), transitions.end(),
                     EngineController::EngineState::Stopping) != transitions.end());
    CHECK(std::find(transitions.begin(), transitions.end(),
                     EngineController::EngineState::NotStarted) != transitions.end());

    // ENG-02 non-regression: MatchConfig's "Engine plays" side is untouched
    // by the whole close path.
    CHECK(p.gs().matchConfig().enginePlays == enginePlaysBefore);

    std::remove(SettingsStorage::settingsFilePath().string().c_str());
}
