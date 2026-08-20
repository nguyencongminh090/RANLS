#pragma once

#include <giomm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>
#include <string>

/// Manages the engine subprocess and I/O.
/// Uses asynchronous I/O to avoid blocking the GTK main loop.
class EngineProcess {
public:
    EngineProcess();
    ~EngineProcess();

    /// Spawn the engine process. Returns false on failure.
    bool start(const std::string &enginePath);

    /// Kill the engine process.
    void stop();

    /// Send a raw command string to the engine's stdin (appends newline).
    void sendLine(const std::string &command);

    /// Is the engine running?
    bool isRunning() const { return running_; }

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
};
