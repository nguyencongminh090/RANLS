# PORT-02's GResource was never compiled in — `project(LANGUAGES CXX)` silently drops the generated `.c`, so the app shipped with no stylesheet

**Status:** ✅ FIXED

Inline fix, no `TODO.md` CODE (follows the `port02-style-css-bundled` guard added by PORT-02).
Branch `fix/port02-gresource-not-bundled`.

## Prompt

`ctest` reported `port02-style-css-bundled` failing on a from-scratch build on the dev host —
flagged during the ANLZ-05 UI-test fix
(`docs/fix-log/2026-09-06-anlz05-uitest-trailing-coordinate-mock-engine.md`). PORT-02's own fix-log
had recorded the guard passing, so this was a regression *or* the guard had been a false positive.

## Investigation (`/systematic-debugging`)

**The guard was a false positive from day one, and the real feature never worked.**

1. **Bundling check.** `objdump -h build/ranls-gui` — **no `.gresource` section**. `strings` — none
   of `style.css`'s content (`.panel`, `.engine-status`, …) in the binary. The compiled resource
   blob is simply absent from every build, clean or incremental.

2. **Why the guard passed anyway.** `tests/port02_check_gresource.cmake` check #1 was
   `file(STRINGS BIN REGEX "org/ranls/style\.css")`. That string is *also* a plain literal in
   `application.cpp` (`load_from_resource("/org/ranls/style.css")`), so the check matched the call
   site, not bundled data. (It is in fact even weaker than that — GCC `.rodata` string-merging
   splits the literal so `strings` shows `/org/ranls/style` glued to the next symbol; whether check
   #1 passes depends on `-fmerge-constants`, which is why it flipped between PORT-02's host and this
   one.)

3. **Why the `.c` is never built.** `CMakeLists.txt` line 2: `project(ranls-gui LANGUAGES CXX)`.
   C is not an enabled language. `add_executable(ranls-gui … ${RANLS_GRESOURCE_C})` is handed
   `generated/ranls_gresource.c`, but with no C compiler configured CMake **silently treats the
   `.c` as a non-compiled source** — no error, no warning. `build.ninja` proves it: the
   `add_custom_command` that runs `glib-compile-resources` is present, but there is **no
   `ranls_gresource.c.o` build edge** and the file is **absent from the `ranls-gui` link line**.
   `CMakeCache.txt` has no `CMAKE_C_COMPILER`.

4. **Runtime effect.** `cssProvider->load_from_resource("/org/ranls/style.css")` throws
   `Glib::Error` (resource not registered) on every launch. PORT-02 wrapped that call in
   `try/catch` with a single warning line — so the failure is silent and **the app has run with
   zero custom styling since PORT-02 merged** (`328490e`, 2026-09-06).

## Fix

`CMakeLists.txt`:
- `project(… LANGUAGES CXX C)` — enables the C compiler so `ranls_gresource.c` compiles and links.
  Verified: `[Building C object …/ranls_gresource.c.o]`, `.gresource.ranls` section now in the
  binary, `style.css` content present.
- `set_source_files_properties(${RANLS_GRESOURCE_C} PROPERTIES COMPILE_OPTIONS …/-w)` — the
  generated file embeds the stylesheet as one >4095-char string literal, which `-Wpedantic` flags
  (`-Woverlength-strings`); silenced for that one generated TU only, both toolchains.

`src/resources/style.css`: a `/* @ranls-gresource-marker … */` sentinel comment (ignored by the
GTK CSS parser).

`tests/port02_check_gresource.cmake`: check #1 rewritten to grep the binary for
`@ranls-gresource-marker` — a string that exists **only** inside `style.css`, so it can reach the
binary only through the linked, compiled GResource blob. Check #2 (no build-host source path)
unchanged. Negative-tested: reverting the `LANGUAGES` change makes the guard fail as intended.

## Verification

- Clean `./build.sh` — 3 warnings, all the pre-existing `-Wunused-function` ones in
  `gomocup_protocol.cpp` (0 new; the `-Woverlength-strings` one is gone).
- `ctest` — **`port02-style-css-bundled` PASS** (was FAIL), `ranls-gui-tests` 209/209,
  `rel02-version-single-source` pass.
- `ranls-gui-ui-tests` — 28/28 excluding `test_ui12_move_log_scroll_target` (2 cases). UI-12 is the
  pre-existing GTK4 kinetic-scroll timing flake (`CHECK(value >= maxValue - 4.0)` at line 136) —
  fails 4/4 in isolation on the unchanged base commit `44fc877` too, load-dependent, and no UI test
  loads the stylesheet (`test_ui12` pulls in `ui/bottom_panel.h` only), so this change cannot affect
  it. PORT-03's fix-log claimed to have resolved this flake; it has not held on this host — noted
  for a separate look, not fixed here.
- Manual live launch (does the running app now pick up `style.css`) still needs a human — no display
  path exercised by the test suite.

## Related

- `docs/fix-log/2026-09-06-bundle-style-css-via-gresource.md` — PORT-02, the change whose feature
  this repairs and whose guard this hardens.
- `docs/audit/2026-09-06-native-msvc-windows-ci-goal.md` — the MSVC `if(MSVC)` compiler-flag branch;
  `LANGUAGES CXX C` applies there too (vcpkg provides `cl` for both).
