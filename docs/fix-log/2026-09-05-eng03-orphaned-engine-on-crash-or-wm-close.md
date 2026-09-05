# 2026-09-05 — Engine subprocess orphaned on WM-close ("X") or GUI crash (ENG-03)

## Root cause

Two exit paths bypassed the only two mechanisms that ever stopped the engine subprocess
(`MainWindow::onQuit()`'s `controller_.stopEngine()`, or `~EngineController`/`~EngineProcess`'s
synchronous force-kill):

1. **WM close button ("X").** `MainWindow::connectSignals()` never connected
   `signal_close_request`, so clicking the titlebar X ran GTK's default close and `main()` returned.
   `src/application.cpp` does `new MainWindow()` and never `delete`s it (no `on_shutdown`, not
   `Gtk::manage`d), so `~MainWindow` → `~EngineController` → `~EngineProcess` never ran — nothing
   ever sent `END` or called `force_exit()`.
2. **GUI crash** (SIGSEGV / abort / uncaught exception) or an external `kill -9` of the GUI process.
   Same result — no destructor runs — and there was no `PR_SET_PDEATHSIG` on the spawned child, so
   the kernel does not kill or reap the engine when the parent dies; it is simply reparented (to
   init, or to whatever subreaper is active).

In both cases the engine subprocess's termination relied entirely on it noticing EOF on its own
stdin once the GUI's file descriptors closed. Compliant engines exit on that EOF, so a stray
process was rare in practice, but an engine that only polls stdin between search iterations
(mid-search) would linger until that iteration finished, and a non-compliant engine would leak
outright.

## Fix

Two independent changes, per `docs/instruction/ENG-03-orphaned-engine-on-crash-or-wm-close.md`:

1. **`signal_close_request` → the same graceful shutdown as menu-Quit.** Factored `onQuit()`'s body
   into a new private `MainWindow::requestGracefulClose()`, called from both `onQuit()` and a new
   `signal_close_request` handler wired at the end of `connectSignals()`. The handler vetoes the
   first close request (`return true`), sets a new `closeInFlight_` bool, and calls
   `requestGracefulClose()` → `controller_.stopEngine([this]{ close(); })`. The completion callback's
   `close()` re-triggers `signal_close_request`; `closeInFlight_` makes that second pass fall through
   (`return false`) to GTK's real close. A second close request arriving while a stop is already in
   flight (double-click X, or X during an in-flight menu-Quit) is a no-op by construction —
   `EngineController::stopEngine()` already chains completions onto the in-flight shutdown when
   `state_ == Stopping`, so no second independent timeout was needed (and the instruction file
   explicitly warned against adding one, to avoid a `close()` racing the async stop while it still
   touches `this`).
2. **`PR_SET_PDEATHSIG(SIGKILL)` on the spawned engine (Linux).** `EngineProcess::start()` now
   builds a `Gio::SubprocessLauncher` and calls the C API
   `g_subprocess_launcher_set_child_setup()` (gtkmm does not wrap it) with a plain, non-capturing
   function that runs in the forked child before `exec()`: `prctl(PR_SET_PDEATHSIG, SIGKILL)`, plus
   a best-effort `getppid() == 1` race guard for the rare case where the parent already died between
   `fork()` and this callback. Only async-signal-safe calls (`prctl`, `getppid`, `_exit`) appear in
   that callback. Guarded by `#ifdef __linux__`; falls back to the previous
   `Gio::Subprocess::create()` path on other platforms. Every downstream accessor
   (`get_stdin_pipe()`/`get_stdout_pipe()`/`wait_async()`/`force_exit()`) works identically on the
   `Glib::RefPtr<Gio::Subprocess>` a launcher's `spawn()` returns.

`~EngineController`/`~EngineProcess`'s synchronous force-kill `stop()` path is unchanged —
PDEATHSIG supplements it for the no-destructor crash/kill case, it does not replace it.

## Files changed

- `src/main_window.h` — new `requestGracefulClose()` declaration, `closeInFlight_` member, new
  `friend struct RanlsEng03Probe` (test seam, same pattern as `RanlsAnlz05Probe`/`RanlsAnlz07Probe`).
- `src/main_window.cpp` — `onQuit()` now delegates to `requestGracefulClose()`; new
  `signal_close_request` handler wired at the end of `connectSignals()`.
- `src/engine/engine_process.h` — new `pid()` accessor (OS PID of the spawned engine as a string),
  test-only, no production call site.
- `src/engine/engine_process.cpp` — `EngineProcess::start()` refactored to
  `Gio::SubprocessLauncher` + `g_subprocess_launcher_set_child_setup()` on Linux; unchanged
  `Gio::Subprocess::create()` fallback elsewhere.
- `tests/test_eng03_close_request.cpp` (new) + `tests/CMakeLists.txt` registration
  (`ranls-gui-ui-tests`, links gtkmm) — widget-level test against a real `MainWindow`: drives the
  actual `signal_close_request` handler via `g_signal_emit_by_name` (no live WM needed), asserts the
  first close request vetoes the close and drives `EngineController` through real
  `Idle → Stopping → NotStarted` transitions, a second concurrent close request is inert, and
  `MatchConfig::enginePlays` is untouched throughout (ENG-02 non-regression).
- `tests/test_eng03_pdeathsig.cpp` (new) + `tests/CMakeLists.txt` registration (`ranls-gui-tests`,
  no gtkmm) — forks a harness process that spawns a long-lived shell script (deliberately ignoring
  stdin, so only PDEATHSIG or an explicit kill can end it) via a real `EngineProcess`, `SIGKILL`s
  the harness to simulate a crash with zero chance for any destructor to run, and polls
  `/proc/<pid>` for the engine child to disappear or become a zombie within ~1.5s.

## Verification

- `./build.sh build_cmd` — clean build, no new warnings.
- `ctest --test-dir build_cmd --output-on-failure`:
  - `ranls-gui-tests` — **passed**, 195 assertions (includes the new PDEATHSIG test), 0 failed.
  - `rel02-version-single-source` — **passed**.
  - `ranls-gui-ui-tests` — 25/26 test cases passed (174/175 assertions). The one failure,
    `test_anlz05_no_automove_action.cpp:135`, reproduces identically on unmodified `main`
    (confirmed via `git stash` + rebuild + rerun before making any ENG-03 change) — a pre-existing,
    unrelated flake, not a regression introduced here. The new `ENG-03: WM close request routes
    through stopEngine and only closes on completion` test case in this same binary passed cleanly
    (10/10 assertions).
- PDEATHSIG test cross-checked for a false positive: temporarily commented out the `prctl()` call
  in `engineChildSetup()`, rebuilt, reran `test_eng03_pdeathsig` — it failed (`CHECK(gone)` false,
  engine child still alive after 1.5s) — then restored the `prctl()` call and confirmed the test
  passes again. This confirms the test is actually exercising PDEATHSIG, not some other reaping
  mechanism (e.g. an aggressive subreaper).
- **Not run**: the live-engine/display manual smoke listed in the instruction file (click the WM X
  during real analysis against a real engine binary + display; repeat with `kill -9` of the real GUI
  PID; confirm via `pgrep`). No real engine binary or display server was available in this
  environment — this tier is explicitly unverified, not claimed as passing.

## Known limitation

Windows/macOS have no `PR_SET_PDEATHSIG` equivalent wired up — out of scope per the instruction
file (`docs/instruction/ENG-03-orphaned-engine-on-crash-or-wm-close.md`'s Boundaries: "Windows/macOS
parity for the PDEATHSIG equivalent is out of scope ... the project targets Linux/GTK4"). On those
platforms a GUI crash still relies solely on the engine noticing stdin EOF, same as before this fix.

## Left out of scope

- No SIGSEGV backtrace handler / crash reporter (explicitly out of scope per the instruction file —
  a separate Backlog item if wanted).
- No `setsid` / engine process-group change.
- The ENG-01 state enum and `stop()`/`stopAsync()` split are unchanged — only the close-request
  entry point and the child-setup callback were added.
