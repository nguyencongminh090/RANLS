cmake_minimum_required(VERSION 3.18.2)
project(rapfi-gui
    VERSION     1.0.0
    LANGUAGES   CXX
    DESCRIPTION "Rapfi Professional Analysis GUI"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Find gtkmm-4.0 ──────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)

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

    # Engine
    src/engine/engine_process.cpp
    src/engine/engine_controller.cpp
    src/engine/gomocup_protocol.cpp

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
)

# ── Executable ───────────────────────────────────────────────────────────────
add_executable(rapfi-gui ${GUI_SOURCES})

target_include_directories(rapfi-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(rapfi-gui PRIVATE
    PkgConfig::GTKMM
)

target_compile_options(rapfi-gui PRIVATE
    -Wall -Wextra -Wpedantic
)

# ── Copy style.css alongside the binary during build ────────────────────────
add_custom_command(TARGET rapfi-gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/style.css"
        "${CMAKE_CURRENT_BINARY_DIR}/style.css"
    COMMENT "Copying style.css to build directory..."
)

# ── Install rules (used by package.sh via cmake --install) ──────────────────
install(TARGETS rapfi-gui RUNTIME DESTINATION .)
install(FILES src/resources/style.css DESTINATION .)

# ── Tests (model/engine layers only, no gtkmm/display server required) ──────
option(RAPFI_GUI_BUILD_TESTS "Build the rapfi-gui-tests target" ON)
if(RAPFI_GUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
