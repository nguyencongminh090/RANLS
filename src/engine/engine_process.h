#pragma once

#include <giomm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>
#include <functional>
#include <memory>
#include <string>

/// Manages the engine subprocess and I/O.
/// Uses asynchronous I/O to avoid blocking the GTK main loop.
class EngineProcess {
public:
    EngineProcess();
    ~EngineProcess();

    /// Spawn the engine process. Returns false on failure.
    bool start(const std::string &enginePath);

    /// Immediately force-kill the process with no waiting, no g_usleep, and no
    /// g_main_context_iteration pumping. Safe to call with no main loop
    /// available (e.g. from a destructor). Used for teardown, not for the
    /// user-initiated "Stop" path — see stopAsync() for that.
    void stop();

    /// Ask the process to exit and complete asynchronously: cancels pending
    /// reads immediately, then waits for the process to exit naturally (via
    /// Gio::Subprocess::wait_async) up to a grace period, force-killing it if
    /// the grace period elapses first. `onComplete` runs once, exactly when
    /// teardown has actually finished (never blocks the caller). Requires a
    /// running GLib main loop to be pumped for `onComplete` to ever fire —
    /// do not use this from a destructor.
    void stopAsync(std::function<void()> onComplete);

    /// Send a raw command string to the engine's stdin (appends newline).
    void sendLine(const std::string &command);

    /// Is the engine running?
    bool isRunning() const { return running_; }

    /// The OS process ID of the spawned engine, or empty if not running.
    /// ENG-03: exposed for the PDEATHSIG regression test (confirming the
    /// real child PID is gone after the parent is killed) — not used by
    /// any production code path.
    std::string pid() const { return process_ ? std::string(process_->get_identifier()) : std::string(); }

    /// Signal emitted when a line is received from the engine's stdout.
    sigc::signal<void(const std::string&)> signal_line_received;

    /// Signal emitted when a line is received from stderr.
    sigc::signal<void(const std::string&)> signal_error_received;

    /// Signal emitted when a line is SENT to the engine's stdin.
    sigc::signal<void(const std::string&)> signal_line_sent;

    /// Signal emitted when the process has died.
    sigc::signal<void()> signal_process_died;

private:
    void readStdout();
    void readStderr();

    bool running_ = false;

    Glib::RefPtr<Gio::Subprocess>       process_;
    Glib::RefPtr<Gio::DataInputStream>  stdoutStream_;
    Glib::RefPtr<Gio::DataInputStream>  stderrStream_;
    Glib::RefPtr<Gio::Cancellable>      cancellable_;

    // The old stop() busy-waited on g_main_context_iteration for up to 2s,
    // which (incidentally) guaranteed any just-cancelled read_line_async's
    // callback ran — with `this` still valid — before the destructor
    // returned. The new stop() is immediate and does not pump the loop, so a
    // cancelled read's callback may now run after this object is destroyed
    // (GIO's underlying GTask outlives us). readStdout()/readStderr()'s
    // lambdas capture only a weak_ptr to this guard and check `.expired()`
    // before touching `this`.
    std::shared_ptr<void> selfGuard_ = std::make_shared<int>(0);
};
