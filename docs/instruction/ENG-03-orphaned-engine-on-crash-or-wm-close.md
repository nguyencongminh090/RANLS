# ENG-03 — execution guidance

## Approach

Two independent changes, both small; do them in order and verify each before the next.

### 1. `signal_close_request` → graceful stop

In `MainWindow::connectSignals()` add:

```cpp
signal_close_request().connect([this]() -> bool {
    if (closeInFlight_) return false;   // second X → let GTK close now
    closeInFlight_ = true;
    controller_.stopEngine([this]() { close(); });
    return true;                        // veto this close; callback re-issues it
}, false);
```

`onQuit()` already does `controller_.stopEngine([this]{ close(); })` — factor the body into one
private helper (`requestGracefulClose()`) and call it from both. `close()` inside the callback
re-triggers `signal_close_request`; the `closeInFlight_` flag makes the second pass fall through.
`stopEngine()` when `state_ == Stopping` chains the new completion onto the in-flight one
(`pendingStopComplete_`), so a menu-Quit already running + an X click is safe.

### 2. `PR_SET_PDEATHSIG` on the engine child

`EngineProcess::start()` currently does `Gio::Subprocess::create({enginePath}, flags)`. Switch to:

```cpp
auto launcher = Gio::SubprocessLauncher::create(flags);
// child-setup runs in the forked child before exec:
g_subprocess_launcher_set_child_setup(
    launcher->gobj(),
    +[](gpointer) {
        ::prctl(PR_SET_PDEATHSIG, SIGKILL);
        // race: parent may have already died between fork and here
        if (::getppid() == 1) ::_exit(1);
    },
    nullptr, nullptr);
process_ = launcher->spawn({enginePath});
```

`#include <sys/prctl.h>` and `<unistd.h>`, guard the whole block with `#ifdef __linux__` (fall back
to the current `Gio::Subprocess::create` path elsewhere). Everything downstream
(`get_stdin_pipe()` / `get_stdout_pipe()` / `wait_async` / `force_exit`) is identical on the
handle a launcher returns.

## Pitfalls

- **`gtkmm` may not wrap `set_child_setup`** — drop to the C `g_subprocess_launcher_set_child_setup`
  on `launcher->gobj()` as above. Only async-signal-safe calls in the callback (`prctl`, `getppid`,
  `_exit` are; `std::cerr` is not).
- **Do not veto close forever.** If `stopEngine()`'s callback somehow never fires, the window
  won't close. `stopAsync()` has a hard 2 s grace-timeout that always calls `onComplete`, so this
  is bounded — but do not add a second independent timeout here that could `close()` while the
  async stop is still touching `this`.
- **`~EngineController` stays synchronous.** PDEATHSIG does not replace it — a clean exit still
  runs destructors and that force-kill path is correct. PDEATHSIG only covers the no-destructor
  crash/kill case.
- **`getppid() == 1` race check**: on modern Linux with subreapers the reparent target may not be
  PID 1. This is a best-effort belt-and-braces check; the real guarantee is PDEATHSIG itself.
- ENG-02: the auto-play-interrupt → "Engine plays Off" revert must not fire on the close path —
  the close callback calls `close()`, not `onStopAnalysis()`, so it shouldn't, but add an
  ENG-02 assertion to the close-path test.

## Verification before done

- Build clean; `ctest` green.
- New test: close-request handler issues `stopEngine` and only closes on its callback (mock/observe
  `EngineController` state transitions; assert `MatchConfig` engine-plays side is untouched).
- New test: fork a harness that spawns a dummy long-lived child via `EngineProcess`, `SIGKILL` the
  harness, assert the child is reaped within ~1 s.
- Manual (needs a real engine + display): start analysis, click the WM X → confirm `pgrep -f
  <engine>` is empty; repeat with `kill -9` of the GUI PID.
- `docs/fix-log/<date>-eng-03-*.md` with the Linux-only PDEATHSIG gap noted for other platforms.

## Boundaries

- No crash-reporter / backtrace handler (separate Backlog item if wanted).
- No change to the ENG-01 enum, `stop()`/`stopAsync()` split, or `~EngineController`.
- No `setsid` / process-group change.
