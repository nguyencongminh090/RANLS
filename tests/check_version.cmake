# REL-02 regression check: the built `ranls-gui --version` output must equal
# the CMake PROJECT_VERSION (the single source of truth). Runs the real binary
# headless — links no gtkmm itself — and is wired into ctest from tests/CMakeLists.txt.
#
# Invoked as: cmake -DRANLS_GUI_BIN=<path> -DEXPECTED_VERSION=<x> -P check_version.cmake

if(NOT DEFINED RANLS_GUI_BIN OR NOT DEFINED EXPECTED_VERSION)
    message(FATAL_ERROR "RANLS_GUI_BIN and EXPECTED_VERSION must be provided")
endif()

foreach(flag "--version" "-v")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY
                "${RANLS_GUI_BIN}" ${flag}
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        RESULT_VARIABLE rc
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "`ranls-gui ${flag}` exited ${rc} (stderr: ${err})")
    endif()
    if(NOT out STREQUAL EXPECTED_VERSION)
        message(FATAL_ERROR "`ranls-gui ${flag}` printed '${out}', expected '${EXPECTED_VERSION}'")
    endif()
endforeach()

message(STATUS "ranls-gui --version / -v == ${EXPECTED_VERSION} (matches CMake PROJECT_VERSION)")
