// PORT-03: portable stand-in "engine" for the test suite.
//
// Replaces the POSIX-only /bin/cat and /bin/true stand-ins that 7 test
// suites hardcoded, so the tests build and run under native MSVC / plain
// Windows as well as Linux and MSYS2 MINGW64. It carries NO protocol
// knowledge — it only has to reproduce the process-lifecycle behaviour the
// tests relied on:
//
//   default build (target `mock_engine`)  — a faithful `/bin/cat`: it echoes
//       each stdin line straight back to stdout, stays alive as long as its
//       stdin pipe is open, and exits 0 on EOF (pipe closed) or on the
//       sentinel line "END". The echo matters: the 7 suites were written
//       against `/bin/cat`, whose echo drives EngineProcess's stdout reader
//       (and hence the GLib main loop) on every line the GUI sends — several
//       assertions and the surrounding widget bookkeeping depend on that
//       cadence. The suites simulate *coordinate* engine replies separately
//       by emitting on EngineProcess::signal_line_received; `/bin/cat` (and
//       this) never echo a coordinate-shaped line, so nothing here interferes
//       with what they assert.
//
//   -DMOCK_ENGINE_QUIT build (target `mock_engine_quit`) — behaves like
//       `/bin/true`: exits 0 immediately without reading or writing
//       anything. Used by test_eng01 to exercise "engine dies right away".
//
// Kept deliberately tiny and dependency-free (no glib, no gtkmm) — it is a
// leaf test fixture, built as its own CMake executable target.

#include <iostream>
#include <string>

int main()
{
#ifdef MOCK_ENGINE_QUIT
    return 0;
#else
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "END")
            break;
        std::cout << line << '\n';
        std::cout.flush();
    }
    return 0;
#endif
}
