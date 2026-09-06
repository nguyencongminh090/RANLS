// Regression tests for PORT-01: the Windows executable-extension check that
// replaces the POSIX `access(path, X_OK)` execute-bit test in
// settings_dialog.cpp's isValidEnginePath(). The helper is pure string logic
// (std::filesystem::path::extension + case-fold), so it compiles and behaves
// identically on every platform and is exercised here on the Linux CI host even
// though the _WIN32 branch that calls it is not compiled.
//
// Pins: the four accepted extensions, case-insensitivity, and rejection of
// everything else (no extension, engine binaries with a POSIX-style name,
// look-alike suffixes, trailing dot, directory-like paths).

#include "vendor/doctest.h"

#include "ui/settings_dialog.h"

using settings_dialog_detail::hasExecutableExtension;

TEST_CASE("PORT-01: accepted Windows executable extensions") {
    CHECK(hasExecutableExtension("engine.exe"));
    CHECK(hasExecutableExtension("engine.bat"));
    CHECK(hasExecutableExtension("engine.cmd"));
    CHECK(hasExecutableExtension("engine.com"));
    CHECK(hasExecutableExtension(R"(C:\Engines\rapfi\pbrain-rapfi.exe)"));
    CHECK(hasExecutableExtension("/opt/engines/yixin.exe"));
}

TEST_CASE("PORT-01: extension match is case-insensitive") {
    CHECK(hasExecutableExtension("Engine.EXE"));
    CHECK(hasExecutableExtension("Engine.Exe"));
    CHECK(hasExecutableExtension("engine.BaT"));
    CHECK(hasExecutableExtension("engine.CMD"));
}

TEST_CASE("PORT-01: non-executable and look-alike paths are rejected") {
    CHECK_FALSE(hasExecutableExtension(""));
    CHECK_FALSE(hasExecutableExtension("pbrain-rapfi"));        // POSIX-style, no ext
    CHECK_FALSE(hasExecutableExtension("/usr/local/bin/rapfi")); // POSIX-style, no ext
    CHECK_FALSE(hasExecutableExtension("engine.exe.txt"));
    CHECK_FALSE(hasExecutableExtension("engine.executable"));
    CHECK_FALSE(hasExecutableExtension("engine.command"));
    CHECK_FALSE(hasExecutableExtension("readme.md"));
    CHECK_FALSE(hasExecutableExtension("engine.dll"));
    CHECK_FALSE(hasExecutableExtension("engine."));             // trailing dot, empty ext
    CHECK_FALSE(hasExecutableExtension("engine.sh"));
}
