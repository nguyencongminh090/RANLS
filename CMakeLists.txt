cmake_minimum_required(VERSION 3.18.2)
project(ranls-gui
    VERSION     0.4.2
    # C is needed to compile the glib-compile-resources output
    # (generated/ranls_gresource.c, PORT-02); with CXX only, add_executable
    # silently drops the .c and the stylesheet is never bundled.
    LANGUAGES   CXX C
    DESCRIPTION "RANLS — Renju/Gomoku Analysis GUI"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Find gtkmm-4.0 + gio-2.0 ────────────────────────────────────────────────
#
# PORT-03: the GCC/Clang + pkg-config path (Linux, MSYS2 MINGW64) is the
# default and unchanged. Native MSVC has no pkg-config on PATH, so under
# MSVC we use CMake's own package discovery instead. With vcpkg this means
# configuring with
#   -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
# after `vcpkg install gtkmm zlib` — vcpkg ships gtkmm4/glibmm CMake configs
# (unofficial-* package names) and a `PkgConfig` shim, so pkg_check_modules
# also works there. See README "Building on Windows".
if(MSVC)
    # vcpkg exposes gtkmm via a pkg-config shim; fall back to it if present,
    # otherwise expect the caller's toolchain to provide gtkmm-4.0.
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)
        pkg_check_modules(GIO   REQUIRED IMPORTED_TARGET gio-2.0)
    else()
        message(FATAL_ERROR
            "MSVC build needs gtkmm-4.0 via a vcpkg toolchain file "
            "(pkg-config shim) — see README 'Building on Windows'.")
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)
    # ── gio-2.0 + glib-compile-resources (PORT-02: bundle style.css) ─────────
    pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
endif()

# ── zlib (RDB-01: .rdb container crc32 + deflate payload codec) ──────────────
find_package(ZLIB REQUIRED)

# ── glib-compile-resources (PORT-02: bundle style.css as GResource) ─────────
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources
    HINTS $ENV{HOME}/.local/bin
)
if(NOT GLIB_COMPILE_RESOURCES)
    pkg_get_variable(GLIB_COMPILE_RESOURCES gio-2.0 glib_compile_resources)
endif()
if(NOT GLIB_COMPILE_RESOURCES)
    message(FATAL_ERROR "glib-compile-resources not found (needed to bundle style.css)")
endif()

set(RANLS_GRESOURCE_XML ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ranls.gresource.xml)
set(RANLS_GRESOURCE_C    ${CMAKE_CURRENT_BINARY_DIR}/generated/ranls_gresource.c)
add_custom_command(
    OUTPUT  ${RANLS_GRESOURCE_C}
    COMMAND ${GLIB_COMPILE_RESOURCES}
            --target=${RANLS_GRESOURCE_C}
            --sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/src/resources
            --generate-source
            ${RANLS_GRESOURCE_XML}
    DEPENDS ${RANLS_GRESOURCE_XML}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/style.css
    COMMENT "Compiling GResource bundle (style.css)"
    VERBATIM
)

# ── Source files ─────────────────────────────────────────────────────────────
set(GUI_SOURCES
    # Entry point
    src/main.cpp
    src/application.cpp
    src/main_window.cpp

    # Data model
    src/model/board_state.cpp
    src/model/game_state.cpp
    src/model/move_history.cpp
    src/model/variation_tree.cpp
    src/model/board_view_model.cpp
    src/model/renju_rule.cpp
    src/model/settings_storage.cpp
    src/model/game_io.cpp

    # RDB-01: .rdb binary save format — container, codec, DTO, CBOR, convert
    src/model/rdb/compressor.cpp
    src/model/rdb/rdb_container.cpp
    src/model/rdb/game_graph_cbor.cpp
    src/model/rdb/game_graph_convert.cpp
    src/model/rdb/game_archive.cpp

    # Engine
    src/engine/engine_process.cpp
    src/engine/engine_controller.cpp
    src/engine/gomocup_protocol.cpp
    src/engine/protocol_extension.cpp

    # Command console (in-app CLI)
    src/command/command.cpp
    src/command/command_dispatcher.cpp

    # UI components
    src/ui/board_view.cpp
    src/ui/board_renderer.cpp
    src/ui/analysis_panel.cpp
    src/ui/engine_status.cpp
    src/ui/empty_state.cpp
    src/ui/win_graph_view.cpp
    src/ui/pv_view.cpp
    src/ui/tree_explorer.cpp
    src/ui/tree_node_view.cpp
    src/ui/bottom_panel.cpp
    src/ui/settings_dialog.cpp
    src/ui/about_dialog.cpp
)

# ── Generated version header (single source of truth: project(VERSION) above) ─
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
    @ONLY
)

# ── UI-11: generated build-info header (build date + git commit) ────────────
# Values resolved at configure time. `git rev-parse` is guarded so a no-.git
# tarball build still configures — it falls back to "unknown".
string(TIMESTAMP APP_BUILD_DATE "%Y-%m-%d" UTC)
execute_process(
    COMMAND           git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE   APP_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE   _git_commit_rc
)
if(NOT _git_commit_rc EQUAL 0 OR "${APP_GIT_COMMIT}" STREQUAL "")
    set(APP_GIT_COMMIT "unknown")
endif()
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/build_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.h
    @ONLY
)

# ── Executable ───────────────────────────────────────────────────────────────
# PORT-03: WIN32 selects the Windows GUI subsystem (no cmd.exe console window
# on launch). It is silently ignored on non-Windows platforms.
add_executable(ranls-gui WIN32 ${GUI_SOURCES} ${RANLS_GRESOURCE_C})

target_include_directories(ranls-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(ranls-gui PRIVATE
    PkgConfig::GTKMM
    PkgConfig::GIO
    ZLIB::ZLIB
)

# PORT-03: MSVC rejects the GCC/Clang -W* flags; use its own warning level.
if(MSVC)
    target_compile_options(ranls-gui PRIVATE /W4 /permissive-)
else()
    target_compile_options(ranls-gui PRIVATE -Wall -Wextra -Wpedantic)
endif()

# PORT-02: the glib-compile-resources output embeds the stylesheet as one long
# C string literal — ISO C99 only mandates 4095 chars, so -Wpedantic warns.
# It is generated code; silence warnings for that one translation unit only.
set_source_files_properties(${RANLS_GRESOURCE_C} PROPERTIES
    COMPILE_OPTIONS "$<IF:$<BOOL:${MSVC}>,/w,-w>")

# ── Install rules (used by package.sh via cmake --install) ──────────────────
# style.css is bundled into the binary as a GResource (PORT-02) — no sidecar
# file to copy at build time or install alongside the binary.
install(TARGETS ranls-gui RUNTIME DESTINATION .)

# ── Tests (model/engine layers only, no gtkmm/display server required) ──────
option(RANLS_GUI_BUILD_TESTS "Build the ranls-gui-tests target" ON)
if(RANLS_GUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)

    # ── PORT-02: guard the GResource bundling of style.css ──────────────────
    # No gtkmm/display test infra here, so this is a binary-inspection guard:
    # the CSS resource path must be linked in, and no build-host absolute path
    # may be baked into the executable.
    add_test(NAME port02-style-css-bundled
        COMMAND ${CMAKE_COMMAND}
            -DBIN=$<TARGET_FILE:ranls-gui>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/port02_check_gresource.cmake
    )
endif()
