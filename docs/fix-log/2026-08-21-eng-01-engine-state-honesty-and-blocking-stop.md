# 2026-08-21 — ENG-01: engine state honesty and blocking stop

**Status:** ✅ FIXED

## Summary

`EngineController` tracked engine lifecycle with two independent bools (`started_`, `analyzing_`).
Reality has six states, so the bools lied in five ways:

1. `startEngine()` (`src/engine/engine_controller.cpp:76-78`, old) discarded
   `EngineProcess::start()`'s return value and set `started_ = true` unconditionally — a bad engine
   path produced a green "● ON" with no process behind it.
2. `signal_process_died`'s handler emitted `signal_engine_state(false)`, rendering identically to
   "never started" — no distinct crash indication.
3. `signal_analyzing_state` only toggled button sensitivity; the status label never showed
   "thinking" separately from "on".
4. `EngineController::stopEngine()` called `g_usleep(500000)`, and `EngineProcess::stop()` busy-waited
   up to 2000ms via `g_usleep(20000)` in a loop that also called
   `g_main_context_iteration(NULL, FALSE)` **during teardown** — freezing the UI for up to ~2.5s and
   re-entrantly pumping the main loop mid-destruction.
5. `MainWindow::onStartAnalysis()` (`src/main_window.cpp:559-572`, old) silently did nothing when
   `cfg.enginePath` was empty, unlike `EngineStatusView`'s own Start button which already opened
   Settings in that case.

## Fix

### State model

Replaced the bool pair with `EngineController::EngineState { NotStarted, Starting, Idle, Analyzing,
Stopping, Crashed }` (`src/engine/engine_controller.h`), and a single
`sigc::signal<void(EngineState)> signal_state_changed` (replacing both
`signal_engine_state(bool)`/`signal_analyzing_state(bool)`). A private `setState()` helper only emits
on an actual value change, satisfying "emit only on real transitions."

`GameState::analyzing_` (`src/model/game_state.h:66-67`) stays the sole owner of the "is analyzing"
fact — it guards several of `GameState`'s own mutation methods (`makeMove`, `undoMove`, `gotoPath`,
etc.) and the model layer must not depend on the engine layer to know its own guard state.
`EngineController::isAnalyzing()` now reads through `gameState_.isAnalyzing()` instead of keeping a
second, independently-updated copy — that second copy was the literal duplicate the instruction file
flagged.

### Honest start/crash/thinking states

- `startEngine()` (`src/engine/engine_controller.cpp`) now checks `EngineProcess::start()`'s return
  value: on failure it emits `signal_engine_output("Error", "Failed to start engine: " + path)`
  (routed to the existing bottom-panel console, same path as all other engine messages) and stays at
  `NotStarted` — it never sets `Idle` or emits a "running" transition.
- `signal_process_died`'s handler only transitions `NotStarted`/`Crashed` → nothing (already
  correctly "not a fresh event"); anything else → `Crashed`, a state rendered distinctly from
  `NotStarted`.
- `EngineStatusView::setEngineState(EngineController::EngineState)` (`src/ui/engine_status.cpp`)
  renders all six states with distinct text + CSS class: OFF / STARTING / ON / THINKING / STOPPING /
  CRASHED (`src/resources/style.css` gained matching `.engine-starting`/`.engine-thinking`/
  `.engine-stopping`/`.engine-crashed` rules).
- Crash is an **active announcement**, not just a label flip: checked `CMakeLists.txt` — no
  `Adw::`/libadwaita is linked in this build — so `EngineStatusView` gained `signal_crashed`, and
  `AnalysisPanel` (`src/ui/analysis_panel.h`/`.cpp`) gained an inline dismissible `Gtk::Revealer`
  crash banner (`showEngineCrashBanner()`/`hideEngineCrashBanner()`) wired to it, instead of an
  `Adw::Toast`.

### Non-blocking stop

- `EngineProcess::stop()` (`src/engine/engine_process.cpp`) is now a synchronous, *immediate*
  force-kill: cancel pending reads, force-exit if not already exited, reset. No `g_usleep`, no
  `g_main_context_iteration`. This is the path used by both destructors
  (`~EngineController`/`~EngineProcess`) — correct with no main loop guaranteed to be running.
- New `EngineProcess::stopAsync(std::function<void()> onComplete)`: cancels pending reads
  immediately (same ordering as `stop()`, so the `running_`-flag de-dup guard in
  `readStdout()`/`readStderr()` still suppresses a spurious `signal_process_died` from the
  now-cancelled reads), then races `Gio::Subprocess::wait_async()` against a 2000ms
  `Glib::signal_timeout()` grace period (whichever finishes first wins via a shared "already
  completed" guard; the timeout force-exits before completing). `onComplete` runs exactly once, only
  through the live main loop — never inline, never in a busy-wait.
- `EngineController::stopEngine(std::function<void()> onComplete = nullptr)`: sends
  `YXSAVEDATABASE`/`END` if running, transitions to `Stopping`, then calls `stopAsync()`. Calling it
  again while already `Stopping` chains the new caller's `onComplete` onto the in-flight shutdown
  instead of firing early (firing early would let a caller like `onQuit()` destroy `this` before the
  original stop actually finished). Calling it while `NotStarted` is a synchronous no-op — no signal
  emitted, `onComplete` (if given) still runs, satisfying "only emit on real transitions."
- `EngineController::reloadEngine()` now sequences `stopEngine()` → (on completion) `startEngine()` →
  `sendConfig()` if `Idle`, instead of three synchronous calls in a row.

### Lifetime safety (not explicitly requested, but required for correctness)

Removing the busy-wait loop from `EngineProcess::stop()` removed its incidental side effect of
flushing any just-cancelled `read_line_async`'s callback (with `this` still valid) before the
destructor returned. Both `EngineController` and `EngineProcess` now hold a
`std::shared_ptr<void>` presence marker (`lifetimeGuard_`/`selfGuard_`); async completion callbacks
(`stopEngine()`'s `stopAsync` callback, `readStdout()`/`readStderr()`'s read callbacks) capture only
a `std::weak_ptr` to it and check `.expired()` before touching `this`, so a callback arriving after
destruction is a safe no-op instead of a dangling-pointer access.

### Two call sites that needed to change mechanically

- `MainWindow::onQuit()` (`src/main_window.cpp`): was `stopEngine(); close();` — now
  `stopEngine([this]() { close(); });`, since closing immediately would destroy
  `EngineController`/`EngineProcess` while the async stop might still be in flight.
- `MainWindow::onSettings()`'s path-change branch (`src/main_window.cpp`): was
  `stopEngine(); startEngine(); sendConfig();` back-to-back — with `stopEngine()` now async,
  `startEngine()` would see state `Stopping` (not `NotStarted`) and silently no-op. Replaced with a
  single `reloadEngine()` call, which already sequences this correctly via a completion callback.

### Silent no-op on empty engine path

`MainWindow::onStartAnalysis()` (`src/main_window.cpp`) now calls `onSettings()` when
`cfg.enginePath` is empty, reusing the same fallback `EngineStatusView`'s own Start button already
used (`analysisPanel_.engineStatus().signal_start`'s handler).

## Verification

- **Build:** `RUN_TESTS=1 bash build.sh` (clean, from a removed `build_cmd`) — both `rapfi-gui` and
  `rapfi-gui-tests` build with no new warnings.
- **Tests:** `ctest` / direct run of `build_cmd/tests/rapfi-gui-tests` — **52/52 test cases pass,
  209/209 assertions pass** (48 pre-existing + 4 new in `tests/test_eng01_engine_state.cpp`):
  - bad-path `startEngine()` never reaches a running state and never emits `Idle`.
  - `stopEngine()` on a never-started controller is a synchronous no-op: zero signal emissions.
  - a real short-lived process (`/bin/true`) produces exactly one `Crashed` transition through
    `EngineController` — not two (one per stdout/stderr EOF).
  - a direct `EngineProcess`-level test confirms the same stdout+stderr EOF de-dup guard
    (`running_`) still fires `signal_process_died` exactly once.
  - Added `engine_process.cpp`/`engine_controller.cpp` + `giomm-2.68` to `tests/CMakeLists.txt` —
    confirmed this does not violate the "no gtkmm" invariant that target enforces by construction
    (giomm/glibmm need no display server, same as the existing sigc++-3.0 link).
  - Had to switch `tests/test_main.cpp` from `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` to a custom
    `main()` calling `Gio::init()` first — without it, every `GUnixInputStream` wrap (behind
    `Gio::Subprocess`'s pipes) failed with `GLib-GIO-CRITICAL` and async reads never completed, since
    production code only gets this init for free via `Gtk::Application::create()`.
- **Source-level, per task's step 4/5 (grep is not sufficient on its own, confirmed by reading):**
  `grep -rn "g_usleep(\|g_main_context_iteration(" src` returns no real calls (comment mentions
  only). `~EngineController()` calls `engine_.stop()` (sync); `~EngineProcess()` calls `stop()`
  (sync) — neither depends on `stopAsync()`'s callback ever firing.

## Deviations from `docs/instruction/ENG-01-engine-state-honesty-and-blocking-stop.md`

- **Toast vs. banner (anticipated by the instruction file):** no libadwaita linked → inline
  `Gtk::Revealer` banner, as the instruction file's own fallback.
- **`onSettings()` → `reloadEngine()` (mechanical consequence, not scope creep):** the old
  stop-then-start call pair only worked because stop was synchronous; making stop async per this
  task's core requirement forces this call site to change too.
- **Lifetime guards (required for correctness, not requested):** removing the busy-wait loop (as
  instructed) removed its incidental memory-safety side effect; the `weak_ptr` guards restore it
  explicitly instead of leaving a latent dangling-pointer risk.

## Scope boundaries respected

- Did not touch engine-path validation inside the settings dialog (UX-02).
- Did not touch protocol parsing (`GomocupProtocol` untouched, PROTO-01/02 territory).
- `src/command/command_dispatcher.cpp`'s `!engine start/stop/reload` calls
  `startEngine()`/`stopEngine()`/`reloadEngine()` unchanged in signature/call shape — no dispatcher
  changes were needed beyond what the enum/signal rename mechanically required (none were, since it
  only calls those three methods without touching the old bool accessors or signals).
