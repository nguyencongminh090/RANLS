#include "engine_process.h"

#include <iostream>

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
    running_ = false;

    // Cancel all pending async reads FIRST to prevent callbacks from firing.
    if (cancellable_) {
        cancellable_->cancel();
        cancellable_.reset();
    }

    if (process_) {
        // Try to let it exit naturally if it's already received a quit command.
        // We wait a bit longer for large databases to flush.
        int timeout = 2000; // 2 seconds
        while (timeout > 0 && !process_->get_if_exited()) {
            g_main_context_iteration(NULL, FALSE);
            g_usleep(20000); // 20ms
            timeout -= 20;
        }

        if (!process_->get_if_exited()) {
            process_->force_exit();
        }
        process_.reset();
    }
    stdoutStream_.reset();
    stderrStream_.reset();
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
    stream->read_line_async(
        [this, stream](Glib::RefPtr<Gio::AsyncResult> &result) {
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
    stream->read_line_async(
        [this, stream](Glib::RefPtr<Gio::AsyncResult> &result) {
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
