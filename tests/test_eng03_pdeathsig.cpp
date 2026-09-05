// Regression test for ENG-03 — PR_SET_PDEATHSIG on the spawned engine child.
//
// Covers docs/todo/ENG-03-orphaned-engine-on-crash-or-wm-close.md's second
// acceptance criterion: after the GUI process dies with no chance to run any
// destructor (simulating a crash or `kill -9`), the engine subprocess is
// gone within ~1s, because the kernel itself delivers SIGKILL to it via
// PR_SET_PDEATHSIG — not because it noticed stdin EOF (a mid-search engine
// that only polls stdin between iterations would not notice for a long
// time, and a non-compliant engine would never notice at all).
//
// Structure: fork a "harness" process that spawns a long-lived child via a
// real EngineProcess and then blocks in pause(). The spawned child is a
// tiny shell script that ignores stdin entirely (unlike /bin/cat) and just
// sleeps — so the only thing that can end it is PDEATHSIG (or an explicit
// kill), never EOF-on-stdin. The parent (this test) SIGKILLs the harness —
// simulating the GUI process dying outright, with no destructor chance —
// and polls /proc/<pid> for the engine child to disappear (or become a
// zombie, i.e. already terminated and just pending reaping by whatever it
// got reparented to).
//
// Linux-only, matching PR_SET_PDEATHSIG itself and this test target's
// existing Linux assumptions (giomm/Gio::Subprocess spawning).

#ifdef __linux__

#include "vendor/doctest.h"

#include "engine/engine_process.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

// True once the process is gone from the process table, or is a zombie
// (terminated, just not yet reaped by its new parent) — both mean
// PDEATHSIG did its job; reaping is a separate, unrelated race this test
// does not need to wait out.
bool processGoneOrZombie(pid_t pid)
{
    std::ifstream f("/proc/" + std::to_string(pid) + "/stat");
    if (!f.is_open()) return true; // /proc entry gone entirely.

    std::string line;
    std::getline(f, line);
    // Format: "<pid> (<comm>) <state> ...". comm can contain spaces/parens,
    // so search from the LAST ')' for the state field.
    auto paren = line.rfind(')');
    if (paren == std::string::npos || paren + 2 >= line.size()) return true;
    char state = line[paren + 2];
    return state == 'Z';
}

bool pollUntil(const std::function<bool()> &done, int timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

} // namespace

TEST_CASE("ENG-03: engine child dies via PDEATHSIG within ~1s of the GUI process being SIGKILLed")
{
    // A script that deliberately never reads stdin (unlike /bin/cat) so the
    // only way it ever ends is PDEATHSIG or an explicit kill — proving the
    // kernel-level mechanism, not EOF-on-stdin.
    std::string scriptPath = "/tmp/ranls_eng03_pdeathsig_victim_" + std::to_string(::getpid()) + ".sh";
    {
        std::ofstream script(scriptPath);
        REQUIRE(script.is_open());
        script << "#!/bin/sh\nexec sleep 30\n";
    }
    ::chmod(scriptPath.c_str(), 0755);

    int pipefd[2];
    REQUIRE(::pipe(pipefd) == 0);

    pid_t harnessPid = ::fork();
    REQUIRE(harnessPid >= 0);

    if (harnessPid == 0) {
        // ── Harness process (stands in for the GUI process) ────────────────
        ::close(pipefd[0]);

        EngineProcess engineProc;
        if (!engineProc.start(scriptPath)) {
            ::_exit(1);
        }
        std::string pidStr = engineProc.pid() + "\n";
        ssize_t written = ::write(pipefd[1], pidStr.data(), pidStr.size());
        (void)written;
        ::close(pipefd[1]);

        // Block until this harness is SIGKILLed by the test below — this
        // stands in for a GUI crash / `kill -9`, where no destructor ever
        // runs (EngineProcess's own destructor force-kill path is
        // deliberately never reached here; PDEATHSIG is the only thing
        // that can end the child now).
        for (;;) ::pause();
        ::_exit(0); // unreachable
    }

    // ── Test process (parent) ──────────────────────────────────────────────
    ::close(pipefd[1]);

    std::string pidLine;
    {
        char buf[64];
        ssize_t n = ::read(pipefd[0], buf, sizeof(buf) - 1);
        REQUIRE(n > 0);
        buf[n] = '\0';
        pidLine = buf;
    }
    ::close(pipefd[0]);

    pid_t enginePid = static_cast<pid_t>(std::atoi(pidLine.c_str()));
    REQUIRE(enginePid > 0);
    REQUIRE_FALSE(processGoneOrZombie(enginePid)); // sanity: it's actually running first.

    // Simulate the GUI process dying outright with zero warning — no
    // destructor, no signal handler, nothing.
    REQUIRE(::kill(harnessPid, SIGKILL) == 0);
    int status = 0;
    ::waitpid(harnessPid, &status, 0); // reap the harness itself.

    bool gone = pollUntil([&] { return processGoneOrZombie(enginePid); }, 1500);

    // Best-effort cleanup: don't leak a runaway `sleep 30` into the test
    // runner's process tree if the assertion below is about to fail.
    if (!gone) ::kill(enginePid, SIGKILL);

    ::remove(scriptPath.c_str());

    CHECK(gone);
}

#endif // __linux__
