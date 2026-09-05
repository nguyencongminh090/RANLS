#include "engine_process.h"

#include <iostream>
#include <memory>

#ifdef __linux__
#include <sys/prctl.h>
#include <unistd.h>
#endif

EngineProcess::EngineProcess() = default;

EngineProcess::~EngineProcess()
{
    stop();
}

#ifdef __linux__
namespace {
// ENG-03: runs in the forked child, between fork() and exec(), before any
// engine code executes. Async-signal-safe only — prctl/getppid/_exit are;
// std::cerr, logging, and anything allocating are NOT, and must never be
// added here. Sets PR_SET_PDEATHSIG so the kernel SIGKILLs this child the
// moment the GUI process dies for any reason (crash, kill -9, WM-forced
// termination) with no destructor / EOF-on-stdin round trip required. The
// getppid() check is a best-effort race guard for the (rare) case where the
// parent already died between fork() and this callback running — on most
// systems the child gets reparented to PID 1, but under a subreaper the
// reparent target can differ (see docs/instruction/ENG-03-...), so this is
// belt-and-braces on top of PDEATHSIG, not the primary guarantee.
void engineChildSetup(gpointer)
{
    ::prctl(PR_SET_PDEATHSIG, SIGKILL);
    if (::getppid() == 1) ::_exit(1);
}
} // namespace
#endif

bool EngineProcess::start(const std::string &enginePath)
{
    if (running_) stop();

    try {
        auto flags = Gio::Subprocess::Flags::STDIN_PIPE
                   | Gio::Subprocess::Flags::STDOUT_PIPE
                   | Gio::Subprocess::Flags::STDERR_PIPE;

#ifdef __linux__
        // ENG-03: gtkmm does not wrap g_subprocess_launcher_set_child_setup,
        // so drop to the C API on the launcher's underlying GObject. Every
        // downstream accessor (get_stdin_pipe/get_stdout_pipe/wait_async/
        // force_exit) works identically on the Glib::RefPtr<Gio::Subprocess>
        // a launcher's spawn() returns as on one from Subprocess::create().
        auto launcher = Gio::SubprocessLauncher::create(flags);
        g_subprocess_launcher_set_child_setup(
            launcher->gobj(), &engineChildSetup, nullptr, nullptr);
        process_ = launcher->spawn({enginePath});
#else
        // Windows/macOS: no PDEATHSIG equivalent wired up here (known gap,
        // see docs/fix-log — this task targets Linux/GTK4 per project scope).
        process_ = Gio::Subprocess::create({enginePath}, flags);
#endif

        // Wrap the output streams for line-based reading.
        auto stdoutBase = process_->get_stdout_pipe();
        auto stderrBase = process_->get_stderr_pipe();

        stdoutStream_ = Gio::DataInputStream::create(stdoutBase);
        stderrStream_ = Gio::DataInputStream::create(stderrBase);
        cancellable_  = Gio::Cancellable::create();

        running_ = true;

        // Start async read loops.
        readStdout();
        readStderr();

        return true;
    }
    catch (const Glib::Error &e) {
        std::cerr << "[EngineProcess] Failed to start: " << e.what() << "\n";
        return false;
    }
}

void EngineProcess::stop()
{
    // Synchronous, immediate force-kill: no g_usleep, no g_main_context_iteration.
    // This is the only path safe to call from a destructor, where no main loop
    // is guaranteed to be running to pump async callbacks. It does not give the
    // engine any grace period to flush — callers on a live main loop that want
    // a graceful shutdown should use stopAsync() instead.
    running_ = false;

    // Cancel all pending async reads FIRST to prevent callbacks from firing.
    if (cancellable_) {
        cancellable_->cancel();
        cancellable_.reset();
    }

    if (process_) {
        if (!process_->get_if_exited()) {
            process_->force_exit();
        }
        process_.reset();
    }
    stdoutStream_.reset();
    stderrStream_.reset();
}

void EngineProcess::stopAsync(std::function<void()> onComplete)
{
    running_ = false;

    // Cancel all pending async reads FIRST to prevent callbacks from firing
    // (same ordering as stop(): running_ is already false by the time any
    // cancelled read's error handler runs, so the running_-flag de-dup guard
    // in readStdout()/readStderr() suppresses a spurious signal_process_died).
    if (cancellable_) {
        cancellable_->cancel();
        cancellable_.reset();
    }
    stdoutStream_.reset();
    stderrStream_.reset();

    if (!process_) {
        if (onComplete) onComplete();
        return;
    }

    // Keep the Gio::Subprocess handle alive locally for the duration of the
    // async wait/timeout race, independent of this EngineProcess instance.
    auto proc = process_;
    process_.reset();

    // Shared completion guard: whichever of {process exits naturally, grace
    // period elapses} happens first wins; the other is a no-op. This is the
    // non-blocking replacement for the old g_usleep/g_main_context_iteration
    // polling loop — no thread is ever put to sleep and no main-loop
    // re-entrancy is triggered.
    struct StopRace {
        bool completed = false;
        sigc::connection timeoutConn;
    };
    auto race = std::make_shared<StopRace>();

    auto finish = [race, onComplete]() {
        if (race->completed) return;
        race->completed = true;
        race->timeoutConn.disconnect();
        if (onComplete) onComplete();
    };

    // Grace period: force-kill if the engine hasn't exited on its own in time.
    race->timeoutConn = Glib::signal_timeout().connect(
        [proc, finish]() mutable {
            if (!proc->get_if_exited()) {
                proc->force_exit();
            }
            finish();
            return false; // one-shot
        },
        2000);

    proc->wait_async([proc, finish](Glib::RefPtr<Gio::AsyncResult> &result) mutable {
        try {
            proc->wait_finish(result);
        } catch (const Glib::Error &) {
            // Ignore — we only care that the process has exited.
        }
        finish();
    });
}

void EngineProcess::sendLine(const std::string &command)
{
    if (!running_ || !process_) return;

    auto stdinStream = process_->get_stdin_pipe();
    if (stdinStream) {
        gsize written = 0;
        try {
            std::string msg = command + "\n";
            stdinStream->write_all(msg, written);
            stdinStream->flush();
            signal_line_sent.emit(command);
        }
        catch (const Glib::Error &e) {
            std::cerr << "[EngineProcess] Write error: " << e.what() << "\n";
        }
    }
}

void EngineProcess::readStdout()
{
    if (!running_ || !stdoutStream_ || !cancellable_) return;

    auto stream = stdoutStream_;
    std::weak_ptr<void> guard = selfGuard_;
    stream->read_line_async(
        [this, stream, guard](Glib::RefPtr<Gio::AsyncResult> &result) {
            if (guard.expired()) return; // EngineProcess was destroyed meanwhile.
            try {
                std::string line;
                bool success = stream->read_line_finish(result, line);

                if (stream == stdoutStream_) {
                    if (success && !line.empty()) {
                        signal_line_received.emit(line);
                    }
                    
                    if (success && running_) {
                        readStdout();
                    } else if (!success) { // EOF
                        bool wasRunning = running_;
                        running_ = false;
                        if (wasRunning) signal_process_died.emit();
                    }
                }
            }catch (const Glib::Error &) {
                if (stream == stdoutStream_) {
                    bool wasRunning = running_;
                    running_ = false;
                    if (wasRunning) signal_process_died.emit();
                }
            }
        },
        cancellable_);
}

void EngineProcess::readStderr()
{
    if (!running_ || !stderrStream_ || !cancellable_) return;

    auto stream = stderrStream_;
    std::weak_ptr<void> guard = selfGuard_;
    stream->read_line_async(
        [this, stream, guard](Glib::RefPtr<Gio::AsyncResult> &result) {
            if (guard.expired()) return; // EngineProcess was destroyed meanwhile.
            try {
                std::string line;
                bool success = stream->read_line_finish(result, line);

                if (stream == stderrStream_) {
                    if (success && !line.empty()) {
                        signal_error_received.emit(line);
                    }
                    
                    if (success && running_) {
                        readStderr();
                    } else if (!success) { // EOF
                        bool wasRunning = running_;
                        running_ = false;
                        if (wasRunning) signal_process_died.emit();
                    }
                }
            }
            catch (const Glib::Error &) {
                if (stream == stderrStream_) {
                    bool wasRunning = running_;
                    running_ = false;
                    if (wasRunning) signal_process_died.emit();
                }
            }
        },
        cancellable_);
}
