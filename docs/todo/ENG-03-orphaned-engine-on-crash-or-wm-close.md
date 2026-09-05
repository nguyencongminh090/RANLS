# ENG-03 — Engine subprocess can be orphaned on WM-close and on GUI crash

**Status:** ✅ FIXED — 2026-09-05. `signal_close_request` now routes through the same
`requestGracefulClose()` helper as menu-Quit (vetoes the first close, calls
`controller_.stopEngine()`, `close()` again from the completion callback; `closeInFlight_` guards
re-entrancy). `EngineProcess::start()` now spawns via `Gio::SubprocessLauncher` with a
`g_subprocess_launcher_set_child_setup` callback that arms `PR_SET_PDEATHSIG(SIGKILL)` on Linux
(`#ifdef __linux__`, falls back to the previous `Gio::Subprocess::create` path elsewhere).
Verification: clean build (`./build.sh build_cmd`); `ctest` — `ranls-gui-tests` fully green
(195 assertions, includes the new PDEATHSIG test) and `rel02-version-single-source` green;
`ranls-gui-ui-tests` 25/26 test cases pass (174/175 assertions) — the one failure
(`test_anlz05_no_automove_action.cpp:135`) reproduces identically on unmodified `main` (confirmed
via `git stash`), so it is a pre-existing, unrelated flake, not a regression from this change. New
tests: `tests/test_eng03_close_request.cpp` (widget-level, drives the real close-request handler via
`g_signal_emit_by_name`, asserts real `EngineController` Stopping→NotStarted transitions and that
`MatchConfig::enginePlays` is untouched — ENG-02 non-regression) and
`tests/test_eng03_pdeathsig.cpp` (forks a harness that spawns a stdin-ignoring long-lived script via
a real `EngineProcess`, SIGKILLs the harness, asserts the child is gone/zombie within ~1.5s;
cross-checked by temporarily disabling the `prctl()` call and confirming the test then fails).
Manual/live-engine smoke tier (WM "X" during real analysis + `kill -9` of the GUI PID against a real
engine + display) was **not run** — no real engine binary/display available in this environment; see
fix-log detail for the explicit skip note. Known gap: Windows/macOS have no PDEATHSIG equivalent
wired up (documented limitation, out of scope per the instruction file).
**Area:** `src/engine/engine_process.{h,cpp}`, `src/main_window.cpp` (`connectSignals`, `onQuit`), possibly `src/application.cpp`
**Priority:** P2
**Source:** User safety question 2026-09-04 ("if the program crashes / the user closes normally or while analyzing, does the engine subprocess terminate correctly?") + code-base trace of the engine lifecycle.
**Design:** none — scoped directly
**Depends on / relates to:** builds on ENG-01 (async/blocking-stop split, `stop()` vs `stopAsync()`); must not regress ENG-02.

## Problem

The engine subprocess is only *guaranteed* killed when a C++ destructor runs
(`~EngineController` → `EngineProcess::stop()` → `force_exit()`) or when the user takes the
explicit **Quit** action (`MainWindow::onQuit` → `controller_.stopEngine(...)` → graceful `END`,
2 s grace, then `force_exit`). Two common exit paths hit neither:

1. **Window-manager close button ("X").** `signal_close_request` is not connected anywhere in
   `MainWindow::connectSignals()`, so the titlebar X runs GTK's default close, `run()` returns,
   and `main()` exits. The window is `new MainWindow()` in `src/application.cpp` and is **never
   `delete`d** (no `on_shutdown`, not `Gtk::manage`d), so `~MainWindow` → `~EngineController` →
   `~EngineProcess` never run. Nothing sends `END` or `force_exit()`.

2. **GUI crash** (SIGSEGV / abort / uncaught exception). No signal handler, no
   `std::set_terminate`, no `atexit` — destructors are skipped, same as (1).

In both cases termination relies entirely on the engine noticing its **stdin hits EOF** when the
GUI's fds close. Compliant Gomocup/Yixin engines (Rapfi included) do exit on stdin EOF, so in
practice a stray process is rare — but there is no safety net:

- No `PR_SET_PDEATHSIG` on the child and no subprocess-reaper, so the OS does not kill or reap the
  engine when the parent dies; it is reparented to init.
- An engine **mid-search that only polls stdin between iterations** keeps all threads + hash busy
  until the current search finishes, *then* reads EOF and exits — a lingering heavyweight process,
  not an instant one. Closing "while analyzing" via the X button is exactly this case.
- A non-compliant engine that never checks stdin leaks entirely.
- `Gio::Subprocess::create` does not `setsid`, but that does not help on a GUI crash.

The **Quit menu / quit hotkey** path is correct and must stay correct (including while analyzing).

## Scope (in order)

1. **Wire `signal_close_request` in `MainWindow::connectSignals()`** to route through the same
   graceful shutdown as `onQuit()`: on first close request, kick off `controller_.stopEngine([this]{ ... })`,
   return `true` to veto the immediate close, and let the completion callback do the real close.
   Guard against re-entrancy (user clicks X twice / X during a menu-Quit already in flight) —
   `stopEngine()` already chains completions when `state_ == Stopping`, lean on that.
2. **Add `PR_SET_PDEATHSIG(SIGKILL)` to the spawned engine** (Linux) so a GUI crash or `kill -9`
   guarantees the engine dies immediately. `Gio::Subprocess` needs a `GSubprocessLauncher` with a
   child-setup callback (`g_subprocess_launcher_set_child_setup`) — `prctl(PR_SET_PDEATHSIG, SIGKILL)`
   in the child, plus the standard race check (`getppid()` != original parent → exit now).
   Refactor `EngineProcess::start()` from `Gio::Subprocess::create({path}, flags)` to a launcher.
3. Regression coverage — see Acceptance.

## Acceptance criteria

- Closing the window with the WM close button (**including while an analysis is running**) sends
  the engine `END` and force-kills it after the grace period, identical to the Quit menu — verified
  by a test on the close-request handler path and, where a live engine is available, a manual smoke
  that checks no `pgrep` match survives.
- After the GUI process is killed with `SIGKILL` (simulating a crash), the engine process is gone
  within ~1 s (PDEATHSIG), verified by an integration-style test that spawns a trivial child
  through `EngineProcess` and `kill -9`s a forked harness.
- The Quit menu / quit hotkey path and ENG-02 (auto-play interrupt → revert to Off) are unchanged.
- `~EngineController` / `~EngineProcess` synchronous `stop()` force-kill path is unchanged.

## Scope boundary

- Do not change the ENG-01 state enum or the `stop()` / `stopAsync()` split — only add the
  close-request entry point and the child-setup.
- Do not add a `SIGSEGV` backtrace handler / crash reporter — out of scope; PDEATHSIG is the
  safety net this task delivers. A crash-reporter can be a separate Backlog item.
- Do not `setsid` / change the engine's process group.
- Windows/macOS parity for the PDEATHSIG equivalent is out of scope (note it in the fix-log as a
  known gap); the project targets Linux/GTK4.
