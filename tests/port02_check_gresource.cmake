# PORT-02 regression guard — see CMakeLists.txt (add_test port02-style-css-bundled).
#
# Asserts the built ranls-gui binary:
#   1. contains the GResource path "/org/ranls/style.css" (stylesheet is linked in), and
#   2. contains no absolute build-host source path (the old __FILE__ fallback baked one in).
#
# Invoked as: cmake -DBIN=<path-to-ranls-gui> -P port02_check_gresource.cmake

if(NOT DEFINED BIN)
    message(FATAL_ERROR "BIN not set")
endif()
if(NOT EXISTS "${BIN}")
    message(FATAL_ERROR "binary not found: ${BIN}")
endif()

file(STRINGS "${BIN}" _css_hits REGEX "org/ranls/style\\.css")
if(NOT _css_hits)
    message(FATAL_ERROR "GResource path 'org/ranls/style.css' not found in ${BIN} — style.css not bundled")
endif()

# Any absolute path from a typical build host baked into the binary is a regression.
file(STRINGS "${BIN}" _bad_hits REGEX "/(run/media|home)/[A-Za-z0-9_.-]+/.*/(src/resources|application\\.cpp)")
if(_bad_hits)
    message(FATAL_ERROR "build-host source path baked into ${BIN}:\n${_bad_hits}")
endif()

message(STATUS "PORT-02 guard OK: style.css bundled, no build-host path in binary")
