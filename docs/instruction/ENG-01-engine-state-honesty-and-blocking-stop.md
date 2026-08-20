# ENG-01 — execution guidance

## Approach

This item bundles five symptoms with one root cause: **engine state is a `bool` and reality has more
than two states.** Fix the model first, then the display follows.

Replace the `started_` / `analyzing_` bool pair in `EngineController`
(`src/engine/engine_controller.h`) with an explicit state enum — roughly
`NotStarted / Starting / Idle / Analyzing / Stopping / Crashed` — and emit transitions. `GameState`
already mirrors `analyzing_` separately (`src/model/game_state.h:101`), which is a second copy of
the same fact; decide which owns it and remove the other.

Then:

- `startEngine` (`src/engine/engine_controller.cpp:69-79`) must honour `EngineProcess::start`'s
  return value. It currently discards it — that single ignored `bool` is the "● ON with no process"
  bug.
- `EngineStatusView::setEngineState(bool)` (`src/ui/engine_status.cpp:125`) takes the enum instead,
  and renders crashed distinctly from stopped.
- Crash needs an active announcement, not a label flip. Check whether libadwaita is linked
  (`CMakeLists.txt`) — `Adw::Toast` is the right tool if so; an inline banner in the analysis panel
  otherwise.

## Non-blocking shutdown

The two blocking waits are `g_usleep(500000)` (`src/engine/engine_controller.cpp:87`) and the
2000ms loop in `EngineProcess::stop` (`src/engine/engine_process.cpp:58-63`).

Restructure to: send `YXSAVEDATABASE`/`END`, enter `Stopping`, arm a timeout, and complete
asynchronously — either when the process exits (`Gio::Subprocess::wait_async`) or when the timeout
fires and forces `force_exit()`. The UI stays live and shows a busy state throughout.

## Pitfalls

- **`g_main_context_iteration` inside `stop()` is a re-entrancy trap.** It can dispatch queued
  signals into `EngineProcess`/`EngineController` while they are being torn down. Removing it is part
  of the fix, not optional — do not just shorten the sleep.
- `~EngineController` calls `stopEngine()` (`src/engine/engine_controller.cpp:28`) and
  `~EngineProcess` calls `stop()` (`src/engine/engine_process.cpp:9`). An async shutdown must still
  behave correctly during destruction — there is no main loop to run callbacks on by then. Keep a
  synchronous force-kill path for the destructor specifically, and use the async path for
  user-initiated stops.
- `MainWindow::onQuit` (`src/main_window.cpp:468-472`) calls `stopEngine()` then `close()` — with
  async shutdown this needs to wait for completion or accept the force-kill.
- `stopEngine` emits `signal_engine_state(false)` and `signal_analyzing_state(false)`
  unconditionally, even when never started (`:92-93`). With a state enum, emit only on real
  transitions or handlers will see spurious events.
- Both stdout and stderr EOF can fire `signal_process_died` (`src/engine/engine_process.cpp:114`,
  `:149`). The `running_` flag currently de-duplicates this correctly — preserve that guard when
  restructuring.

## Do not touch

- Engine-path validation inside the settings dialog — UX-02.
- Protocol parsing — PROTO-01/02.
- The command dispatcher's engine commands (`src/command/command_dispatcher.cpp`) beyond what the
  state-enum change forces; if `!engine start`/`!engine stop` need updating, keep it mechanical.
