# ── Test target: model + engine layers only, no gtkmm ───────────────────────
#
# Deliberately does NOT link PkgConfig::GTKMM or include any src/ui, src/command,
# src/application.*, or src/main_window.* sources. This is enforced by construction
# (the source list below), not by a build flag — if a future change to src/model or
# src/engine starts pulling in gtkmm headers transitively, this target will fail to
# compile and that is the intended signal (see docs/audit/2026-08-21-test-framework-choice.md).

find_package(PkgConfig REQUIRED)
pkg_check_modules(SIGCXX REQUIRED IMPORTED_TARGET sigc++-3.0)

add_executable(rapfi-gui-tests
    test_main.cpp
    test_move_history.cpp
    test_gomocup_protocol.cpp
    test_game_state.cpp
    test_rt01_throttle.cpp
    test_engine_log_model.cpp
    test_settings_storage.cpp

    # Sources under test (model/engine only — no UI, no gtkmm).
    ${CMAKE_SOURCE_DIR}/src/model/move_history.cpp
    ${CMAKE_SOURCE_DIR}/src/model/board_state.cpp
    ${CMAKE_SOURCE_DIR}/src/model/game_state.cpp
    ${CMAKE_SOURCE_DIR}/src/model/variation_tree.cpp
    ${CMAKE_SOURCE_DIR}/src/model/settings_storage.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/gomocup_protocol.cpp
)

target_include_directories(rapfi-gui-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rapfi-gui-tests PRIVATE
    PkgConfig::SIGCXX
)

target_compile_features(rapfi-gui-tests PRIVATE cxx_std_20)

include(CTest)

# doctest ships a CMake helper (doctest_discover_tests) that registers one
# ctest case per TEST_CASE; we don't vendor that helper, so register the
# whole binary as a single ctest case instead — simpler and sufficient for
# this harness's size.
add_test(NAME rapfi-gui-tests COMMAND rapfi-gui-tests)
