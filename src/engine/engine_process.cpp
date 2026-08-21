#include "engine_process.h"

#include <iostream>
#include <memory>

EngineProcess::EngineProcess() = default;

EngineProcess::~EngineProcess()
{
    stop();
}

bool EngineProcess::start(const std::string &enginePath)
{
    if (running_) stop();

    try {
        auto flags = Gio::Subprocess::Flags::STDIN_PIPE
                   | Gio::Subprocess::Flags::STDOUT_PIPE
                   | Gio::Subprocess::Flags::STDERR_PIPE;

        process_ = Gio::Subprocess::create({enginePath}, flags);

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
