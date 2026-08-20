# ENG-01 — Engine state is dishonest, and stopping the engine freezes the UI

**Status:** open
**Area:** engine lifecycle / status indicator
**Priority:** P0
**Source:** UI/UX + codebase review, 2026-08-21

## Problem

### 1. "● ON" is shown even when the engine failed to start

`EngineProcess::start` returns `bool` and returns `false` on failure
(`src/engine/engine_process.cpp:39-42`). `EngineController::startEngine` ignores it
(`src/engine/engine_controller.cpp:76-78`):

```cpp
engine_.start(cfg.enginePath);
started_ = true;
signal_engine_state.emit(true);
```

So a bad engine path produces a green **● ON** indicator (`src/ui/engine_status.cpp:127-130`) with
no process behind it. Every later command is silently swallowed by
`EngineProcess::sendLine`'s `if (!running_ || !process_) return;`
(`src/engine/engine_process.cpp:76`). The user gets a UI claiming the engine runs and an engine that
never answers.

### 2. Crashed is indistinguishable from never-started

`signal_process_died` (`src/engine/engine_controller.cpp:16-23`) emits `signal_engine_state(false)`,
which renders exactly the same **● OFF** as a fresh, never-started app
(`src/ui/engine_status.cpp:131-135`). No toast, no dialog, no distinct state. The user's first clue
is that numbers stopped moving — which the `ui-ux-review` "engine-state honesty" checklist item
names as the failure mode to avoid.

### 3. No visible "thinking" state

`signal_analyzing_state` only toggles button sensitivity (`src/main_window.cpp:405-426`). The status
label stays "● ON" whether the engine is idle or mid-search. Idle / thinking / stopped / crashed are
four states rendered as two.

### 4. Stopping the engine blocks the main loop for up to ~2.5s

- `EngineController::stopEngine` — `g_usleep(500000)` (`src/engine/engine_controller.cpp:87`)
- `EngineProcess::stop` — up to 2000ms of `g_usleep(20000)` in a loop
  (`src/engine/engine_process.cpp:58-63`)

Both run on the main thread, so the window is frozen for the duration. Worse, that loop calls
`g_main_context_iteration(NULL, FALSE)` **while tearing the object down** — a re-entrancy hazard
that can dispatch queued signals into `EngineProcess`/`EngineController` mid-destruction.

### 5. Analyze with no engine path does nothing, silently

`MainWindow::onStartAnalysis` (`src/main_window.cpp:541-554`) returns without any feedback when
`cfg.enginePath` is empty. Note `EngineStatusView`'s own start button *does* handle this correctly
by opening Settings (`src/main_window.cpp:434-442`) — the toolbar/hotkey path just doesn't.

## Acceptance criteria

- `startEngine` propagates `EngineProcess::start`'s failure: no `started_ = true`, no
  `signal_engine_state(true)`, and a visible error naming the path that failed.
- The status surface distinguishes at least: **not started / starting / idle / thinking / stopped /
  crashed**. Crash is announced actively (toast or inline banner), not just by a label flip.
- No `g_usleep` on the main thread in the engine stop path; shutdown is asynchronous, with the UI
  responsive and a busy indication while it completes.
- No `g_main_context_iteration` inside object teardown.
- Analyze with an unset engine path gives feedback — reuse the existing "open Settings" behaviour
  from `src/main_window.cpp:434-442`.

## Scope boundary

- Engine-path *validation inside the settings dialog* is UX-02.
- Do not restructure the protocol abstraction; this is lifecycle and feedback only.

## Related

- UX-02 (settings validation), and the `perf-optimization` skill's note on the blocking waits
