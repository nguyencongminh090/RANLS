# PORT-02 regression guard — see CMakeLists.txt (add_test port02-style-css-bundled).
#
# Asserts the built ranls-gui binary:
#   1. contains the compiled GResource blob for style.css — checked via the
#      "@ranls-gresource-marker" sentinel comment that only exists inside
#      src/resources/style.css, so it can only reach the binary through the
#      linked glib-compile-resources output. (The earlier check for the
#      "/org/ranls/style.css" path string was unreliable: that literal also
#      lives in application.cpp, and GCC string-merging can split it so it is
#      not found even when bundling works — it proved nothing either way.)
#   2. contains no absolute build-host source path (the old __FILE__ fallback
#      in application.cpp baked one in).
#
# Invoked as: cmake -DBIN=<path-to-ranls-gui> -P port02_check_gresource.cmake

if(NOT DEFINED BIN)
    message(FATAL_ERROR "BIN not set")
endif()
if(NOT EXISTS "${BIN}")
    message(FATAL_ERROR "binary not found: ${BIN}")
endif()

file(STRINGS "${BIN}" _marker_hits REGEX "@ranls-gresource-marker")
if(NOT _marker_hits)
    message(FATAL_ERROR
        "style.css GResource blob not found in ${BIN} — the stylesheet is not "
        "bundled (is 'C' in project(LANGUAGES) so generated/ranls_gresource.c "
        "actually compiles and links?)")
endif()

# Any absolute path from a typical build host baked into the binary is a regression.
file(STRINGS "${BIN}" _bad_hits REGEX "/(run/media|home)/[A-Za-z0-9_.-]+/.*/(src/resources|application\\.cpp)")
if(_bad_hits)
    message(FATAL_ERROR "build-host source path baked into ${BIN}:\n${_bad_hits}")
endif()

message(STATUS "PORT-02 guard OK: style.css GResource blob bundled, no build-host path in binary")
