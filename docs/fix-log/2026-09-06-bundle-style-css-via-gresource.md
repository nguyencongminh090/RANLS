# 2026-09-06 — PORT-02: Bundle style.css into the binary via GResource

## Prompt

Implement tracked task PORT-02 (`docs/todo/PORT-02-bundle-style-css-via-gresource.md`), the Tier-2
remediation from the 2026-09-06 platform-dependency audit: stop loading `style.css` from the launch
directory with an `__FILE__`-derived source-tree fallback (which bakes the build host's absolute
path into the binary and loses all styling on any non-CWD launch), and instead compile the
stylesheet into the executable as a GResource loaded via
`Gtk::CssProvider::load_from_resource("/org/ranls/style.css")`.

## Action

### 1. `src/resources/ranls.gresource.xml` (new)

`<gresource prefix="/org/ranls/">` listing the single file `style.css`. `style.css` stays in the
tree as the source of truth (not moved, not restructured, contents unchanged).

### 2. `CMakeLists.txt`

- `pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)` + `find_program(GLIB_COMPILE_RESOURCES
  glib-compile-resources)` (falls back to `pkg_get_variable(... glib_compile_resources)`, hard error
  if still not found).
- `add_custom_command(OUTPUT build/generated/ranls_gresource.c ...)` runs `glib-compile-resources
  --generate-source --target=<build>/generated/ranls_gresource.c
  --sourcedir=src/resources src/resources/ranls.gresource.xml`, with `DEPENDS` on both the XML and
  `style.css` so editing either regenerates. The generated `.c` lives only in the build dir.
- `ranls_gresource.c` added to the `add_executable(ranls-gui ...)` sources; `PkgConfig::GIO` added
  to `target_link_libraries`.
- Removed the `add_custom_command(TARGET ranls-gui POST_BUILD ...)` that copied `style.css` into
  the build dir, and the `install(FILES src/resources/style.css DESTINATION .)` line. The
  `install(TARGETS ranls-gui ...)` rule is unchanged.

### 3. `src/application.cpp`

`loadStylesheet()` body replaced: `cssProvider->load_from_resource("/org/ranls/style.css")` inside
a `try` / `catch (const Glib::Error &e)` that logs one `[RANLS] Warning: failed to load bundled
style.css: ...` line. Removed `#include <filesystem>`, the `load_from_path` call, the
`std::filesystem::exists` probes and the `std::filesystem::path(__FILE__).parent_path()` fallback.

### 4. Regression guard

This area has no unit-test infrastructure — `ctest` covers the model/engine layers only, with no
gtkmm/display-server harness for `application.cpp`'s CSS loading. Added a binary-inspection CTest
instead: `port02-style-css-bundled` runs `tests/port02_check_gresource.cmake`, which asserts the
built `ranls-gui` (a) contains the string `org/ranls/style.css` (stylesheet linked in) and (b)
contains no absolute build-host source path matching `/(run/media|home)/…/(src/resources|
application\.cpp)`. Kept as a permanent regression guard.

## Verification (Linux host, 2026-09-06)

- `rm -rf build && cmake -S . -B build` — configure rc 0 (`Found giomm-2.68`, GResource step wired).
- `cmake --build build -j2` — rc 0; `[  0%] Compiling GResource bundle (style.css)`; no new
  warnings.
- `strings build/ranls-gui | grep -c "org/ranls/style.css"` → `1`.
- `strings build/ranls-gui | grep "/run/media"` → empty (grep rc 1).
- `ls build/generated/ranls_gresource.c` → present, 25556 bytes, linked into `ranls-gui`.
- `ctest --test-dir build` → 3/4 suites pass: `port02-style-css-bundled` **Passed**,
  `ranls-gui-tests` Passed, `rel02-version-single-source` Passed; `ranls-gui-ui-tests` fails on
  exactly one case, `test_anlz05_no_automove_action` (`test_anlz05_no_automove_action.cpp:135`,
  engine-subprocess `BEGIN` timing flake), which fails identically on baseline `main` (72f0fd3) —
  pre-existing, not a regression. doctest: 28/29 cases, 194/195 assertions.
- GUI launch-from-arbitrary-directory check: not run — no display server on this host. Verified
  structurally instead (resource string present + generated `.c` linked + no CWD-relative or
  `__FILE__` path left in the code).

## Summary

1 new resource XML + 1 new CMake test script; `CMakeLists.txt` and `src/application.cpp` modified;
`src/resources/style.css` left in place unchanged. The stylesheet is now compiled into the binary,
so styling no longer depends on the launch directory and no build-host path is baked into the
executable. Scope boundary respected: `style.css` only, no other assets touched, no `.desktop` /
icon resources, no change to the stylesheet contents. Remaining human step: launch the packaged
binary from an unrelated directory on a machine with a display and confirm custom styling renders
with no `style.css` warning.
